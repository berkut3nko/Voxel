module;
#include <vector>
#include <SDL3/SDL_opengl.h>
#include <cstdlib> // rand
export module VoxelGame.RenderUtils;

import VoxelGame.GL;
import VoxelGame.Meshing; 
import VoxelGame.Shader; 
import VoxelGame.Math;

namespace GL = VoxelGame::GL;
using namespace VoxelGame::Meshing; // GpuQuad
using namespace VoxelGame::Shader;  // ShaderProgram
using namespace VoxelGame::Math;    // Mat4

export namespace VoxelGame::RenderUtils {
    
    struct ChunkRenderData {
        std::vector<GpuQuad> gpuQuads;
        int chunkX, chunkZ;
        unsigned int globalBufferOffset; 
        
        // Для Occlusion Culling
        GLuint queryID;
        bool visible;     
        bool waitingForQuery; 
    };

    struct GpuChunkInfo {
        int x, z;
        unsigned int quadStart;
        int pad;
    };

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

    ShaderProgram CreateBoxShader() {
        const char* vs = R"(
            #version 450 core
            layout(location=0) in vec3 aPos;
            uniform mat4 u_mvp;
            void main() { gl_Position = u_mvp * vec4(aPos, 1.0); }
        )";
        const char* fs = R"(
            #version 450 core
            out vec4 Color;
            void main() { Color = vec4(1.0); } 
        )";
        
        GLuint p = GL::glCreateProgram();
        GLuint v = GL::glCreateShader(GL_VERTEX_SHADER);
        GLuint f = GL::glCreateShader(GL_FRAGMENT_SHADER);
        GL::glShaderSource(v, 1, &vs, nullptr); GL::glCompileShader(v);
        GL::glShaderSource(f, 1, &fs, nullptr); GL::glCompileShader(f);
        GL::glAttachShader(p, v); GL::glAttachShader(p, f);
        GL::glLinkProgram(p);
        
        ShaderProgram sp;
        sp.id = p;
        sp.loc_model = GL::glGetUniformLocation(p, "u_mvp"); 
        return sp;
    }

    GLuint CreateCubeVAO() {
        float vertices[] = {
            0,0,0, 1,0,0, 1,1,0, 0,1,0, 0,0,1, 1,0,1, 1,1,1, 0,1,1
        };
        unsigned int indices[] = {
            0,1,2, 2,3,0, 4,5,6, 6,7,4, 4,5,1, 1,0,4, 
            3,2,6, 6,7,3, 1,5,6, 6,2,1, 4,0,3, 3,7,4
        };
        
        GLuint vao, vbo, ibo;
        GL::glGenVertexArrays(1, &vao);
        GL::glBindVertexArray(vao);
        
        GL::glGenBuffers(1, &vbo);
        GL::glBindBuffer(0x8892, vbo); 
        GL::glBufferData(0x8892, sizeof(vertices), vertices, GL_STATIC_DRAW);
        
        GL::glGenBuffers(1, &ibo);
        GL::glBindBuffer(0x8893, ibo); 
        GL::glBufferData(0x8893, sizeof(indices), indices, GL_STATIC_DRAW);
        
        GL::glEnableVertexAttribArray(0);
        GL::glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*)0);
        
        GL::glBindVertexArray(0);
        return vao;
    }
}