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
    
    // Структура для кешування локацій шейдера
    struct ShaderProgram {
        GLuint id;
        
        // Uniform Locations
        GLint loc_model;
        GLint loc_view;
        GLint loc_proj;
        GLint loc_textureArray;
        GLint loc_showGrid;
    };

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

    // Оновлена функція повертає структуру з кешованими локаціями
    ShaderProgram CreateProgram(const std::string& vertPath, const std::string& fragPath) {
        std::string vCode = LoadFile(vertPath);
        std::string fCode = LoadFile(fragPath);
        
        GLuint progID = GL::glCreateProgram();
        GLuint vs = CreateShader(vCode, GL_VERTEX_SHADER);
        GLuint fs = CreateShader(fCode, GL_FRAGMENT_SHADER);
        
        GL::glAttachShader(progID, vs);
        GL::glAttachShader(progID, fs);
        GL::glLinkProgram(progID);
        
        // Кешування локацій
        ShaderProgram prog;
        prog.id = progID;
        prog.loc_model = GL::glGetUniformLocation(progID, "u_model");
        prog.loc_view = GL::glGetUniformLocation(progID, "u_view");
        prog.loc_proj = GL::glGetUniformLocation(progID, "u_proj");
        prog.loc_textureArray = GL::glGetUniformLocation(progID, "u_textureArray");
        prog.loc_showGrid = GL::glGetUniformLocation(progID, "u_showGrid");

        return prog;
    }
}