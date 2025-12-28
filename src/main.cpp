#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <vector>
#include <iostream>
#include <ctime>
#include <cmath>
#include <algorithm> 
#include <thread>
#include <queue>
#include <mutex>
#include <atomic>
#include <cstring> 
#include <iomanip> 

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
const int RENDER_DISTANCE = 1; 
const int MAX_CHUNKS = (RENDER_DISTANCE * 2 + 1) * (RENDER_DISTANCE * 2 + 1);
const size_t MAX_QUADS_BUFFER = 16000000; 

// --- GPU Structures ---
struct GpuChunkInput {
    int packedXZ; 
    unsigned int quadStart;
    unsigned int quadCount;
    int maxHeight;
    unsigned int scale; // 1, 2, 4
    int padding;
};

struct MeshJob { int cx, cz, lod; };
struct MeshResult { int cx, cz, lod, maxHeight; std::vector<GpuQuad> gpuQuads; bool valid; };

template<typename T>
class SafeQueue {
    std::queue<T> q; std::mutex m;
public:
    void push(T val) { std::lock_guard<std::mutex> lk(m); q.push(std::move(val)); }
    bool try_pop(T& val) { std::lock_guard<std::mutex> lk(m); if(q.empty())return false; val=std::move(q.front()); q.pop(); return true; }
    bool empty() { std::lock_guard<std::mutex> lk(m); return q.empty(); }
};

SafeQueue<MeshJob> jobQueue;
SafeQueue<MeshResult> resultQueue;
std::atomic<bool> stopWorkers{false};

void MeshWorker(WorldManager* world) {
    while(!stopWorkers) {
        MeshJob job;
        if(jobQueue.try_pop(job)) {
            VoxelChunk* center = world->getChunk(job.cx, job.cz);
            if (!center) { resultQueue.push({job.cx, job.cz, job.lod, 0, {}, false}); continue; }

            MeshingContext ctx = {};
            ctx.center = center;
            ctx.neighbors[0] = world->getChunk(job.cx+1, job.cz);
            ctx.neighbors[1] = world->getChunk(job.cx-1, job.cz);
            ctx.neighbors[4] = world->getChunk(job.cx, job.cz+1);
            ctx.neighbors[5] = world->getChunk(job.cx, job.cz-1);
            ctx.neighbors[2] = nullptr; 
            ctx.neighbors[3] = nullptr;

            int maxH = 0;
            for (int lx=0; lx<CHUNK_SIZE; lx+=4) 
                for (int lz=0; lz<CHUNK_SIZE; lz+=4) 
                    for (int ly=CHUNK_SIZE-1; ly>=0; --ly) 
                        if (!IsTransparent(center->get(lx, ly, lz))) { if (ly>maxH) maxH=ly; break; }
            maxH += 2; if (maxH>CHUNK_SIZE) maxH=CHUNK_SIZE;

            auto quads = GenerateQuads(ctx, job.lod);
            resultQueue.push({job.cx, job.cz, job.lod, maxH, BuildSSBOData(quads), true});
        } else std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
}

struct RenderChunk {
    int cx, cz;
    int lod;
    int maxHeight;
    unsigned int gpuOffset; 
    unsigned int gpuCount;
    bool inGpu;
};

struct GpuMemoryManager {
    size_t currentOffset = 0;
    size_t capacity = 0;
    void Init(size_t cap) { capacity = cap; currentOffset = 0; }
    bool Allocate(size_t count, unsigned int& outOffset) {
        if (currentOffset + count > capacity) return false;
        outOffset = (unsigned int)currentOffset;
        currentOffset += count;
        return true;
    }
    void Reset() { currentOffset = 0; }
};

int main(int argc, char* argv[]) {
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "x11"); 

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << std::endl;
        return -1;
    }
    
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    
    SDL_Window* window = SDL_CreateWindow("Voxel Engine - Keyboard Support", 1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) return -1;

    SDL_GLContext context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, context);
    SDL_GL_SetSwapInterval(0);
    GL::LoadFunctions();
    
    bool mouseCaptured = true;
    SDL_SetWindowRelativeMouseMode(window, true);
    
    WorldManager world;
    int seed = (int)std::time(nullptr);
    std::cout << "Generating " << MAX_CHUNKS << " chunks..." << std::endl;
    for (int cx = -RENDER_DISTANCE; cx <= RENDER_DISTANCE; ++cx) {
        for (int cz = -RENDER_DISTANCE; cz <= RENDER_DISTANCE; ++cz) {
            TerrainSystem::Generate(world.createChunk(cx, cz), seed);
        }
    }

    unsigned int threadCount = std::thread::hardware_concurrency();
    std::vector<std::jthread> workers;
    for(unsigned int i=0; i<threadCount; ++i) workers.emplace_back(MeshWorker, &world);

    std::vector<RenderChunk> renderChunks;
    renderChunks.reserve(MAX_CHUNKS);
    int totalInitialChunks = 0;
    for (int cx = -RENDER_DISTANCE; cx <= RENDER_DISTANCE; ++cx) {
        for (int cz = -RENDER_DISTANCE; cz <= RENDER_DISTANCE; ++cz) {
            renderChunks.push_back({cx, cz, -1, 0, 0, 0, false});
            jobQueue.push({cx, cz, 1}); 
            totalInitialChunks++;
        }
    }
    
    auto findChunk = [&](int cx, int cz) -> RenderChunk* {
        int r = RENDER_DISTANCE;
        int idx = (cx + r) * (2*r + 1) + (cz + r);
        if (idx >= 0 && idx < (int)renderChunks.size()) return &renderChunks[idx];
        return nullptr;
    };

    GLuint ssboQuads, ssboInputChunks, ssboDrawCommands, ssboVisibleChunks, atomicCounterBuf;
    GL::glGenBuffers(1, &ssboQuads);
    GL::glBindBuffer(GL::SHADER_STORAGE_BUFFER, ssboQuads);
    GL::glBufferData(GL::SHADER_STORAGE_BUFFER, MAX_QUADS_BUFFER * sizeof(GpuQuad), nullptr, GL_DYNAMIC_DRAW);
    GL::glBindBufferBase(GL::SHADER_STORAGE_BUFFER, 0, ssboQuads); 

    GL::glGenBuffers(1, &ssboInputChunks);
    GL::glBindBuffer(GL::SHADER_STORAGE_BUFFER, ssboInputChunks);
    GL::glBufferData(GL::SHADER_STORAGE_BUFFER, MAX_CHUNKS * sizeof(GpuChunkInput), nullptr, GL_DYNAMIC_DRAW);
    GL::glBindBufferBase(GL::SHADER_STORAGE_BUFFER, 1, ssboInputChunks); 

    GL::glGenBuffers(1, &ssboDrawCommands);
    GL::glBindBuffer(GL::SHADER_STORAGE_BUFFER, ssboDrawCommands);
    GL::glBufferData(GL::SHADER_STORAGE_BUFFER, MAX_CHUNKS * sizeof(GL::DrawElementsIndirectCommand), nullptr, GL_DYNAMIC_DRAW);
    GL::glBindBufferBase(GL::SHADER_STORAGE_BUFFER, 2, ssboDrawCommands); 
    
    GL::glGenBuffers(1, &ssboVisibleChunks);
    GL::glBindBuffer(GL::SHADER_STORAGE_BUFFER, ssboVisibleChunks);
    GL::glBufferData(GL::SHADER_STORAGE_BUFFER, MAX_CHUNKS * sizeof(GpuChunkInfo), nullptr, GL_DYNAMIC_DRAW);
    GL::glBindBufferBase(GL::SHADER_STORAGE_BUFFER, 3, ssboVisibleChunks); 

    GL::glGenBuffers(1, &atomicCounterBuf);
    GL::glBindBuffer(GL::ATOMIC_COUNTER_BUFFER, atomicCounterBuf); 
    GLuint zero = 0;
    GL::glBufferData(GL::ATOMIC_COUNTER_BUFFER, sizeof(GLuint), &zero, GL_DYNAMIC_DRAW);
    GL::glBindBufferBase(GL::ATOMIC_COUNTER_BUFFER, 0, atomicCounterBuf);

    ShaderProgram drawShader = CreateProgram("src/shaders/voxel.vert.glsl", "src/shaders/voxel.frag.glsl");
    ShaderProgram cullShader = CreateComputeProgram("src/shaders/cull.comp.glsl");
    
    GLuint heightMapTex;
    GL::glGenTextures(1, &heightMapTex);
    GL::glBindTexture(GL_TEXTURE_2D, heightMapTex);
    int mapSize = RENDER_DISTANCE * 2 + 1;
    GL::glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, mapSize, mapSize, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
    GL::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    GL::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    GL::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F); 
    GL::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F);
    std::vector<GLubyte> heightMapData(mapSize * mapSize, 0);

    GLuint emptyVAO, ibo;
    GL::glGenVertexArrays(1, &emptyVAO);
    GL::glBindVertexArray(emptyVAO);
    GLuint indices[] = {0,1,2, 0,2,3};
    GL::glGenBuffers(1, &ibo);
    GL::glBindBuffer(0x8893, ibo); 
    GL::glBufferData(0x8893, sizeof(indices), indices, GL_STATIC_DRAW);
    GL::glBindVertexArray(0);

    GLuint texArrayID = CreatePaletteTextureArray();

    float camX=0, camY=60, camZ=0, yaw=-90, pitch=-30;
    bool showGrid = false;
    GpuMemoryManager gpuMem;
    gpuMem.Init(MAX_QUADS_BUFFER);

    std::vector<GpuChunkInput> inputChunksData(MAX_CHUNKS);
    bool chunksDataDirty = true;

    GLint loc_cull_heightMap = GL::glGetUniformLocation(cullShader.id, "u_heightMap");
    GLint loc_cull_renderDist = GL::glGetUniformLocation(cullShader.id, "u_renderDist");
    GLint loc_cull_viewDir = GL::glGetUniformLocation(cullShader.id, "u_viewDir");

    Profiler fpsTimer;
    double acc=0; int frames=0;
    bool initialLoadDone = false;
    int chunksLoadedCount = 0;
    
    bool isFullscreen = false;
    uint32_t lastEscTime = 0;
    const uint32_t DOUBLE_CLICK_TIME = 300; 

    bool running = true;
    while(running) {
        fpsTimer.begin();
        SDL_Event ev;
        
        while(SDL_PollEvent(&ev)) {
            if(ev.type == SDL_EVENT_QUIT) running = false;
            
            if(ev.type == SDL_EVENT_KEY_DOWN && ev.key.key == SDLK_F11) {
                isFullscreen = !isFullscreen;
                SDL_SetWindowFullscreen(window, isFullscreen);
            }

            if(ev.type == SDL_EVENT_KEY_DOWN && ev.key.key == SDLK_ESCAPE) { 
                uint32_t now = (uint32_t)SDL_GetTicks();
                if (mouseCaptured) {
                    mouseCaptured = false;
                    SDL_SetWindowRelativeMouseMode(window, false);
                } else {
                    if (now - lastEscTime < DOUBLE_CLICK_TIME) running = false; 
                    else lastEscTime = now;
                }
            }
            if(ev.type == SDL_EVENT_KEY_DOWN && ev.key.key == SDLK_G) showGrid = !showGrid;
            
            if(ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN && !mouseCaptured) {
                mouseCaptured = true;
                SDL_SetWindowRelativeMouseMode(window, true);
            }

            if(ev.type == SDL_EVENT_MOUSE_MOTION && mouseCaptured) {
                // Пряме зчитування дельти. Якщо миша впирається в край хоста, xrel/yrel стануть 0.
                yaw += ev.motion.xrel * 0.1f; 
                pitch -= ev.motion.yrel * 0.1f;
            }
        }

        const bool* keys = SDL_GetKeyboardState(nullptr);
        
        // --- Альтернативне обертання клавіатурою ---
        float rotateSpeed = 1.5f;
        if(keys[SDL_SCANCODE_LEFT])  { yaw -= rotateSpeed; }
        if(keys[SDL_SCANCODE_RIGHT]) { yaw += rotateSpeed; }
        if(keys[SDL_SCANCODE_UP])    { pitch += rotateSpeed; }
        if(keys[SDL_SCANCODE_DOWN])  { pitch -= rotateSpeed; }
        if(pitch > 89.0f) pitch = 89.0f;
        if(pitch < -89.0f) pitch = -89.0f;

        float radYaw = yaw * 0.0174533f, radPitch = pitch * 0.0174533f;
        Vec3 front = Normalize({ std::cos(radYaw)*std::cos(radPitch), std::sin(radPitch), std::sin(radYaw)*std::cos(radPitch) });
        Vec3 right = Normalize(Cross(front, {0,1,0}));
        
        // --- Рух ---
        float speed = keys[SDL_SCANCODE_LSHIFT] ? 2.5f : 0.8f;
        if(keys[SDL_SCANCODE_W]) { camX+=front.x*speed; camY+=front.y*speed; camZ+=front.z*speed; }
        if(keys[SDL_SCANCODE_S]) { camX-=front.x*speed; camY-=front.y*speed; camZ-=front.z*speed; }
        if(keys[SDL_SCANCODE_A]) { camX-=right.x*speed; camY-=right.y*speed; camZ-=right.z*speed; }
        if(keys[SDL_SCANCODE_D]) { camX+=right.x*speed; camY+=right.y*speed; camZ+=right.z*speed; }
        
        // Рух по осі Y
        if(keys[SDL_SCANCODE_SPACE]) { camY += speed; }
        if(keys[SDL_SCANCODE_LCTRL]) { camY -= speed; }

        int w, h; SDL_GetWindowSizeInPixels(window, &w, &h);
        Mat4 view = LookAt({camX, camY, camZ}, {camX+front.x, camY+front.y, camZ+front.z}, {0,1,0});
        Mat4 proj = Perspective(1.047f, (float)w/h, 0.1f, 40000.0f);
        Mat4 viewProj = MultiplyMat4(proj, view);
        Frustum frustum = CreateFrustum(viewProj);

        MeshResult res;
        int processLimit = initialLoadDone ? 50 : 500; 
        int processed = 0;
        bool memoryFull = false;
        
        while(processed < processLimit && resultQueue.try_pop(res)) {
            if(!res.valid) continue;
            RenderChunk* rc = findChunk(res.cx, res.cz);
            if(!rc) continue;

            unsigned int newOffset = 0;
            if (gpuMem.Allocate(res.gpuQuads.size(), newOffset)) {
                if(!res.gpuQuads.empty()) {
                    GL::glBindBuffer(GL::SHADER_STORAGE_BUFFER, ssboQuads);
                    GL::glBufferSubData(GL::SHADER_STORAGE_BUFFER, newOffset * sizeof(GpuQuad), res.gpuQuads.size() * sizeof(GpuQuad), res.gpuQuads.data());
                }
                rc->gpuOffset = newOffset;
                rc->gpuCount = (unsigned int)res.gpuQuads.size();
                rc->maxHeight = res.maxHeight;
                rc->lod = res.lod;
                rc->inGpu = true;
                chunksDataDirty = true;
            } else { memoryFull = true; }
            if (!initialLoadDone) chunksLoadedCount++;
            processed++;
        }

        if (!initialLoadDone) {
            if (chunksLoadedCount >= totalInitialChunks) {
                initialLoadDone = true;
                SDL_SetWindowTitle(window, "Voxel Game - Ready");
            } else {
                GL::glViewport(0,0,w,h);
                GL::glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
                GL::glClear(GL_COLOR_BUFFER_BIT);
                SDL_GL_SwapWindow(window);
                continue; 
            }
        }

        if (chunksDataDirty) {
            int idx = 0;
            for(const auto& rc : renderChunks) {
                if(rc.inGpu) {
                    int16_t sx = (int16_t)rc.cx;
                    int16_t sz = (int16_t)rc.cz;
                    int packed = (uint16_t)sx | ((uint16_t)sz << 16);
                    unsigned int scale = (rc.lod == 3) ? 4 : ((rc.lod == 2) ? 2 : 1);
                    inputChunksData[idx] = {packed, rc.gpuOffset, rc.gpuCount, rc.maxHeight, scale, 0};
                } else {
                    inputChunksData[idx] = {0, 0, 0, 0, 1, 0};
                }
                int u = rc.cx + RENDER_DISTANCE;
                int v = rc.cz + RENDER_DISTANCE;
                if (u >= 0 && u < mapSize && v >= 0 && v < mapSize) heightMapData[v * mapSize + u] = (GLubyte)rc.maxHeight;
                idx++;
            }
            GL::glBindBuffer(GL::SHADER_STORAGE_BUFFER, ssboInputChunks);
            GL::glBufferSubData(GL::SHADER_STORAGE_BUFFER, 0, MAX_CHUNKS * sizeof(GpuChunkInput), inputChunksData.data());
            GL::glActiveTexture(GL_TEXTURE1); 
            GL::glBindTexture(GL_TEXTURE_2D, heightMapTex);
            GL::glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, mapSize, mapSize, GL_RED, GL_UNSIGNED_BYTE, heightMapData.data());
            chunksDataDirty = false;
        }
        
        if (memoryFull) {
            std::cout << "GPU Memory Full! Resetting..." << std::endl;
            gpuMem.Reset();
            for(auto& rc : renderChunks) { rc.inGpu = false; jobQueue.push({rc.cx, rc.cz, rc.lod}); } 
        }

        GL::glUseProgram(cullShader.id);
        GL::glUniformMatrix4fv(cullShader.loc_cull_viewProj, 1, GL_FALSE, viewProj.m);
        GL::glUniform3f(cullShader.loc_cull_camPos, camX, camY, camZ);
        if(loc_cull_viewDir != -1) GL::glUniform3f(loc_cull_viewDir, front.x, front.y, front.z); 
        GL::glUniform1ui(cullShader.loc_cull_chunkCount, MAX_CHUNKS);
        if(loc_cull_heightMap != -1) GL::glUniform1i(loc_cull_heightMap, 1); 
        if(loc_cull_renderDist != -1) GL::glUniform1f(loc_cull_renderDist, (float)RENDER_DISTANCE);

        float planesFlat[24];
        for(int i=0;i<6;++i) {
            planesFlat[i*4+0]=frustum.planes[i].x; planesFlat[i*4+1]=frustum.planes[i].y; 
            planesFlat[i*4+2]=frustum.planes[i].z; planesFlat[i*4+3]=frustum.planes[i].w; 
        }
        if(cullShader.loc_cull_frustumPlanes != -1)
             GL::glUniform4fv(cullShader.loc_cull_frustumPlanes, 6, planesFlat);

        GLuint zero = 0;
        GL::glBindBuffer(GL::ATOMIC_COUNTER_BUFFER, atomicCounterBuf);
        GL::glBufferSubData(GL::ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint), &zero);

        GL::glBindBufferBase(GL::SHADER_STORAGE_BUFFER, 1, ssboInputChunks);   
        GL::glBindBufferBase(GL::SHADER_STORAGE_BUFFER, 2, ssboDrawCommands); 
        GL::glBindBufferBase(GL::SHADER_STORAGE_BUFFER, 3, ssboVisibleChunks);
        
        GL::glActiveTexture(GL_TEXTURE1);
        GL::glBindTexture(GL_TEXTURE_2D, heightMapTex);

        GL::glDispatchCompute((MAX_CHUNKS + 63) / 64, 1, 1);
        GL::glMemoryBarrier(0x00000040 | 0x00002000); 

        GL::glViewport(0,0,w,h);
        GL::glClearColor(0.53f, 0.8f, 0.92f, 1.0f);
        GL::glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        GL::glEnable(GL_DEPTH_TEST);
        GL::glDisable(GL_CULL_FACE); 

        GL::glUseProgram(drawShader.id);
        GL::glUniformMatrix4fv(drawShader.loc_view, 1, GL_FALSE, view.m);
        GL::glUniformMatrix4fv(drawShader.loc_proj, 1, GL_FALSE, proj.m);
        GL::glUniform1i(drawShader.loc_showGrid, showGrid ? 1 : 0);
        GL::glActiveTexture(GL_TEXTURE0);
        GL::glBindTexture(GL_TEXTURE_2D_ARRAY, texArrayID);
        GL::glUniform1i(drawShader.loc_textureArray, 0);

        GL::glBindVertexArray(emptyVAO);
        GL::glBindBuffer(GL::DRAW_INDIRECT_BUFFER, ssboDrawCommands);
        GL::glBindBufferBase(GL::SHADER_STORAGE_BUFFER, 1, ssboVisibleChunks);
        GL::glBindBufferBase(GL::SHADER_STORAGE_BUFFER, 0, ssboQuads);

        GL::glBindBuffer(GL::ATOMIC_COUNTER_BUFFER, atomicCounterBuf);
        GLuint drawnCount = 0;
        GL::glGetBufferSubData(GL::ATOMIC_COUNTER_BUFFER, 0, sizeof(GLuint), &drawnCount);

        if(drawnCount > 0) {
            GL::glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, 0, (GLsizei)drawnCount, 0);
        }

        SDL_GL_SwapWindow(window);

        if (initialLoadDone && frames % 30 == 0) {
             bool highAltitude = (camY > 160.0f); 
             for(auto& rc : renderChunks) {
                float dist = std::sqrt(pow(rc.cx*32+16-camX,2)+pow(rc.cz*32+16-camZ,2));
                int needed = (dist > 192) ? 3 : ((dist > 96) ? 2 : 1);
                if (highAltitude && needed == 1) needed = 2;
                if(rc.lod != (int)needed) jobQueue.push({rc.cx, rc.cz, (int)needed}); 
             }
        }

        acc += fpsTimer.end(); frames++;
        if(acc >= 500) {
            std::stringstream ss; ss << "Voxel GPU | " << std::fixed << std::setprecision(1) << (frames*1000.0/acc) 
                                     << " FPS | Chunks: " << drawnCount << "/" << MAX_CHUNKS;
            SDL_SetWindowTitle(window, ss.str().c_str()); acc=0; frames=0;
        }
    }
    stopWorkers = true;
    return 0;
}