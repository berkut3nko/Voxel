#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <vector>
#include <iostream>
#include <ctime>
#include <cmath>

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

// Structure to hold mesh data per chunk
struct ChunkRenderData {
    std::vector<PackedVertex> vertices;
    int chunkX, chunkZ;
};

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_Window* window = SDL_CreateWindow("Voxel Game - Packed & Optimized", 1280, 720, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_GLContext context = SDL_GL_CreateContext(window);
    
    GL::LoadFunctions();
    
    bool mouseCaptured = true;
    if (!SDL_SetWindowRelativeMouseMode(window, true)) {
        mouseCaptured = false;
    }

    WorldManager world;
    int seed = std::time(nullptr) % 1000;
    
    // Generate Chunks
    for (int cx = -1; cx <= 1; ++cx) {
        for (int cz = -1; cz <= 1; ++cz) {
            VoxelChunk& chunk = world.createChunk(cx, cz);
            TerrainSystem::Generate(chunk, seed);
        }
    }

    // Mesh Chunks (Store separately now!)
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
            
            if(!verts.empty()) {
                renderChunks.push_back({verts, cx, cz});
            }
        }
    }

    std::cout << "Chunks to render: " << renderChunks.size() << std::endl;

    // Create Buffers for EACH chunk or one big buffer? 
    // Let's use one big buffer but keep offsets, OR simplest: Just re-upload everything for this demo.
    // For optimization, we will put ALL vertices into one VBO.
    
    std::vector<PackedVertex> allVertices;
    std::vector<int> chunkOffsets;
    std::vector<int> chunkCounts;
    // We also need to store the World Position for each chunk to pass as Uniform.
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

    // IMPORTANT: glVertexAttribIPointer for INTEGERS!
    // Loc 0: Pos (uint32)
    GL::glVertexAttribIPointer(0, 1, GL_UNSIGNED_INT, sizeof(PackedVertex), (void*)0);
    GL::glEnableVertexAttribArray(0);
    
    // Loc 1: Attr (uint32)
    GL::glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, sizeof(PackedVertex), (void*)(sizeof(uint32_t)));
    GL::glEnableVertexAttribArray(1);

    GLuint prog = CreateProgram("src/shaders/voxel.vert.glsl", "src/shaders/voxel.frag.glsl");
    GL::glUseProgram(prog);

    float camX = 16.0f, camY = 40.0f, camZ = 60.0f;
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
        float speed = 0.5f;
        if(keys[SDL_SCANCODE_LSHIFT]) speed = 1.5f;
        if(keys[SDL_SCANCODE_W]) { camX += front.x*speed; camY += front.y*speed; camZ += front.z*speed; }
        if(keys[SDL_SCANCODE_S]) { camX -= front.x*speed; camY -= front.y*speed; camZ -= front.z*speed; }
        if(keys[SDL_SCANCODE_A]) { camX -= right.x*speed; camY -= right.y*speed; camZ -= right.z*speed; }
        if(keys[SDL_SCANCODE_D]) { camX += right.x*speed; camY += right.y*speed; camZ += right.z*speed; }

        int w, h; 
        SDL_GetWindowSizeInPixels(window, &w, &h);
        glViewport(0,0,w,h);
        
        glClearColor(0.2f, 0.2f, 0.2f, 1.0f); 
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        // ENABLE CULL FACE NOW!
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);  // Відсікання задніх граней
        glCullFace(GL_BACK);     // Відсікаємо задні
        glFrontFace(GL_CCW);     // Проти годинникової стрілки - це перед

        Mat4 model = Identity();
        Mat4 view = LookAt({camX, camY, camZ}, {camX+front.x, camY+front.y, camZ+front.z}, {0,1,0});
        Mat4 proj = Perspective(1.047f, (float)w/h, 0.1f, 1000.0f); 

        // Model matrix is Identity now, we use u_chunkPos for translation
        GL::glUniformMatrix4fv(GL::glGetUniformLocation(prog, "u_model"), 1, GL_FALSE, model.m);
        GL::glUniformMatrix4fv(GL::glGetUniformLocation(prog, "u_view"), 1, GL_FALSE, view.m);
        GL::glUniformMatrix4fv(GL::glGetUniformLocation(prog, "u_proj"), 1, GL_FALSE, proj.m);

        GL::glBindVertexArray(VAO);
        
        GLint chunkPosLoc = GL::glGetUniformLocation(prog, "u_chunkPos");

        // Draw each chunk with its own offset
        for(const auto& cmd : drawCmds) {
            // Set uniform for this chunk
            // NOTE: glUniform3f is usually available, we might need to load it or use struct
            // For now, let's just hack it: We assume glUniform3f IS loaded or use glUniform4f/vec4 or modify loader.
            // Wait, we didn't load glUniform3f in GL.cppm! 
            // Let's create a quick translate matrix or just add the offset in shader via a vec3 uniform.
            // Since we didn't expose glUniform3f, let's assume we can add it or use a workaround.
            // Workaround: We can't easily pass vec3 without loading the function.
            // Let's modify GL.cppm to load glUniform3f really quick OR pass it via Model Matrix.
            
            // Method A: Update Model Matrix per chunk
            Mat4 chunkModel = Identity();
            chunkModel.m[12] = cmd.wx;
            chunkModel.m[13] = 0;
            chunkModel.m[14] = cmd.wz;
            GL::glUniformMatrix4fv(GL::glGetUniformLocation(prog, "u_model"), 1, GL_FALSE, chunkModel.m);
            
            // Reset u_chunkPos in shader to 0 (since we use matrix now)
            // Actually, shader adds u_chunkPos. Let's just rely on Model Matrix!
            // We need to change Shader to remove u_chunkPos or set it to 0. 
            // Since we can't easily set vec3 uniform without loading the function...
            // Let's check GL.cppm... NO glUniform3f.
            // OK, we will use the Model Matrix approach which we already have loaded.
            
            glDrawArrays(GL_TRIANGLES, cmd.start, cmd.count);
        }

        SDL_GL_SwapWindow(window);
    }
    return 0;
}