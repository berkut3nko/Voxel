module;
#include <string>
#include <fstream>
#include <iostream>
#include <SDL3/SDL_opengl.h>
export module VoxelGame.Shader;

import VoxelGame.GL;

namespace GL = VoxelGame::GL;

export namespace VoxelGame::Shader {
    
    struct ShaderProgram {
        GLuint id;
        
        GLint loc_model;
        GLint loc_view;
        GLint loc_proj;
        GLint loc_textureArray;
        GLint loc_showGrid;
        
        // Compute specifics
        GLint loc_cull_viewProj;
        GLint loc_cull_camPos;
        GLint loc_cull_chunkCount;
        GLint loc_cull_frustumPlanes;
    };

    std::string LoadFile(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) return "";
        return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    }

    GLuint CreateShader(const std::string& src, GLenum type) {
        GLuint sh = GL::glCreateShader(type);
        const char* csrc = src.c_str();
        GL::glShaderSource(sh, 1, &csrc, nullptr);
        GL::glCompileShader(sh);
        
        GLint success;
        GL::glGetShaderiv(sh, GL_COMPILE_STATUS, &success);
        if(!success) {
            char infoLog[512];
            GL::glGetShaderInfoLog(sh, 512, nullptr, infoLog);
            std::cerr << "!!! SHADER ERROR (" << (type == GL_VERTEX_SHADER ? "VERT" : (type == GL_FRAGMENT_SHADER ? "FRAG" : "COMP")) << ") !!!\n" << infoLog << std::endl;
        }
        return sh;
    }

    ShaderProgram CreateProgram(const std::string& vertPath, const std::string& fragPath) {
        std::string vCode = LoadFile(vertPath);
        std::string fCode = LoadFile(fragPath);
        
        GLuint progID = GL::glCreateProgram();
        GLuint vs = CreateShader(vCode, GL_VERTEX_SHADER);
        GLuint fs = CreateShader(fCode, GL_FRAGMENT_SHADER);
        
        GL::glAttachShader(progID, vs);
        GL::glAttachShader(progID, fs);
        GL::glLinkProgram(progID);
        
        ShaderProgram prog;
        prog.id = progID;
        prog.loc_view = GL::glGetUniformLocation(progID, "u_view");
        prog.loc_proj = GL::glGetUniformLocation(progID, "u_proj");
        prog.loc_textureArray = GL::glGetUniformLocation(progID, "u_textureArray");
        prog.loc_showGrid = GL::glGetUniformLocation(progID, "u_showGrid");

        return prog;
    }

    // New: Compute Shader support
    ShaderProgram CreateComputeProgram(const std::string& compPath) {
        std::string cCode = LoadFile(compPath);
        
        GLuint progID = GL::glCreateProgram();
        GLuint cs = CreateShader(cCode, 0x91B9); // GL_COMPUTE_SHADER
        
        GL::glAttachShader(progID, cs);
        GL::glLinkProgram(progID);
        
        GLint success;
        GL::glGetProgramiv(progID, GL_LINK_STATUS, &success);
        if(!success) {
             char infoLog[512];
             GL::glGetProgramInfoLog(progID, 512, nullptr, infoLog);
             std::cerr << "!!! COMPUTE LINK ERROR !!!\n" << infoLog << std::endl;
        }

        ShaderProgram prog;
        prog.id = progID;
        prog.loc_cull_viewProj = GL::glGetUniformLocation(progID, "u_viewProj");
        prog.loc_cull_camPos   = GL::glGetUniformLocation(progID, "u_camPos");
        prog.loc_cull_chunkCount = GL::glGetUniformLocation(progID, "u_chunkCount");
        prog.loc_cull_frustumPlanes = GL::glGetUniformLocation(progID, "u_frustumPlanes");
        
        return prog;
    }
}