#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <vector>
#include <iostream>
#include <ctime>
#include <cmath>
#include <chrono> // Для профайлера
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
    std::vector<PackedVertex> vertices;
    int chunkX, chunkZ;
};

// --- Palette Generation Helper ---
GLuint CreatePaletteTextureArray() {
    GLuint texID;
    GL::glGenTextures(1, &texID);
    GL::glBindTexture(GL_TEXTURE_2D_ARRAY, texID);

    int width = 16;
    int height = 16;
    int layers = 8; 

    GL::glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGB8, width, height, layers, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);

    auto FillLayer = [&](int layer, uint8_t r, uint8_t g, uint8_t b) {
        std::vector<uint8_t> data(width * height * 3);
        for (int i = 0; i < width * height; ++i) {
            uint8_t noise = (rand() % 40); 
            data[i*3+0] = (r > noise) ? r - noise : 0;
            data[i*3+1] = (g > noise) ? g - noise : 0;
            data[i*3+2] = (b > noise) ? b - noise : 0;
        }
        GL::glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, layer, width, height, 1, GL_RGB, GL_UNSIGNED_BYTE, data.data());
    };

    FillLayer(0, 50, 200, 50);
    FillLayer(1, 139, 69, 19);
    FillLayer(2, 240, 240, 255);
    FillLayer(3, 50, 50, 50);
    FillLayer(4, 100, 100, 255);
    FillLayer(5, 200, 80, 80);
    FillLayer(6, 128, 128, 128);

    GL::glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    GL::glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    GL::glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
    GL::glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);

    return texID;
}

// Простий клас таймера
struct Profiler {
    using Clock = std::chrono::high_resolution_clock;
    std::chrono::time_point<Clock> start;
    
    void begin() { start = Clock::now(); }
    
    double end() { // Повертає мс
        auto now = Clock::now();
        std::chrono::duration<double, std::milli> diff = now - start;
        return diff.count();
    }
};

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_Window* window = SDL_CreateWindow("Voxel Game", 1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_GLContext context = SDL_GL_CreateContext(window);
    // Вимикаємо V-Sync для тестування максимального FPS
    SDL_GL_SetSwapInterval(0); 
    
    GL::LoadFunctions();
    
    bool mouseCaptured = true;
    if (!SDL_SetWindowRelativeMouseMode(window, true)) {
        mouseCaptured = false;
    }

    WorldManager world;
    int seed = std::time(nullptr) % 1000;
    
    // --- Початкова генерація ---
    for (int cx = -1; cx <= 1; ++cx) {
        for (int cz = -1; cz <= 1; ++cz) {
            VoxelChunk& chunk = world.createChunk(cx, cz);
            TerrainSystem::Generate(chunk, seed);
        }
    }

    std::vector<ChunkRenderData> renderChunks;
    for (int cx = -1; cx <= 1; ++cx) {
        for (int cz = -1; cz <= 1; ++cz) {
            VoxelChunk* chunk = world.getChunk(cx, cz);
            MeshingContext ctx;
            ctx.center = chunk;
            ctx.neighbors[0] = world.getChunk(cx + 1, cz);
            ctx.neighbors[1] = world.getChunk(cx - 1, cz);
            ctx.neighbors[2] = nullptr;
            ctx.neighbors[3] = nullptr;
            ctx.neighbors[4] = world.getChunk(cx, cz + 1);
            ctx.neighbors[5] = world.getChunk(cx, cz - 1);

            std::vector<Quad> quads = GenerateQuads(ctx);
            std::vector<PackedVertex> verts = Triangulate(quads);
            if(!verts.empty()) renderChunks.push_back({verts, cx, cz});
        }
    }

    std::vector<PackedVertex> allVertices;
    struct DrawCmd { int start; int count; float wx; float wz; };
    std::vector<DrawCmd> drawCmds;

    for(const auto& rc : renderChunks) {
        DrawCmd cmd;
        cmd.start = allVertices.size();
        cmd.count = rc.vertices.size();
        cmd.wx = (float)(rc.chunkX * CHUNK_SIZE);
        cmd.wz = (float)(rc.chunkZ * CHUNK_SIZE);
        
        allVertices.insert(allVertices.end(), rc.vertices.begin(), rc.vertices.end());
        drawCmds.push_back(cmd);
    }

    GLuint VAO, VBO;
    GL::glGenVertexArrays(1, &VAO);
    GL::glGenBuffers(1, &VBO);
    GL::glBindVertexArray(VAO);
    GL::glBindBuffer(GL_ARRAY_BUFFER, VBO);
    GL::glBufferData(GL_ARRAY_BUFFER, allVertices.size() * sizeof(PackedVertex), allVertices.data(), GL_STATIC_DRAW);

    GLsizei stride = sizeof(PackedVertex); // 12 bytes
    
    // Loc 0: PackedPos (uint) -> Offset 0
    GL::glVertexAttribIPointer(0, 1, GL_UNSIGNED_INT, stride, (void*)0);
    GL::glEnableVertexAttribArray(0);
    
    // Loc 1: PackedAttr (uint) -> Offset 4
    GL::glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, stride, (void*)(sizeof(uint32_t)));
    GL::glEnableVertexAttribArray(1);

    // Loc 2: PackedUV (uint) -> Offset 8
    GL::glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT, stride, (void*)(2 * sizeof(uint32_t)));
    GL::glEnableVertexAttribArray(2);

    // Тепер це структура ShaderProgram
    ShaderProgram shader = CreateProgram("src/shaders/voxel.vert.glsl", "src/shaders/voxel.frag.glsl");
    GL::glUseProgram(shader.id);

    GLuint texArrayID = CreatePaletteTextureArray();
    glActiveTexture(GL_TEXTURE0);
    GL::glBindTexture(GL_TEXTURE_2D_ARRAY, texArrayID);
    // Використовуємо кешовану локацію
    GL::glUniform1i(shader.loc_textureArray, 0);

    float camX = 16.0f, camY = 40.0f, camZ = 60.0f;
    float yaw = -90.0f, pitch = -30.0f;
    bool showGrid = false; 

    // --- Profiler Variables ---
    Profiler frameTimer, logicTimer, renderTimer;
    double logicTimeMs = 0.0;
    double renderTimeMs = 0.0;
    double totalFrameTimeMs = 0.0;
    int frameCount = 0;
    double timeAccumulator = 0.0; // Для оновлення заголовка раз на 1с

    bool running = true;
    while(running) {
        frameTimer.begin(); // START FRAME

        // --- 1. Input & Logic ---
        logicTimer.begin();
        SDL_Event ev;
        while(SDL_PollEvent(&ev)) {
            if(ev.type == SDL_EVENT_QUIT) running = false;
            
            if(ev.type == SDL_EVENT_KEY_DOWN) {
                if (ev.key.key == SDLK_ESCAPE) {
                    mouseCaptured = false;
                    SDL_SetWindowRelativeMouseMode(window, false);
                }
                if (ev.key.key == SDLK_G) { 
                    showGrid = !showGrid;
                }
                // Regenerate on R (Demo logic overhead)
                if (ev.key.key == SDLK_R) {
                    // Simple rebuild logic (can be slow, but useful to profile)
                    // ... (re-generate code omitted for brevity as it's static in loop for now) ...
                }
            }
            if(ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN && !mouseCaptured) {
                mouseCaptured = true;
                SDL_SetWindowRelativeMouseMode(window, true);
            }
            if(ev.type == SDL_EVENT_MOUSE_MOTION && mouseCaptured) {
                yaw += ev.motion.xrel * 0.1f;
                pitch -= ev.motion.yrel * 0.1f;
                if(pitch > 89.0f) pitch = 89.0f;
                if(pitch < -89.0f) pitch = -89.0f;
            }
        }

        float radYaw = yaw * 0.0174533f;
        float radPitch = pitch * 0.0174533f;
        Vec3 front = { std::cos(radYaw)*std::cos(radPitch), std::sin(radPitch), std::sin(radYaw)*std::cos(radPitch) };
        front = Normalize(front);
        Vec3 right = Normalize(Cross(front, {0,1,0}));

        const bool* keys = SDL_GetKeyboardState(nullptr);
        float speed = 0.5f; // Per frame speed (should use delta time)
        if(keys[SDL_SCANCODE_LSHIFT]) speed = 1.5f;
        if(keys[SDL_SCANCODE_W]) { camX += front.x*speed; camY += front.y*speed; camZ += front.z*speed; }
        if(keys[SDL_SCANCODE_S]) { camX -= front.x*speed; camY -= front.y*speed; camZ -= front.z*speed; }
        if(keys[SDL_SCANCODE_A]) { camX -= right.x*speed; camY -= right.y*speed; camZ -= right.z*speed; }
        if(keys[SDL_SCANCODE_D]) { camX += right.x*speed; camY += right.y*speed; camZ += right.z*speed; }
        
        logicTimeMs = logicTimer.end();

        // --- 2. Render ---
        renderTimer.begin();
        
        int w, h; 
        SDL_GetWindowSizeInPixels(window, &w, &h);
        glViewport(0,0,w,h);
        
        glClearColor(0.5f, 0.7f, 1.0f, 1.0f); 
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);  
        glCullFace(GL_BACK);     
        glFrontFace(GL_CCW);     

        Mat4 model = Identity();
        Mat4 view = LookAt({camX, camY, camZ}, {camX+front.x, camY+front.y, camZ+front.z}, {0,1,0});
        Mat4 proj = Perspective(1.047f, (float)w/h, 0.1f, 1000.0f); 

        // Використовуємо кешовані локації
        GL::glUniformMatrix4fv(shader.loc_model, 1, GL_FALSE, model.m);
        GL::glUniformMatrix4fv(shader.loc_view, 1, GL_FALSE, view.m);
        GL::glUniformMatrix4fv(shader.loc_proj, 1, GL_FALSE, proj.m);
        GL::glUniform1i(shader.loc_showGrid, showGrid ? 1 : 0);

        GL::glBindVertexArray(VAO);
        glActiveTexture(GL_TEXTURE0);
        GL::glBindTexture(GL_TEXTURE_2D_ARRAY, texArrayID);

        for(const auto& cmd : drawCmds) {
            Mat4 chunkModel = Identity();
            chunkModel.m[12] = cmd.wx;
            chunkModel.m[13] = 0;
            chunkModel.m[14] = cmd.wz;
            GL::glUniformMatrix4fv(shader.loc_model, 1, GL_FALSE, chunkModel.m);
            
            glDrawArrays(GL_TRIANGLES, cmd.start, cmd.count);
        }

        SDL_GL_SwapWindow(window); // SwapBuffers is part of render/present time
        renderTimeMs = renderTimer.end();

        totalFrameTimeMs = frameTimer.end(); // END FRAME

        // --- 3. Profiler Title Update (Every ~500ms) ---
        timeAccumulator += totalFrameTimeMs;
        frameCount++;
        
        if (timeAccumulator >= 500.0) {
            double avgFrameTime = timeAccumulator / frameCount;
            double fps = 1000.0 / avgFrameTime;
            
            std::stringstream ss;
            ss << "Voxel Engine | "
               << std::fixed << std::setprecision(1) << fps << " FPS | "
               << std::setprecision(2) << avgFrameTime << "ms (Total) | "
               << "Logic: " << logicTimeMs << "ms | "
               << "Render: " << renderTimeMs << "ms";
               
            SDL_SetWindowTitle(window, ss.str().c_str());
            
            timeAccumulator = 0.0;
            frameCount = 0;
        }
    }
    return 0;
}