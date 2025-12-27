module;
#include <string>
#include <fstream>
#include <iostream>
#include <SDL3/SDL_opengl.h>
export module VoxelGame.Shader;

import VoxelGame.GL;

// Namespace alias for shorter code
namespace GL = VoxelGame::GL;

export namespace VoxelGame::Shader {
    
    std::string LoadFile(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) return "";
        return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    }

    GLuint CreateShader(const std::string& src, GLenum type) {
        // Use loaded pointers explicitly
        GLuint sh = GL::glCreateShader(type);
        const char* csrc = src.c_str();
        GL::glShaderSource(sh, 1, &csrc, nullptr);
        GL::glCompileShader(sh);
        
        GLint success;
        GL::glGetShaderiv(sh, GL_COMPILE_STATUS, &success);
        if(!success) {
            char infoLog[512];
            GL::glGetShaderInfoLog(sh, 512, nullptr, infoLog);
            std::cerr << "!!! SHADER ERROR (" << (type == GL_VERTEX_SHADER ? "VERT" : "FRAG") << ") !!!\n" << infoLog << std::endl;
        }
        return sh;
    }

    GLuint CreateProgram(const std::string& vertPath, const std::string& fragPath) {
        std::string vCode = LoadFile(vertPath);
        std::string fCode = LoadFile(fragPath);
        
        GLuint prog = GL::glCreateProgram();
        GLuint vs = CreateShader(vCode, GL_VERTEX_SHADER);
        GLuint fs = CreateShader(fCode, GL_FRAGMENT_SHADER);
        
        GL::glAttachShader(prog, vs);
        GL::glAttachShader(prog, fs);
        GL::glLinkProgram(prog);
        
        return prog;
    }
}