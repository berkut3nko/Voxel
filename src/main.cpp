#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <vector>
#include <iostream>
#include <ctime>
#include <cmath>
#include <chrono> 
#include <string>
#include <iomanip>
#include <sstream>
#include <algorithm> 
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <optional>

import VoxelGame.Types;
import VoxelGame.Math;
import VoxelGame.GL;
import VoxelGame.Shader;
import VoxelGame.World;
import VoxelGame.Meshing;
import VoxelGame.Utils;
import VoxelGame.RenderUtils;

using namespace VoxelGame::Types;
using namespace VoxelGame::Math;
using namespace VoxelGame::World;
using namespace VoxelGame::Meshing;
using namespace VoxelGame::Shader;
using namespace VoxelGame::Utils;
using namespace VoxelGame::RenderUtils;

namespace GL = VoxelGame::GL;

// --- Config ---
const float FOV_DEG = 100.0f;         
const float YAW_PADDING = 10.0f;      
const int HORIZON_BUCKETS = 256; 

// --- Multithreading Data Structures ---
struct MeshJob {
    int cx, cz;
    int lod;
    int maxHeight; // Need for culling, calculated during meshing usually, or pre-calc
};

struct MeshResult {
    int cx, cz;
    int lod;
    int maxHeight;
    std::vector<GpuQuad> gpuQuads;
    bool valid;
};

// Thread-safe Queue
template<typename T>
class SafeQueue {
    std::queue<T> q;
    std::mutex m;
public:
    void push(T val) {
        std::lock_guard<std::mutex> lk(m);
        q.push(std::move(val));
    }
    bool try_pop(T& val) {
        std::lock_guard<std::mutex> lk(m);
        if(q.empty()) return false;
        val = std::move(q.front());
        q.pop();
        return true;
    }
    bool empty() {
        std::lock_guard<std::mutex> lk(m);
        return q.empty();
    }
    void clear() {
        std::lock_guard<std::mutex> lk(m);
        std::queue<T> empty;
        std::swap(q, empty);
    }
};

SafeQueue<MeshJob> jobQueue;
SafeQueue<MeshResult> resultQueue;
std::atomic<bool> stopWorkers{false};

void MeshWorker(WorldManager* world) {
    while(!stopWorkers) {
        MeshJob job;
        if(jobQueue.try_pop(job)) {
            // Read Lock is essential for thread safety while reading voxels
            // Note: shared_lock is held implicitly by Meshing logic if we updated World.cppm 
            // OR we explicitly hold it here if we access the map structure.
            // Since getChunk in World.cppm now uses shared_lock internally for map access,
            // we are safe to get pointers. BUT, the vector inside chunk is not protected 
            // from modification if another thread writes to it. 
            // Assuming static world for now after gen.
            
            // To be 100% safe against re-allocation of vectors, we grab pointers.
            // Using the thread-safe getChunk:
            VoxelChunk* center = world->getChunk(job.cx, job.cz);
            
            if (!center) {
                resultQueue.push({job.cx, job.cz, job.lod, 0, {}, false});
                continue;
            }

            MeshingContext ctx = {};
            ctx.center = center;
            ctx.neighbors[0] = world->getChunk(job.cx + 1, job.cz);
            ctx.neighbors[1] = world->getChunk(job.cx - 1, job.cz);
            ctx.neighbors[4] = world->getChunk(job.cx, job.cz + 1);
            ctx.neighbors[5] = world->getChunk(job.cx, job.cz - 1);
            ctx.neighbors[2] = world->getChunk(job.cx, job.cz); // Top placeholder
            ctx.neighbors[3] = world->getChunk(job.cx, job.cz); // Bottom placeholder

            // Calculate Max Height (for culling)
            int maxH = 0;
            // Simple sampling for MaxH to avoid full scan cost if not needed, 
            // but for correct culling we need it. 
            // Let's do a quick scan.
            for (int lx = 0; lx < CHUNK_SIZE; lx += 4) { 
                for (int lz = 0; lz < CHUNK_SIZE; lz += 4) {
                    for (int ly = CHUNK_SIZE - 1; ly >= 0; --ly) {
                        if (!IsTransparent(center->get(lx, ly, lz))) {
                            if (ly > maxH) maxH = ly;
                            break;
                        }
                    }
                }
            }
            maxH += 2; if (maxH > CHUNK_SIZE) maxH = CHUNK_SIZE;

            std::vector<Quad> quads = GenerateQuads(ctx, job.lod);
            std::vector<GpuQuad> gpuQ = BuildSSBOData(quads);
            
            resultQueue.push({job.cx, job.cz, job.lod, maxH, std::move(gpuQ), true});
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }
}

struct ChunkRenderDataExtended {
    std::vector<GpuQuad> gpuQuads;
    int chunkX, chunkZ;
    unsigned int globalBufferOffset; 
    int maxHeight; 
    int currentLod; 
    bool isMeshDirty; // Flag to indicate pending update
};

// Нормалізація кута [-PI, PI]
float WrapAngle(float angle) {
    while (angle > 3.14159265f) angle -= 6.2831853f;
    while (angle < -3.14159265f) angle += 6.2831853f;
    return angle;
}

int main(int argc, char* argv[]) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL_Init failed logic check" << std::endl;
    }
    
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_Window* window = SDL_CreateWindow("Voxel Game - Optimized Multithreading", 1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) return -1;

    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (!context) return -1;
    
    SDL_GL_MakeCurrent(window, context);
    SDL_GL_SetSwapInterval(0); 
    
    GL::LoadFunctions(); 
    
    bool mouseCaptured = true;
    SDL_SetWindowRelativeMouseMode(window, true);

    WorldManager world;
    int seed = std::time(nullptr) % 1000;
    
    int range = 12; 
    size_t estChunks = (range * 2 + 1) * (range * 2 + 1);
    
    std::cout << "Generating chunks (Range: " << range << ")..." << std::endl;
    
    // Initial Generation (Sync for simplicity or could be threaded too)
    for (int cx = -range; cx <= range; ++cx) {
        for (int cz = -range; cz <= range; ++cz) {
            TerrainSystem::Generate(world.createChunk(cx, cz), seed);
        }
    }

    std::cout << "Starting Thread Pool..." << std::endl;
    unsigned int threadCount = std::thread::hardware_concurrency() - 1;
    if(threadCount < 1) threadCount = 1;
    
    std::vector<std::jthread> workers;
    for(unsigned int i=0; i<threadCount; ++i) {
        workers.emplace_back(MeshWorker, &world);
    }

    std::vector<ChunkRenderDataExtended> renderChunks;
    std::vector<GpuQuad> allGpuQuads;

    // Map to quickly find render data by coordinates
    // Using simple linear search for now as N is small (~600 chunks), 
    // but for larger worlds use std::map or hash map.
    auto findChunk = [&](int cx, int cz) -> ChunkRenderDataExtended* {
        for(auto& rc : renderChunks) {
            if(rc.chunkX == cx && rc.chunkZ == cz) return &rc;
        }
        return nullptr;
    };

    // Initial Mesh Push
    for (int cx = -range; cx <= range; ++cx) {
        for (int cz = -range; cz <= range; ++cz) {
            float dist = std::sqrt(float(cx * cx + cz * cz)) * CHUNK_SIZE;
            int lod = GetChunkLOD(dist);
            
            ChunkRenderDataExtended crd;
            crd.chunkX = cx;
            crd.chunkZ = cz;
            crd.currentLod = -1; // Force update
            crd.isMeshDirty = true;
            crd.maxHeight = CHUNK_SIZE;
            crd.globalBufferOffset = 0;
            
            renderChunks.push_back(std::move(crd));
            
            jobQueue.push({cx, cz, lod, 0});
        }
    }

    // GL Buffers initialization
    GLuint emptyVAO;
    GL::glGenVertexArrays(1, &emptyVAO);
    GL::glBindVertexArray(emptyVAO);

    GLuint indices[] = {0, 1, 2, 0, 2, 3};
    GLuint ibo;
    GL::glGenBuffers(1, &ibo);
    GL::glBindBuffer(0x8893, ibo); 
    GL::glBufferData(0x8893, sizeof(indices), indices, GL_STATIC_DRAW);
    GL::glBindVertexArray(0);

    GLuint ssboQuads, ssboChunks, indirectBuffer;
    GL::glGenBuffers(1, &ssboQuads);
    GL::glBindBuffer(GL::SHADER_STORAGE_BUFFER, ssboQuads);
    // Allocate huge buffer initially
    GL::glBufferData(GL::SHADER_STORAGE_BUFFER, estChunks * 2000 * sizeof(GpuQuad), nullptr, GL_DYNAMIC_DRAW); 
    GL::glBindBufferBase(GL::SHADER_STORAGE_BUFFER, 0, ssboQuads);

    GL::glGenBuffers(1, &ssboChunks);
    GL::glBindBuffer(GL::SHADER_STORAGE_BUFFER, ssboChunks);
    GL::glBufferData(GL::SHADER_STORAGE_BUFFER, estChunks * sizeof(GpuChunkInfo), nullptr, GL_DYNAMIC_DRAW);
    GL::glBindBufferBase(GL::SHADER_STORAGE_BUFFER, 1, ssboChunks);

    GL::glGenBuffers(1, &indirectBuffer);
    GL::glBindBuffer(GL::DRAW_INDIRECT_BUFFER, indirectBuffer);
    GL::glBufferData(GL::DRAW_INDIRECT_BUFFER, estChunks * sizeof(GL::DrawElementsIndirectCommand), nullptr, GL_DYNAMIC_DRAW);

    ShaderProgram shader = CreateProgram("src/shaders/voxel.vert.glsl", "src/shaders/voxel.frag.glsl");
    if (shader.id == 0) return -1;
    
    GL::glUseProgram(shader.id);
    GLuint texArrayID = CreatePaletteTextureArray(); 
    GL::glActiveTexture(GL_TEXTURE0);
    GL::glBindTexture(GL_TEXTURE_2D_ARRAY, texArrayID);
    GL::glUniform1i(shader.loc_textureArray, 0);

    float camX = 0.0f, camY = 40.0f, camZ = 0.0f;
    float yaw = -90.0f, pitch = -30.0f;
    bool showGrid = false;

    Profiler frameTimer;
    double acc=0;
    int frames=0;
    int drawnChunks = 0; 
    int culledChunks = 0;
    int horizonCulled = 0;
    unsigned int frameCounter = 0;

    std::vector<GL::DrawElementsIndirectCommand> visibleCommands;
    std::vector<GpuChunkInfo> visibleChunkInfos;
    
    struct ChunkDist { size_t index; float distSq; };
    std::vector<ChunkDist> visibleIndices;
    visibleIndices.reserve(estChunks);

    std::vector<float> horizonBuffer(HORIZON_BUCKETS);
    
    // Buffer Management
    std::vector<MeshResult> pendingUpdates; // Зберігає результати, поки ми не готові оновити GPU

    bool running = true;
    while(running) {
        frameTimer.begin();
        SDL_Event ev;
        while(SDL_PollEvent(&ev)) {
             if(ev.type == SDL_EVENT_QUIT) running = false;
             if(ev.type == SDL_EVENT_KEY_DOWN) {
                 if (ev.key.key == SDLK_ESCAPE) { mouseCaptured = false; SDL_SetWindowRelativeMouseMode(window, false); }
                 if (ev.key.key == SDLK_G) showGrid = !showGrid;
             }
             if(ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN) { mouseCaptured = true; SDL_SetWindowRelativeMouseMode(window, true); }
             if(ev.type == SDL_EVENT_MOUSE_MOTION && mouseCaptured) {
                 yaw += ev.motion.xrel * 0.1f; pitch -= ev.motion.yrel * 0.1f;
                 if(pitch > 89.0f) pitch = 89.0f; if(pitch < -89.0f) pitch = -89.0f;
             }
        }

        // --- Process Async Mesh Results (STORE ONLY) ---
        MeshResult res;
        int updatesProcessed = 0;
        // Збираємо результати в буфер, не застосовуючи їх одразу
        while(updatesProcessed < 50 && resultQueue.try_pop(res)) {
            if (res.valid) {
                pendingUpdates.push_back(std::move(res));
            }
            updatesProcessed++;
        }

        // Оновлюємо GPU буфер тільки раз на N кадрів або якщо накопичилось багато змін
        bool shouldRebuild = !pendingUpdates.empty() && (frameCounter % 5 == 0 || pendingUpdates.size() > 100);

        if (shouldRebuild) { 
            // 1. Застосовуємо зміни до локальних даних (RenderChunks)
            // Тепер ми перемикаємося зі старого LOD на новий атомарно для рендеру
            for (auto& update : pendingUpdates) {
                ChunkRenderDataExtended* rc = findChunk(update.cx, update.cz);
                if (rc) {
                    rc->gpuQuads = std::move(update.gpuQuads);
                    rc->maxHeight = update.maxHeight;
                    rc->currentLod = update.lod;
                    rc->isMeshDirty = false; 
                }
            }
            pendingUpdates.clear();

            // 2. Перебудовуємо глобальний буфер (тільки коли застосували оновлення)
            allGpuQuads.clear();
            // Estimate size
            size_t totalQuads = 0;
            for(const auto& rc : renderChunks) totalQuads += rc.gpuQuads.size();
            allGpuQuads.reserve(totalQuads);

            for(auto& rc : renderChunks) {
                rc.globalBufferOffset = (unsigned int)allGpuQuads.size();
                allGpuQuads.insert(allGpuQuads.end(), rc.gpuQuads.begin(), rc.gpuQuads.end());
            }

            // 3. Завантажуємо на GPU
            GL::glBindBuffer(GL::SHADER_STORAGE_BUFFER, ssboQuads);
            GL::glBufferData(GL::SHADER_STORAGE_BUFFER, allGpuQuads.size() * sizeof(GpuQuad), allGpuQuads.data(), GL_DYNAMIC_DRAW);
        }


        float radYaw = yaw * 0.0174533f; 
        float radPitch = pitch * 0.0174533f;
        Vec3 front = Normalize({ std::cos(radYaw)*std::cos(radPitch), std::sin(radPitch), std::sin(radYaw)*std::cos(radPitch) });
        Vec3 right = Normalize(Cross(front, {0,1,0}));
        
        const bool* keys = SDL_GetKeyboardState(nullptr);
        float speed = 0.5f; if(keys[SDL_SCANCODE_LSHIFT]) speed = 1.5f;
        if(keys[SDL_SCANCODE_W]) { camX += front.x*speed; camY += front.y*speed; camZ += front.z*speed; }
        if(keys[SDL_SCANCODE_S]) { camX -= front.x*speed; camY -= front.y*speed; camZ -= front.z*speed; }
        if(keys[SDL_SCANCODE_A]) { camX -= right.x*speed; camY -= right.y*speed; camZ -= right.z*speed; }
        if(keys[SDL_SCANCODE_D]) { camX += right.x*speed; camY += right.y*speed; camZ += right.z*speed; }

        int w, h; SDL_GetWindowSizeInPixels(window, &w, &h);
        float aspect = (float)w / (float)h;
        Mat4 view = LookAt({camX, camY, camZ}, {camX+front.x, camY+front.y, camZ+front.z}, {0,1,0});
        Mat4 proj = Perspective(1.047f, aspect, 0.1f, 1000.0f); 
        Mat4 vp = MultiplyMat4(proj, view);
        Frustum frustum = CreateFrustum(vp);
        
        // --- LOD REGENERATION REQUESTS ---
        if (frameCounter % 10 == 0) {
            float altitudeThreshold = 100.0f; 
            bool highAltitude = (camY > altitudeThreshold);

            for(auto& rc : renderChunks) {
                // Don't spam jobs if one is already pending
                if (rc.isMeshDirty) continue;

                float dx = (float)rc.chunkX * CHUNK_SIZE + 16.0f - camX;
                float dz = (float)rc.chunkZ * CHUNK_SIZE + 16.0f - camZ;
                float dist = std::sqrt(dx*dx + dz*dz);
                
                int neededLod = GetChunkLOD(dist);
                if (highAltitude && neededLod == 1) neededLod = 2;
                if (!highAltitude && neededLod == 2 && dist < 96.0f) neededLod = 1;

                if (rc.currentLod != neededLod) {
                    rc.isMeshDirty = true;
                    jobQueue.push({rc.chunkX, rc.chunkZ, neededLod, 0});
                }
            }
        }

        // --- CULLING PASS ---
        visibleIndices.clear();
        culledChunks = 0;
        horizonCulled = 0;

        float camDirX = std::cos(radYaw);
        float camDirZ = std::sin(radYaw);
        
        float pitchFactor = std::abs(std::sin(radPitch)); 
        float backwardShift = pitchFactor * (range * CHUNK_SIZE * 0.8f); 
        float checkX = camX - camDirX * backwardShift;
        float checkZ = camZ - camDirZ * backwardShift;

        float fovRad = (FOV_DEG + YAW_PADDING) * 0.0174533f;
        float minCosYaw = std::cos(fovRad / 2.0f);
        float fovVRad = (FOV_DEG * 0.0174533f) / aspect; 
        float minPitchAngle = radPitch - fovVRad * 0.7f;
        float maxPitchAngle = radPitch + fovVRad * 0.7f;

        for(size_t i=0; i<renderChunks.size(); ++i) {
            auto& rc = renderChunks[i];
            
            // Skip rendering if mesh is not ready yet
            if (rc.gpuQuads.empty()) continue; 

            float cx = rc.chunkX * CHUNK_SIZE + CHUNK_SIZE/2.0f;
            float cz = rc.chunkZ * CHUNK_SIZE + CHUNK_SIZE/2.0f;
            
            float realDx = cx - camX;
            float realDz = cz - camZ;
            float realDistSq = realDx*realDx + realDz*realDz;
            float realDist = std::sqrt(realDistSq);

            float checkDx = cx - checkX;
            float checkDz = cz - checkZ;
            float checkDist = std::sqrt(checkDx*checkDx + checkDz*checkDz);

            if (realDist < (CHUNK_SIZE * 1.5f)) {
                visibleIndices.push_back({i, realDistSq});
                continue;
            }

            float dirX = checkDx / checkDist;
            float dirZ = checkDz / checkDist;
            float dot = dirX * camDirX + dirZ * camDirZ;

            if (dot < minCosYaw) { culledChunks++; continue; }

            float angleBottom = std::atan2(0.0f - camY, realDist);
            float angleTop = std::atan2((float)rc.maxHeight - camY, realDist);
            
            if (angleTop < minPitchAngle || angleBottom > maxPitchAngle) { culledChunks++; continue; }

            if (!FrustumCheckAABB(frustum, 
                rc.chunkX * CHUNK_SIZE, 0, rc.chunkZ * CHUNK_SIZE, 
                (rc.chunkX+1) * CHUNK_SIZE, CHUNK_SIZE, (rc.chunkZ+1) * CHUNK_SIZE)) {
                culledChunks++; continue;
            }

            visibleIndices.push_back({i, realDistSq});
        }

        std::sort(visibleIndices.begin(), visibleIndices.end(), [](const ChunkDist& a, const ChunkDist& b) {
            return a.distSq < b.distSq;
        });

        std::fill(horizonBuffer.begin(), horizonBuffer.end(), -100.0f); 
        visibleCommands.clear();
        visibleChunkInfos.clear();
        float standardFovRad = FOV_DEG * 0.0174533f;

        for (const auto& item : visibleIndices) {
            auto& rc = renderChunks[item.index];
            float cx = rc.chunkX * CHUNK_SIZE + CHUNK_SIZE/2.0f;
            float cz = rc.chunkZ * CHUNK_SIZE + CHUNK_SIZE/2.0f;
            float dx = cx - camX;
            float dz = cz - camZ;
            float dist = std::sqrt(item.distSq);
            bool isNearby = (dist < (CHUNK_SIZE * 1.5f));
            float chunkRadius = 24.0f;
            float angularHalfWidth = std::atan2(chunkRadius, dist);
            float angle = std::atan2(dz, dx) - radYaw;
            angle = WrapAngle(angle);
            float normAngleCenter = angle / (standardFovRad / 2.0f);
            float normAngleWidth = angularHalfWidth / (standardFovRad / 2.0f);
            
            float u_start = (normAngleCenter - normAngleWidth + 1.0f) * 0.5f;
            float u_end = (normAngleCenter + normAngleWidth + 1.0f) * 0.5f;

            int bucketStart = (int)(u_start * HORIZON_BUCKETS);
            int bucketEnd = (int)(u_end * HORIZON_BUCKETS);
            
            if (bucketStart < 0) bucketStart = 0;
            if (bucketEnd >= HORIZON_BUCKETS) bucketEnd = HORIZON_BUCKETS - 1;
            float relativeH = (float)rc.maxHeight - camY;
            float tanTheta = relativeH / dist;
            bool visible = false;

            if (isNearby) { visible = true; } else {
                if (bucketStart <= bucketEnd) {
                    for (int b = bucketStart; b <= bucketEnd; ++b) {
                        if (tanTheta >= horizonBuffer[b] - 0.01f) { visible = true; break; }
                    }
                } else { visible = true; }
            }

            if (!visible) { horizonCulled++; continue; }

            if (bucketStart <= bucketEnd) {
                for (int b = bucketStart; b <= bucketEnd; ++b) {
                    if (tanTheta > horizonBuffer[b]) horizonBuffer[b] = tanTheta;
                }
            }

            GL::DrawElementsIndirectCommand cmd;
            cmd.count = 6; 
            cmd.instanceCount = rc.gpuQuads.size(); 
            cmd.firstIndex = 0;
            cmd.baseVertex = 0;
            cmd.baseInstance = 0; 
            visibleCommands.push_back(cmd);
            visibleChunkInfos.push_back({rc.chunkX * CHUNK_SIZE, rc.chunkZ * CHUNK_SIZE, rc.globalBufferOffset, 0});
        }
        
        drawnChunks = visibleCommands.size();

        if (!visibleCommands.empty()) {
            GL::glBindBuffer(GL::SHADER_STORAGE_BUFFER, ssboChunks);
            GL::glBufferSubData(GL::SHADER_STORAGE_BUFFER, 0, visibleChunkInfos.size() * sizeof(GpuChunkInfo), visibleChunkInfos.data());
            GL::glBindBuffer(GL::DRAW_INDIRECT_BUFFER, indirectBuffer);
            GL::glBufferSubData(GL::DRAW_INDIRECT_BUFFER, 0, visibleCommands.size() * sizeof(GL::DrawElementsIndirectCommand), visibleCommands.data());
        }

        GL::glViewport(0,0,w,h);
        GL::glClearColor(0.1f, 0.1f, 0.1f, 1.0f); 
        GL::glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        GL::glEnable(GL_DEPTH_TEST); 
        GL::glEnable(GL_CULL_FACE);
        GL::glDepthMask(GL_TRUE); 

        GL::glUseProgram(shader.id);
        GL::glUniformMatrix4fv(shader.loc_view, 1, GL_FALSE, view.m);
        GL::glUniformMatrix4fv(shader.loc_proj, 1, GL_FALSE, proj.m);
        GL::glUniform1i(shader.loc_showGrid, showGrid ? 1 : 0);

        GL::glBindVertexArray(emptyVAO); 
        GL::glBindBuffer(GL::DRAW_INDIRECT_BUFFER, indirectBuffer);
        if (drawnChunks > 0) {
            GL::glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, 0, (GLsizei)drawnChunks, 0);
        }

        SDL_GL_SwapWindow(window);
        
        acc += frameTimer.end();
        frames++;
        frameCounter++;
        
        if (acc >= 500.0) {
            double fps = (frames * 1000.0) / acc;
            std::stringstream ss;
            ss << "Voxel Engine (MultiThreaded) | " << std::fixed << std::setprecision(1) << fps << " FPS | "
               << "Drawn: " << drawnChunks << " | Q: " << jobQueue.empty();
            SDL_SetWindowTitle(window, ss.str().c_str());
            acc = 0; frames = 0;
        }
    }

    stopWorkers = true;
    return 0;
}