#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <vector>
#include <iostream>
#include <ctime>
#include <cmath>

// Import Modules
import VoxelGame.Types;
import VoxelGame.Math;
import VoxelGame.GL;
import VoxelGame.Shader;
import VoxelGame.World;
import VoxelGame.Meshing;

// Using Declarations
using namespace VoxelGame::Types;
using namespace VoxelGame::Math;
using namespace VoxelGame::World;
using namespace VoxelGame::Meshing;
using namespace VoxelGame::Shader;

// Alias for our GL loader to avoid writing VoxelGame::GL:: everywhere
namespace GL = VoxelGame::GL;

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_Window* window = SDL_CreateWindow("Voxel Game - C++20 Modules", 1024, 768, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_GLContext context = SDL_GL_CreateContext(window);
    
    // Load OpenGL Functions (Namespace: GL)
    GL::LoadFunctions();
    
    bool mouseCaptured = true;
    if (!SDL_SetWindowRelativeMouseMode(window, true)) {
        std::cerr << "Warning: Relative Mouse Mode failed! " << SDL_GetError() << std::endl;
        mouseCaptured = false;
    }

    // World & Mesh Generation
    VoxelChunk myChunk;
    std::srand(std::time(nullptr));
    TerrainSystem::Generate(myChunk, std::rand() % 1000, 0, 0);

    std::vector<Quad> quads = GenerateQuads(myChunk);
    std::vector<RenderVertex> meshVertices = Triangulate(quads);

    std::cout << "Generated " << quads.size() << " quads, " << meshVertices.size() << " vertices." << std::endl;

    // OpenGL Buffers (Use GL:: for loaded functions)
    GLuint VAO, VBO;
    GL::glGenVertexArrays(1, &VAO);
    GL::glGenBuffers(1, &VBO);
    GL::glBindVertexArray(VAO);
    GL::glBindBuffer(GL_ARRAY_BUFFER, VBO);
    GL::glBufferData(GL_ARRAY_BUFFER, meshVertices.size() * sizeof(RenderVertex), meshVertices.data(), GL_STATIC_DRAW);

    GL::glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(RenderVertex), (void*)0);
    GL::glEnableVertexAttribArray(0);
    GL::glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(RenderVertex), (void*)(3 * sizeof(float)));
    GL::glEnableVertexAttribArray(1);
    GL::glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(RenderVertex), (void*)(6 * sizeof(float)));
    GL::glEnableVertexAttribArray(2);

    GLuint prog = CreateProgram("src/shaders/voxel.vert.glsl", "src/shaders/voxel.frag.glsl");
    GL::glUseProgram(prog);

    // Texture Setup
    GLuint texID;
    GL::glGenTextures(1, &texID); 
    GL::glBindTexture(GL_TEXTURE_2D, texID); 
    
    uint8_t texData[8 * 1 * 3] = {
        0, 0, 0,       // 0: Air
        0, 255, 0,     // 1: Grass
        139, 69, 19,   // 2: Dirt
        255, 255, 255, // 3: Snow
        50, 50, 50,    // 4: Internal
        100, 100, 255, // 5: Pillar
        255, 100, 100, // 6: Wall
        128, 128, 128  // 7: Slab
    };
    GL::glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 8, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, texData); 
    
    GL::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); 
    GL::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); 
    GL::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); 
    GL::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); 

    GL::glUniform1i(GL::glGetUniformLocation(prog, "u_texture"), 0);

    float camX = 16.0f, camY = 30.0f, camZ = 60.0f;
    float yaw = -90.0f, pitch = -30.0f;

    bool running = true;
    while(running) {
        SDL_Event ev;
        while(SDL_PollEvent(&ev)) {
            if(ev.type == SDL_EVENT_QUIT) running = false;
            
            if(ev.type == SDL_EVENT_KEY_DOWN) {
                if (ev.key.key == SDLK_ESCAPE) {
                    mouseCaptured = false;
                    SDL_SetWindowRelativeMouseMode(window, false);
                }
                if (ev.key.key == SDLK_R) {
                    std::cout << "Regenerating..." << std::endl;
                    TerrainSystem::Generate(myChunk, std::rand() % 1000, 0, 0);
                    quads = GenerateQuads(myChunk);
                    meshVertices = Triangulate(quads);
                    GL::glBufferData(GL_ARRAY_BUFFER, meshVertices.size() * sizeof(RenderVertex), meshVertices.data(), GL_STATIC_DRAW);
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
        
        Vec3 front = { 
            std::cos(radYaw)*std::cos(radPitch), 
            std::sin(radPitch), 
            std::sin(radYaw)*std::cos(radPitch) 
        };
        front = Normalize(front);
        Vec3 right = Normalize(Cross(front, {0,1,0}));

        const bool* keys = SDL_GetKeyboardState(nullptr);
        float speed = 0.5f;
        if(keys[SDL_SCANCODE_W]) { camX += front.x*speed; camY += front.y*speed; camZ += front.z*speed; }
        if(keys[SDL_SCANCODE_S]) { camX -= front.x*speed; camY -= front.y*speed; camZ -= front.z*speed; }
        if(keys[SDL_SCANCODE_A]) { camX -= right.x*speed; camY -= right.y*speed; camZ -= right.z*speed; }
        if(keys[SDL_SCANCODE_D]) { camX += right.x*speed; camY += right.y*speed; camZ += right.z*speed; }

        int w, h; 
        SDL_GetWindowSizeInPixels(window, &w, &h);
        glViewport(0,0,w,h);
        
        glClearColor(0.5f, 0.7f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);

        Mat4 model = Identity();
        Mat4 view = LookAt({camX, camY, camZ}, {camX+front.x, camY+front.y, camZ+front.z}, {0,1,0});
        Mat4 proj = Perspective(1.047f, (float)w/h, 0.1f, 1000.0f); 

        GL::glUniformMatrix4fv(GL::glGetUniformLocation(prog, "u_model"), 1, GL_FALSE, model.m);
        GL::glUniformMatrix4fv(GL::glGetUniformLocation(prog, "u_view"), 1, GL_FALSE, view.m);
        GL::glUniformMatrix4fv(GL::glGetUniformLocation(prog, "u_proj"), 1, GL_FALSE, proj.m);

        GL::glBindVertexArray(VAO);
        GL::glBindTexture(GL_TEXTURE_2D, texID);
        glDrawArrays(GL_TRIANGLES, 0, meshVertices.size());

        SDL_GL_SwapWindow(window);
    }

    return 0;
}