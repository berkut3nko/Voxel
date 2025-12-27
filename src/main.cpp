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

#ifndef GL_ANY_SAMPLES_PASSED_CONSERVATIVE
#define GL_ANY_SAMPLES_PASSED_CONSERVATIVE 0x8D6A
#endif

// Розширена структура даних чанка
struct ChunkRenderDataExtended {
    std::vector<GpuQuad> gpuQuads;
    int chunkX, chunkZ;
    unsigned int globalBufferOffset; 
    
    GLuint queryID;
    bool visible;     
    bool waitingForQuery; 
    bool wasInFrustum; // Для відстеження входу/виходу
};

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) return -1;
    
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_Window* window = SDL_CreateWindow("Voxel Game - Final Optimization", 1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) return -1;

    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (!context) return -1;
    
    SDL_GL_MakeCurrent(window, context);
    SDL_GL_SetSwapInterval(0); 
    
    GL::LoadFunctions(); 

    bool occlusionSupported = (GL::glGenQueries != nullptr && GL::glBeginQuery != nullptr);
    if (!occlusionSupported) std::cerr << "WARNING: Occlusion Queries not supported." << std::endl;
    
    bool mouseCaptured = true;
    SDL_SetWindowRelativeMouseMode(window, true);

    WorldManager world;
    int seed = std::time(nullptr) % 1000;
    int range = 6; 
    size_t estChunks = (range * 2 + 1) * (range * 2 + 1);

    // --- Batch Query Generation ---
    std::vector<GLuint> queryPool;
    if (occlusionSupported) {
        queryPool.resize(estChunks + 50); 
        GL::glGenQueries((GLsizei)queryPool.size(), queryPool.data());
    }
    size_t queryPoolIdx = 0;
    
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
            
            MeshingContext ctx;
            ctx.center = chunk;
            ctx.neighbors[0] = world.getChunk(cx + 1, cz);
            ctx.neighbors[1] = world.getChunk(cx - 1, cz);
            ctx.neighbors[4] = world.getChunk(cx, cz + 1);
            ctx.neighbors[5] = world.getChunk(cx, cz - 1);

            std::vector<Quad> quads = GenerateQuads(ctx);
            std::vector<GpuQuad> gpuQ = BuildSSBOData(quads);
            
            if(!gpuQ.empty()) {
                unsigned int offset = (unsigned int)allGpuQuads.size();
                allGpuQuads.insert(allGpuQuads.end(), gpuQ.begin(), gpuQ.end());
                
                GLuint qID = 0;
                if (occlusionSupported && queryPoolIdx < queryPool.size()) {
                    qID = queryPool[queryPoolIdx++];
                }
                
                // visible = true, wasInFrustum = false
                renderChunks.push_back({std::move(gpuQ), cx, cz, offset, qID, true, false, false});
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
    GL::glBufferData(GL::SHADER_STORAGE_BUFFER, allGpuQuads.size() * sizeof(GpuQuad), allGpuQuads.data(), GL_STATIC_DRAW);
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
    ShaderProgram boxShader = CreateBoxShader(); 
    if (boxShader.id == 0) return -1;
    GLuint cubeVAO = CreateCubeVAO(); 
    
    GL::glUseProgram(shader.id);
    GLuint texArrayID = CreatePaletteTextureArray(); 
    GL::glActiveTexture(GL_TEXTURE0);
    GL::glBindTexture(GL_TEXTURE_2D_ARRAY, texArrayID);
    GL::glUniform1i(shader.loc_textureArray, 0);

    float camX = 0.0f, camY = 40.0f, camZ = 0.0f;
    float yaw = -90.0f, pitch = -30.0f;
    bool showGrid = false;
    bool enableOcclusion = occlusionSupported; 

    Profiler frameTimer;
    double acc=0;
    int frames=0;
    int drawnChunks = 0; 
    int culledByOcclusion = 0;
    unsigned int frameCounter = 0;

    std::vector<GL::DrawElementsIndirectCommand> visibleCommands;
    std::vector<GpuChunkInfo> visibleChunkInfos;
    struct ChunkDist { size_t index; float distSq; };
    std::vector<ChunkDist> visibleIndices;

    bool running = true;
    while(running) {
        frameTimer.begin();
        SDL_Event ev;
        while(SDL_PollEvent(&ev)) {
             if(ev.type == SDL_EVENT_QUIT) running = false;
             if(ev.type == SDL_EVENT_KEY_DOWN) {
                 if (ev.key.key == SDLK_ESCAPE) { mouseCaptured = false; SDL_SetWindowRelativeMouseMode(window, false); }
                 if (ev.key.key == SDLK_G) showGrid = !showGrid;
                 if (ev.key.key == SDLK_O && occlusionSupported) enableOcclusion = !enableOcclusion;
             }
             if(ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN) { mouseCaptured = true; SDL_SetWindowRelativeMouseMode(window, true); }
             if(ev.type == SDL_EVENT_MOUSE_MOTION && mouseCaptured) {
                 yaw += ev.motion.xrel * 0.1f; pitch -= ev.motion.yrel * 0.1f;
                 if(pitch > 89.0f) pitch = 89.0f; if(pitch < -89.0f) pitch = -89.0f;
             }
        }
        
        // --- 1. Check Query Results (Non-blocking) ---
        culledByOcclusion = 0;
        if(enableOcclusion && occlusionSupported) {
            for(auto& rc : renderChunks) {
                if(rc.waitingForQuery && rc.queryID != 0) {
                    GLuint available = 0;
                    GL::glGetQueryObjectuiv(rc.queryID, GL::QUERY_RESULT_AVAILABLE, &available);
                    
                    if (available) {
                        GLuint result = 1;
                        GL::glGetQueryObjectuiv(rc.queryID, GL::QUERY_RESULT, &result);
                        
                        // Якщо семплів > 0 - чанк видно.
                        rc.visible = (result > 0);
                        rc.waitingForQuery = false; 
                    } 
                    // Якщо не доступний - продовжуємо використовувати попередній стан
                }
                
                if (!rc.visible) culledByOcclusion++;
            }
        } else {
            for(auto& rc : renderChunks) rc.visible = true;
        }

        float radYaw = yaw * 0.0174533f; float radPitch = pitch * 0.0174533f;
        Vec3 front = Normalize({ std::cos(radYaw)*std::cos(radPitch), std::sin(radPitch), std::sin(radYaw)*std::cos(radPitch) });
        Vec3 right = Normalize(Cross(front, {0,1,0}));
        const bool* keys = SDL_GetKeyboardState(nullptr);
        float speed = 0.5f; if(keys[SDL_SCANCODE_LSHIFT]) speed = 1.5f;
        if(keys[SDL_SCANCODE_W]) { camX += front.x*speed; camY += front.y*speed; camZ += front.z*speed; }
        if(keys[SDL_SCANCODE_S]) { camX -= front.x*speed; camY -= front.y*speed; camZ -= front.z*speed; }
        if(keys[SDL_SCANCODE_A]) { camX -= right.x*speed; camY -= right.y*speed; camZ -= right.z*speed; }
        if(keys[SDL_SCANCODE_D]) { camX += right.x*speed; camY += right.y*speed; camZ += right.z*speed; }

        int w, h; SDL_GetWindowSizeInPixels(window, &w, &h);
        Mat4 view = LookAt({camX, camY, camZ}, {camX+front.x, camY+front.y, camZ+front.z}, {0,1,0});
        Mat4 proj = Perspective(1.047f, (float)w/h, 0.1f, 1000.0f); 
        Mat4 vp = MultiplyMat4(proj, view);
        Frustum frustum = CreateFrustum(vp);
        
        // --- 2. Frustum Check ---
        visibleIndices.clear();
        for(size_t i=0; i<renderChunks.size(); ++i) {
            auto& rc = renderChunks[i];
            float minX = (float)rc.chunkX * CHUNK_SIZE;
            float minZ = (float)rc.chunkZ * CHUNK_SIZE;
            
            // Large padding (4.0) to prevent flickering at edges
            float pad = 4.0f; 
            bool inFrustum = FrustumCheckAABB(frustum, 
                minX - pad, -pad, minZ - pad, 
                minX + CHUNK_SIZE + pad, CHUNK_SIZE + pad, minZ + CHUNK_SIZE + pad
            );
            
            float distSq = (minX+16-camX)*(minX+16-camX) + (CHUNK_SIZE/2-camY)*(CHUNK_SIZE/2-camY) + (minZ+16-camZ)*(minZ+16-camZ);

            // "Nearby" check: Тільки чанк, в якому ми знаходимось (дуже малий радіус)
            bool isNearby = distSq < (CHUNK_SIZE * 0.7f * CHUNK_SIZE * 0.7f);

            // FIX FLICKERING:
            // Якщо чанк тільки що увійшов у фрустум - робимо його видимим миттєво.
            if (inFrustum && !rc.wasInFrustum) {
                rc.visible = true;
                rc.waitingForQuery = false; // Force a new query logic cycle
            }
            rc.wasInFrustum = inFrustum;

            if (isNearby) {
                rc.visible = true;
                rc.waitingForQuery = false;
            } else if (!inFrustum) {
                // Keep visible flag true so it's ready when we turn back,
                // but it won't be rendered because of 'inFrustum' check below.
                rc.visible = true; 
                rc.waitingForQuery = false;
            }

            if(inFrustum && rc.visible) {
                visibleIndices.push_back({i, distSq});
            }
        }

        std::sort(visibleIndices.begin(), visibleIndices.end(), [](const ChunkDist& a, const ChunkDist& b) {
            return a.distSq < b.distSq;
        });

        visibleCommands.clear();
        visibleChunkInfos.clear();
        for (const auto& item : visibleIndices) {
            const auto& rc = renderChunks[item.index];
            visibleCommands.push_back({6, (GLuint)rc.gpuQuads.size(), 0, 0, 0});
            visibleChunkInfos.push_back({rc.chunkX * CHUNK_SIZE, rc.chunkZ * CHUNK_SIZE, rc.globalBufferOffset, 0});
        }
        drawnChunks = visibleCommands.size();
        
        if (!visibleCommands.empty()) {
            GL::glBindBuffer(GL::SHADER_STORAGE_BUFFER, ssboChunks);
            GL::glBufferSubData(GL::SHADER_STORAGE_BUFFER, 0, visibleChunkInfos.size() * sizeof(GpuChunkInfo), visibleChunkInfos.data());
            GL::glBindBuffer(GL::DRAW_INDIRECT_BUFFER, indirectBuffer);
            GL::glBufferSubData(GL::DRAW_INDIRECT_BUFFER, 0, visibleCommands.size() * sizeof(GL::DrawElementsIndirectCommand), visibleCommands.data());
        }

        // --- 3. Render Geometry ---
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

        // --- 4. SMART Query Pass (OPTIMIZED) ---
        if(enableOcclusion && occlusionSupported) {
            GL::glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
            GL::glDepthMask(GL_FALSE);
            
            GL::glUseProgram(boxShader.id);
            GL::glBindVertexArray(cubeVAO);
            
            for(size_t i=0; i<renderChunks.size(); ++i) {
                ChunkRenderDataExtended& rc = renderChunks[i];
                
                if (rc.queryID == 0) continue;

                float cx = (float)rc.chunkX * CHUNK_SIZE;
                float cz = (float)rc.chunkZ * CHUNK_SIZE;
                
                // Пропускаємо перевірку для дуже близьких чанків
                float distSq = (cx+16-camX)*(cx+16-camX) + (CHUNK_SIZE/2-camY)*(CHUNK_SIZE/2-camY) + (cz+16-camZ)*(cz+16-camZ);
                if (distSq < (CHUNK_SIZE * 0.7f * CHUNK_SIZE * 0.7f)) continue;

                // Frustum Check для запиту
                bool inFrustum = FrustumCheckAABB(frustum, cx, 0, cz, cx+32, 32, cz+32);
                if (!inFrustum) continue;

                bool doQuery = false;
                
                // STRATEGY: Lazy Hiding / Eager Showing
                // 1. Якщо чанк ВИДИМИЙ - перевіряємо РІДКО (раз на 30 кадрів). 
                //    Ми хочемо заощадити FPS. Якщо він сховається, ми дізнаємось про це пізніше,
                //    але це краще, ніж гальмувати кожен кадр.
                // 2. Якщо чанк СХОВАНИЙ - перевіряємо ЧАСТО (кожні 3 кадри).
                //    Щоб він швидко з'явився, якщо стіна перед ним зникне.
                
                int interval = rc.visible ? 30 : 3;
                
                if ((frameCounter + i) % interval == 0) {
                    doQuery = true;
                }

                if (doQuery) {
                    Mat4 model = Identity();
                    model.m[0] = (float)CHUNK_SIZE; model.m[5] = (float)CHUNK_SIZE; model.m[10] = (float)CHUNK_SIZE;
                    model.m[12] = cx; model.m[13] = 0.0f; model.m[14] = cz;
                    Mat4 mvp = MultiplyMat4(vp, model);
                    GL::glUniformMatrix4fv(boxShader.loc_model, 1, GL_FALSE, mvp.m);

                    GL::glBeginQuery(GL_ANY_SAMPLES_PASSED_CONSERVATIVE, rc.queryID);
                    GL::glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
                    GL::glEndQuery(GL_ANY_SAMPLES_PASSED_CONSERVATIVE);
                    
                    rc.waitingForQuery = true;
                }
            }
            
            GL::glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            GL::glDepthMask(GL_TRUE);
        }

        SDL_GL_SwapWindow(window);
        
        acc += frameTimer.end();
        frames++;
        frameCounter++;
        
        if (acc >= 500.0) {
            double fps = (frames * 1000.0) / acc;
            std::stringstream ss;
            ss << "Voxel Engine | " << std::fixed << std::setprecision(1) << fps << " FPS | "
               << "Drawn: " << drawnChunks << " | Culled(Occ): " << culledByOcclusion;
            SDL_SetWindowTitle(window, ss.str().c_str());
            acc = 0; frames = 0;
        }
    }
    return 0;
}