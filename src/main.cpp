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

struct ChunkRenderDataExtended {
    std::vector<GpuQuad> gpuQuads;
    int chunkX, chunkZ;
    unsigned int globalBufferOffset; 
    int maxHeight; 
    int currentLod; 
};

// Нормалізація кута [-PI, PI]
float WrapAngle(float angle) {
    while (angle > 3.14159265f) angle -= 6.2831853f;
    while (angle < -3.14159265f) angle += 6.2831853f;
    return angle;
}

int main(int argc, char* argv[]) {
    // 1. Forced check format (User requirement: !SDL_Init)
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL_Init failed logic check (returned 0/success or error depending on SDL version)" << std::endl;
        // Продовжуємо, як просили
    }
    
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_Window* window = SDL_CreateWindow("Voxel Game - Advanced Culling", 1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
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
    for (int cx = -range; cx <= range; ++cx) {
        for (int cz = -range; cz <= range; ++cz) {
            TerrainSystem::Generate(world.createChunk(cx, cz), seed);
        }
    }

    std::cout << "Meshing chunks..." << std::endl;

    std::vector<ChunkRenderDataExtended> renderChunks;
    std::vector<GpuQuad> allGpuQuads;

    renderChunks.reserve(estChunks);
    allGpuQuads.reserve(estChunks * 2000); 

    for (int cx = -range; cx <= range; ++cx) {
        for (int cz = -range; cz <= range; ++cz) {
            VoxelChunk* chunk = world.getChunk(cx, cz);
            if(!chunk) continue;
            
            // Розрахунок maxH для Horizon/Pitch Culling
            int maxH = 0;
            for (int lx = 0; lx < CHUNK_SIZE; lx += 4) { 
                for (int lz = 0; lz < CHUNK_SIZE; lz += 4) {
                    for (int ly = CHUNK_SIZE - 1; ly >= 0; --ly) {
                        if (!IsTransparent(chunk->get(lx, ly, lz))) {
                            if (ly > maxH) maxH = ly;
                            break;
                        }
                    }
                }
            }
            maxH += 2; 
            if (maxH > CHUNK_SIZE) maxH = CHUNK_SIZE;

            MeshingContext ctx = {}; 
            ctx.center = chunk;
            ctx.neighbors[0] = world.getChunk(cx + 1, cz);
            ctx.neighbors[1] = world.getChunk(cx - 1, cz);
            ctx.neighbors[4] = world.getChunk(cx, cz + 1);
            ctx.neighbors[5] = world.getChunk(cx, cz - 1);

            float dist = std::sqrt(float(cx * cx + cz * cz)) * CHUNK_SIZE;
            int lod = GetChunkLOD(dist); 

            std::vector<Quad> quads = GenerateQuads(ctx, lod);
            std::vector<GpuQuad> gpuQ = BuildSSBOData(quads);
            
            if(!gpuQ.empty()) {
                unsigned int offset = (unsigned int)allGpuQuads.size();
                allGpuQuads.insert(allGpuQuads.end(), gpuQ.begin(), gpuQ.end());
                
                ChunkRenderDataExtended crd;
                crd.gpuQuads = std::move(gpuQ);
                crd.chunkX = cx;
                crd.chunkZ = cz;
                crd.globalBufferOffset = offset;
                crd.maxHeight = maxH;
                crd.currentLod = lod;
                
                renderChunks.push_back(std::move(crd));
            }
        }
    }

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
    GL::glBufferData(GL::SHADER_STORAGE_BUFFER, allGpuQuads.size() * sizeof(GpuQuad), allGpuQuads.data(), GL_DYNAMIC_DRAW); 
    GL::glBindBufferBase(GL::SHADER_STORAGE_BUFFER, 0, ssboQuads);

    GL::glGenBuffers(1, &ssboChunks);
    GL::glBindBuffer(GL::SHADER_STORAGE_BUFFER, ssboChunks);
    GL::glBufferData(GL::SHADER_STORAGE_BUFFER, renderChunks.size() * sizeof(GpuChunkInfo), nullptr, GL_DYNAMIC_DRAW);
    GL::glBindBufferBase(GL::SHADER_STORAGE_BUFFER, 1, ssboChunks);

    GL::glGenBuffers(1, &indirectBuffer);
    GL::glBindBuffer(GL::DRAW_INDIRECT_BUFFER, indirectBuffer);
    GL::glBufferData(GL::DRAW_INDIRECT_BUFFER, renderChunks.size() * sizeof(GL::DrawElementsIndirectCommand), nullptr, GL_DYNAMIC_DRAW);

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
    visibleIndices.reserve(renderChunks.size());

    // Буфер горизонту
    std::vector<float> horizonBuffer(HORIZON_BUCKETS);

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
        
        // --- LOD REGENERATION ---
        if (frameCounter % 30 == 0) {
            bool geometryChanged = false;
            for(auto& rc : renderChunks) {
                float dx = (float)rc.chunkX * CHUNK_SIZE + 16.0f - camX;
                float dz = (float)rc.chunkZ * CHUNK_SIZE + 16.0f - camZ;
                float dist = std::sqrt(dx*dx + dz*dz);
                int neededLod = GetChunkLOD(dist);
                if (rc.currentLod != neededLod) {
                    VoxelChunk* chunk = world.getChunk(rc.chunkX, rc.chunkZ);
                    if (chunk) {
                        MeshingContext ctx = {}; 
                        ctx.center = chunk;
                        ctx.neighbors[0] = world.getChunk(rc.chunkX + 1, rc.chunkZ);
                        ctx.neighbors[1] = world.getChunk(rc.chunkX - 1, rc.chunkZ);
                        ctx.neighbors[4] = world.getChunk(rc.chunkX, rc.chunkZ + 1);
                        ctx.neighbors[5] = world.getChunk(rc.chunkX, rc.chunkZ - 1);
                        std::vector<Quad> quads = GenerateQuads(ctx, neededLod);
                        rc.gpuQuads = BuildSSBOData(quads);
                        rc.currentLod = neededLod;
                        geometryChanged = true;
                    }
                }
            }
            if (geometryChanged) {
                allGpuQuads.clear();
                allGpuQuads.reserve(renderChunks.size() * 1500); 
                for(auto& rc : renderChunks) {
                    rc.globalBufferOffset = (unsigned int)allGpuQuads.size();
                    allGpuQuads.insert(allGpuQuads.end(), rc.gpuQuads.begin(), rc.gpuQuads.end());
                }
                GL::glBindBuffer(GL::SHADER_STORAGE_BUFFER, ssboQuads);
                GL::glBufferData(GL::SHADER_STORAGE_BUFFER, allGpuQuads.size() * sizeof(GpuQuad), allGpuQuads.data(), GL_DYNAMIC_DRAW);
            }
        }

        // --- CULLING PASS (Advanced) ---
        visibleIndices.clear();
        culledChunks = 0;
        horizonCulled = 0;

        float camDirX = std::cos(radYaw);
        float camDirZ = std::sin(radYaw);
        
        // 1. Dynamic Yaw Culling with Backward Shift
        // Чим більший нахил камери (вниз або вгору), тим більше ми "зсуваємо" центр перевірки Yaw назад.
        // Це розширює конус огляду навколо гравця, імітуючи трапецію.
        float pitchFactor = std::abs(std::sin(radPitch)); 
        
        // Зміщення центру перевірки назад (проти напрямку погляду)
        // При 90 градусах (дивимось вниз) зміщуємо на радіус огляду, щоб бачити все навколо
        float backwardShift = pitchFactor * (range * CHUNK_SIZE * 0.8f); 
        float checkX = camX - camDirX * backwardShift;
        float checkZ = camZ - camDirZ * backwardShift;

        // Базовий FOV для Yaw
        float fovRad = (FOV_DEG + YAW_PADDING) * 0.0174533f;
        float minCosYaw = std::cos(fovRad / 2.0f);

        // 2. Pitch Culling Params
        float fovVRad = (FOV_DEG * 0.0174533f) / aspect; 
        float minPitchAngle = radPitch - fovVRad * 0.7f;
        float maxPitchAngle = radPitch + fovVRad * 0.7f;

        for(size_t i=0; i<renderChunks.size(); ++i) {
            auto& rc = renderChunks[i];
            float cx = rc.chunkX * CHUNK_SIZE + CHUNK_SIZE/2.0f;
            float cz = rc.chunkZ * CHUNK_SIZE + CHUNK_SIZE/2.0f;
            
            // Дистанція до реальної камери
            float realDx = cx - camX;
            float realDz = cz - camZ;
            float realDistSq = realDx*realDx + realDz*realDz;
            float realDist = std::sqrt(realDistSq);

            // Дистанція до центру перевірки Yaw (зсунутого)
            float checkDx = cx - checkX;
            float checkDz = cz - checkZ;
            float checkDist = std::sqrt(checkDx*checkDx + checkDz*checkDz);

            // Завжди малюємо дуже близькі чанки (до реальної камери)
            if (realDist < (CHUNK_SIZE * 1.5f)) {
                visibleIndices.push_back({i, realDistSq});
                continue;
            }

            // --- Yaw Culling (Shifted Origin) ---
            float dirX = checkDx / checkDist;
            float dirZ = checkDz / checkDist;
            float dot = dirX * camDirX + dirZ * camDirZ;

            if (dot < minCosYaw) {
                culledChunks++;
                continue;
            }

            // --- Pitch Culling (Vertical) ---
            // Використовуємо реальну дистанцію
            float angleBottom = std::atan2(0.0f - camY, realDist);
            float angleTop = std::atan2((float)rc.maxHeight - camY, realDist);
            
            if (angleTop < minPitchAngle || angleBottom > maxPitchAngle) {
                culledChunks++;
                continue;
            }

            // --- Frustum Culling ---
            if (!FrustumCheckAABB(frustum, 
                rc.chunkX * CHUNK_SIZE, 0, rc.chunkZ * CHUNK_SIZE, 
                (rc.chunkX+1) * CHUNK_SIZE, CHUNK_SIZE, (rc.chunkZ+1) * CHUNK_SIZE)) {
                culledChunks++;
                continue;
            }

            visibleIndices.push_back({i, realDistSq});
        }

        // Сортуємо Front-to-Back
        std::sort(visibleIndices.begin(), visibleIndices.end(), [](const ChunkDist& a, const ChunkDist& b) {
            return a.distSq < b.distSq;
        });

        // 3. Horizon Occlusion Logic
        std::fill(horizonBuffer.begin(), horizonBuffer.end(), -100.0f); 

        visibleCommands.clear();
        visibleChunkInfos.clear();

        // Стандартний FOV для мапінгу горизонту
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

            if (isNearby) {
                visible = true; 
            } else {
                if (bucketStart <= bucketEnd) {
                    for (int b = bucketStart; b <= bucketEnd; ++b) {
                        // Зменшив bias для більш агресивного відсікання, оскільки у нас є Yaw/Pitch пре-фільтри
                        if (tanTheta >= horizonBuffer[b] - 0.01f) {
                            visible = true;
                            break;
                        }
                    }
                } else {
                    // Fallback для дуже далеких або тих, що не потрапили в бакети
                    visible = true;
                }
            }

            if (!visible) {
                horizonCulled++;
                continue;
            }

            if (bucketStart <= bucketEnd) {
                for (int b = bucketStart; b <= bucketEnd; ++b) {
                    if (tanTheta > horizonBuffer[b]) {
                        horizonBuffer[b] = tanTheta;
                    }
                }
            }

            GL::DrawElementsIndirectCommand cmd;
            cmd.count = 6; 
            cmd.instanceCount = rc.gpuQuads.size(); 
            cmd.firstIndex = 0;
            cmd.baseVertex = 0;
            cmd.baseInstance = 0; 
            
            visibleCommands.push_back(cmd);
            visibleChunkInfos.push_back({
                rc.chunkX * CHUNK_SIZE, 
                rc.chunkZ * CHUNK_SIZE, 
                rc.globalBufferOffset, 
                0
            });
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
            ss << "Voxel Engine | " << std::fixed << std::setprecision(1) << fps << " FPS | "
               << "Drawn: " << drawnChunks << " | Culled: " << culledChunks << " | HorizCull: " << horizonCulled;
            SDL_SetWindowTitle(window, ss.str().c_str());
            acc = 0; frames = 0;
        }
    }
    return 0;
}