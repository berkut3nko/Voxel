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

import VoxelGame.Types;
import VoxelGame.Math;
import VoxelGame.GL;
import VoxelGame.Shader;
import VoxelGame.World;
import VoxelGame.Meshing;

using namespace VoxelGame::Types;
using namespace VoxelGame::Math;
using namespace VoxelGame::World;
using namespace VoxelGame::Meshing;
using namespace VoxelGame::Shader;

namespace GL = VoxelGame::GL;

struct ChunkRenderData {
    std::vector<GpuQuad> gpuQuads;
    int chunkX, chunkZ;
};

struct GpuChunkInfo {
    int x, z;
    unsigned int quadStart;
    int pad;
};

// --- Palette Generation Helper ---
GLuint CreatePaletteTextureArray() {
    GLuint texID;
    GL::glGenTextures(1, &texID);
    GL::glBindTexture(GL_TEXTURE_2D_ARRAY, texID);
    int w=16, h=16, l=8;
    GL::glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGB8, w, h, l, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    auto Fill = [&](int la, uint8_t r, uint8_t g, uint8_t b) {
        std::vector<uint8_t> d(w*h*3);
        for(int i=0; i<w*h; ++i) { 
            uint8_t n = rand()%40; 
            d[i*3]=r>n?r-n:0; d[i*3+1]=g>n?g-n:0; d[i*3+2]=b>n?b-n:0; 
        }
        GL::glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, la, w, h, 1, GL_RGB, GL_UNSIGNED_BYTE, d.data());
    };
    Fill(0,50,200,50); Fill(1,139,69,19); Fill(2,240,240,255); Fill(3,50,50,50);
    Fill(4,100,100,255); Fill(5,200,80,80); Fill(6,128,128,128);
    GL::glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    GL::glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    GL::glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
    GL::glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
    return texID;
}

// Виправлений профайлер з високою точністю
struct Profiler {
    using Clock = std::chrono::high_resolution_clock;
    std::chrono::time_point<Clock> start;
    void begin() { start = Clock::now(); }
    double end() { 
        auto dur = std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - start);
        return dur.count() / 1000.0; 
    }
};

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    
    // OpenGL 4.5
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    
    SDL_Window* window = SDL_CreateWindow("Voxel Game - MultiDrawElements", 1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_GLContext context = SDL_GL_CreateContext(window);
    
    // VSync OFF для тесту продуктивності
    SDL_GL_SetSwapInterval(0);
    
    GL::LoadFunctions();
    
    bool mouseCaptured = true;
    SDL_SetWindowRelativeMouseMode(window, true);

    WorldManager world;
    int seed = std::time(nullptr) % 1000;
    int range = 1;
    
    std::cout << "Generating chunks..." << std::endl;
    for (int cx = -range; cx <= range; ++cx) {
        for (int cz = -range; cz <= range; ++cz) {
            VoxelChunk& chunk = world.createChunk(cx, cz);
            TerrainSystem::Generate(chunk, seed);
        }
    }

    std::cout << "Meshing chunks..." << std::endl;
    std::vector<ChunkRenderData> renderChunks;
    for (int cx = -range; cx <= range; ++cx) {
        for (int cz = -range; cz <= range; ++cz) {
            VoxelChunk* chunk = world.getChunk(cx, cz);
            if(!chunk) continue;
            MeshingContext ctx;
            ctx.center = chunk;
            ctx.neighbors[0] = world.getChunk(cx + 1, cz);
            ctx.neighbors[1] = world.getChunk(cx - 1, cz);
            ctx.neighbors[2] = nullptr;
            ctx.neighbors[3] = nullptr;
            ctx.neighbors[4] = world.getChunk(cx, cz + 1);
            ctx.neighbors[5] = world.getChunk(cx, cz - 1);

            std::vector<Quad> quads = GenerateQuads(ctx);
            std::vector<GpuQuad> gpuQ = BuildSSBOData(quads);
            if(!gpuQ.empty()) renderChunks.push_back({gpuQ, cx, cz});
        }
    }
    
    if (renderChunks.empty()) {
        std::cerr << "WARNING: No geometry generated!" << std::endl;
    }

    std::vector<GpuQuad> allGpuQuads;
    std::vector<GL::DrawElementsIndirectCommand> commands;
    std::vector<GpuChunkInfo> chunkInfos;

    for(size_t i=0; i<renderChunks.size(); ++i) {
        const auto& rc = renderChunks[i];
        
        GL::DrawElementsIndirectCommand cmd;
        cmd.count = 6; 
        cmd.instanceCount = rc.gpuQuads.size(); 
        cmd.firstIndex = 0;
        cmd.baseVertex = 0;
        cmd.baseInstance = i; 
        
        commands.push_back(cmd);
        
        chunkInfos.push_back({
            rc.chunkX * CHUNK_SIZE, 
            rc.chunkZ * CHUNK_SIZE, 
            (unsigned int)allGpuQuads.size(), 
            0
        });

        allGpuQuads.insert(allGpuQuads.end(), rc.gpuQuads.begin(), rc.gpuQuads.end());
    }

    // 1. VAO & IBO setup
    GLuint emptyVAO;
    GL::glGenVertexArrays(1, &emptyVAO);
    GL::glBindVertexArray(emptyVAO); // Bind VAO first!

    // Create Static IBO (0,1,2, 0,2,3)
    GLuint indices[] = {0, 1, 2, 0, 2, 3};
    GLuint ibo;
    GL::glGenBuffers(1, &ibo);
    GL::glBindBuffer(0x8893, ibo); // GL_ELEMENT_ARRAY_BUFFER to CURRENT VAO
    GL::glBufferData(0x8893, sizeof(indices), indices, GL_STATIC_DRAW);
    
    // Unbind VAO to prevents accidental modification
    GL::glBindVertexArray(0);

    // 2. SSBO Buffers
    GLuint ssboQuads, ssboChunks, indirectBuffer;
    GL::glGenBuffers(1, &ssboQuads);
    GL::glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboQuads);
    GL::glBufferData(GL_SHADER_STORAGE_BUFFER, allGpuQuads.size() * sizeof(GpuQuad), allGpuQuads.data(), GL_STATIC_DRAW);
    GL::glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssboQuads);

    GL::glGenBuffers(1, &ssboChunks);
    GL::glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboChunks);
    GL::glBufferData(GL_SHADER_STORAGE_BUFFER, chunkInfos.size() * sizeof(GpuChunkInfo), chunkInfos.data(), GL_STATIC_DRAW);
    GL::glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssboChunks);

    GL::glGenBuffers(1, &indirectBuffer);
    GL::glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectBuffer);
    GL::glBufferData(GL_DRAW_INDIRECT_BUFFER, commands.size() * sizeof(GL::DrawElementsIndirectCommand), commands.data(), GL_STATIC_DRAW);

    ShaderProgram shader = CreateProgram("src/shaders/voxel.vert.glsl", "src/shaders/voxel.frag.glsl");
    
    // !!! CRITICAL CHECK !!!
    if (shader.id == 0) {
        std::cerr << "FATAL: Shader compilation failed. Exiting." << std::endl;
        return -1;
    }
    
    GL::glUseProgram(shader.id);

    GLuint texArrayID = CreatePaletteTextureArray();
    glActiveTexture(GL_TEXTURE0);
    GL::glBindTexture(GL_TEXTURE_2D_ARRAY, texArrayID);
    GL::glUniform1i(shader.loc_textureArray, 0);

    float camX = 0.0f, camY = 40.0f, camZ = 0.0f;
    float yaw = -90.0f, pitch = -30.0f;
    bool showGrid = false;

    Profiler frameTimer, logicTimer, renderTimer;
    double logicTimeMs=0, renderTimeMs=0, acc=0;
    int frames=0;

    bool running = true;
    while(running) {
        frameTimer.begin();
        logicTimer.begin();
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
        Vec3 front = { std::cos(radYaw)*std::cos(radPitch), std::sin(radPitch), std::sin(radYaw)*std::cos(radPitch) };
        front = Normalize(front);
        Vec3 right = Normalize(Cross(front, {0,1,0}));
        const bool* keys = SDL_GetKeyboardState(nullptr);
        float speed = 0.5f;
        if(keys[SDL_SCANCODE_LSHIFT]) speed = 1.5f;
        if(keys[SDL_SCANCODE_W]) { camX += front.x*speed; camY += front.y*speed; camZ += front.z*speed; }
        if(keys[SDL_SCANCODE_S]) { camX -= front.x*speed; camY -= front.y*speed; camZ -= front.z*speed; }
        if(keys[SDL_SCANCODE_A]) { camX -= right.x*speed; camY -= right.y*speed; camZ -= right.z*speed; }
        if(keys[SDL_SCANCODE_D]) { camX += right.x*speed; camY += right.y*speed; camZ += right.z*speed; }
        logicTimeMs = logicTimer.end();

        renderTimer.begin();
        int w, h; SDL_GetWindowSizeInPixels(window, &w, &h); glViewport(0,0,w,h);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f); // Темний фон, щоб бачити, чи працює очистка
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        glEnable(GL_DEPTH_TEST); 
        glEnable(GL_CULL_FACE); 
        glCullFace(GL_BACK); 
        glFrontFace(GL_CCW);

        Mat4 view = LookAt({camX, camY, camZ}, {camX+front.x, camY+front.y, camZ+front.z}, {0,1,0});
        Mat4 proj = Perspective(1.047f, (float)w/h, 0.1f, 1000.0f); 

        GL::glUniformMatrix4fv(shader.loc_view, 1, GL_FALSE, view.m);
        GL::glUniformMatrix4fv(shader.loc_proj, 1, GL_FALSE, proj.m);
        GL::glUniform1i(shader.loc_showGrid, showGrid ? 1 : 0);

        GL::glBindVertexArray(emptyVAO); // VAO вже має IBO
        GL::glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectBuffer);
        
        GL::glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, 0, (GLsizei)commands.size(), 0);

        SDL_GL_SwapWindow(window);
        renderTimeMs = renderTimer.end();
        
        double currentFrameTime = frameTimer.end();
        acc += currentFrameTime;
        frames++;
        
        if (acc >= 500.0) {
            double avg = acc / frames;
            // Уникаємо ділення на нуль
            double fps = (avg > 0.0001) ? 1000.0 / avg : 9999.0;
            
            std::stringstream ss;
            ss << "Voxel Engine | " << std::fixed << std::setprecision(1) << fps << " FPS | "
               << std::setprecision(3) << avg << "ms | "
               << "L:" << logicTimeMs << " R:" << renderTimeMs;
            SDL_SetWindowTitle(window, ss.str().c_str());
            acc = 0; frames = 0;
        }
    }
    return 0;
}