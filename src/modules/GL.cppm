module;
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
export module VoxelGame.GL;

export namespace VoxelGame::GL {
    // Defines types inside namespace to avoid collision with standard headers
    typedef void (APIENTRY *PFN_GLGENVERTEXARRAYS) (GLsizei n, GLuint *arrays);
    typedef void (APIENTRY *PFN_GLBINDVERTEXARRAY) (GLuint array);
    typedef void (APIENTRY *PFN_GLGENBUFFERS) (GLsizei n, GLuint *buffers);
    typedef void (APIENTRY *PFN_GLBINDBUFFER) (GLenum target, GLuint buffer);
    typedef void (APIENTRY *PFN_GLBUFFERDATA) (GLenum target, GLsizeiptr size, const void *data, GLenum usage);
    typedef void (APIENTRY *PFN_GLENABLEVERTEXATTRIBARRAY) (GLuint index);
    typedef void (APIENTRY *PFN_GLVERTEXATTRIBPOINTER) (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer);
    typedef GLuint (APIENTRY *PFN_GLCREATESHADER) (GLenum type);
    typedef void (APIENTRY *PFN_GLSHADERSOURCE) (GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length);
    typedef void (APIENTRY *PFN_GLCOMPILESHADER) (GLuint shader);
    typedef void (APIENTRY *PFN_GLGETSHADERIV) (GLuint shader, GLenum pname, GLint *params);
    typedef void (APIENTRY *PFN_GLGETSHADERINFOLOG) (GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
    typedef GLuint (APIENTRY *PFN_GLCREATEPROGRAM) (void);
    typedef void (APIENTRY *PFN_GLATTACHSHADER) (GLuint program, GLuint shader);
    typedef void (APIENTRY *PFN_GLLINKPROGRAM) (GLuint program);
    typedef void (APIENTRY *PFN_GLGETPROGRAMIV) (GLuint program, GLenum pname, GLint *params);
    typedef void (APIENTRY *PFN_GLGETPROGRAMINFOLOG) (GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
    typedef void (APIENTRY *PFN_GLUSEPROGRAM) (GLuint program);
    typedef GLint (APIENTRY *PFN_GLGETUNIFORMLOCATION) (GLuint program, const GLchar *name);
    typedef void (APIENTRY *PFN_GLUNIFORM1I) (GLint location, GLint v0);
    typedef void (APIENTRY *PFN_GLUNIFORMMATRIX4FV) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
    
    // Textures (GL 1.1 names, but loaded dynamically)
    typedef void (APIENTRY *PFN_GLGENTEXTURES) (GLsizei n, GLuint *textures);
    typedef void (APIENTRY *PFN_GLBINDTEXTURE) (GLenum target, GLuint texture);
    typedef void (APIENTRY *PFN_GLTEXIMAGE2D) (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels);
    typedef void (APIENTRY *PFN_GLTEXPARAMETERI) (GLenum target, GLenum pname, GLint param);

    // Function Pointers
    PFN_GLGENVERTEXARRAYS glGenVertexArrays;
    PFN_GLBINDVERTEXARRAY glBindVertexArray;
    PFN_GLGENBUFFERS glGenBuffers;
    PFN_GLBINDBUFFER glBindBuffer;
    PFN_GLBUFFERDATA glBufferData;
    PFN_GLENABLEVERTEXATTRIBARRAY glEnableVertexAttribArray;
    PFN_GLVERTEXATTRIBPOINTER glVertexAttribPointer;
    PFN_GLCREATESHADER glCreateShader;
    PFN_GLSHADERSOURCE glShaderSource;
    PFN_GLCOMPILESHADER glCompileShader;
    PFN_GLGETSHADERIV glGetShaderiv;
    PFN_GLGETSHADERINFOLOG glGetShaderInfoLog;
    PFN_GLCREATEPROGRAM glCreateProgram;
    PFN_GLATTACHSHADER glAttachShader;
    PFN_GLLINKPROGRAM glLinkProgram;
    PFN_GLGETPROGRAMIV glGetProgramiv;
    PFN_GLGETPROGRAMINFOLOG glGetProgramInfoLog;
    PFN_GLUSEPROGRAM glUseProgram;
    PFN_GLGETUNIFORMLOCATION glGetUniformLocation;
    PFN_GLUNIFORM1I glUniform1i;
    PFN_GLUNIFORMMATRIX4FV glUniformMatrix4fv;
    PFN_GLGENTEXTURES glGenTextures;
    PFN_GLBINDTEXTURE glBindTexture;
    PFN_GLTEXIMAGE2D glTexImage2D;
    PFN_GLTEXPARAMETERI glTexParameteri;

    void LoadFunctions() {
        glGenVertexArrays = (PFN_GLGENVERTEXARRAYS)SDL_GL_GetProcAddress("glGenVertexArrays");
        glBindVertexArray = (PFN_GLBINDVERTEXARRAY)SDL_GL_GetProcAddress("glBindVertexArray");
        glGenBuffers = (PFN_GLGENBUFFERS)SDL_GL_GetProcAddress("glGenBuffers");
        glBindBuffer = (PFN_GLBINDBUFFER)SDL_GL_GetProcAddress("glBindBuffer");
        glBufferData = (PFN_GLBUFFERDATA)SDL_GL_GetProcAddress("glBufferData");
        glEnableVertexAttribArray = (PFN_GLENABLEVERTEXATTRIBARRAY)SDL_GL_GetProcAddress("glEnableVertexAttribArray");
        glVertexAttribPointer = (PFN_GLVERTEXATTRIBPOINTER)SDL_GL_GetProcAddress("glVertexAttribPointer");
        glCreateShader = (PFN_GLCREATESHADER)SDL_GL_GetProcAddress("glCreateShader");
        glShaderSource = (PFN_GLSHADERSOURCE)SDL_GL_GetProcAddress("glShaderSource");
        glCompileShader = (PFN_GLCOMPILESHADER)SDL_GL_GetProcAddress("glCompileShader");
        glGetShaderiv = (PFN_GLGETSHADERIV)SDL_GL_GetProcAddress("glGetShaderiv");
        glGetShaderInfoLog = (PFN_GLGETSHADERINFOLOG)SDL_GL_GetProcAddress("glGetShaderInfoLog");
        glCreateProgram = (PFN_GLCREATEPROGRAM)SDL_GL_GetProcAddress("glCreateProgram");
        glAttachShader = (PFN_GLATTACHSHADER)SDL_GL_GetProcAddress("glAttachShader");
        glLinkProgram = (PFN_GLLINKPROGRAM)SDL_GL_GetProcAddress("glLinkProgram");
        glGetProgramiv = (PFN_GLGETPROGRAMIV)SDL_GL_GetProcAddress("glGetProgramiv");
        glGetProgramInfoLog = (PFN_GLGETPROGRAMINFOLOG)SDL_GL_GetProcAddress("glGetProgramInfoLog");
        glUseProgram = (PFN_GLUSEPROGRAM)SDL_GL_GetProcAddress("glUseProgram");
        glGetUniformLocation = (PFN_GLGETUNIFORMLOCATION)SDL_GL_GetProcAddress("glGetUniformLocation");
        glUniform1i = (PFN_GLUNIFORM1I)SDL_GL_GetProcAddress("glUniform1i");
        glUniformMatrix4fv = (PFN_GLUNIFORMMATRIX4FV)SDL_GL_GetProcAddress("glUniformMatrix4fv");
        glGenTextures = (PFN_GLGENTEXTURES)SDL_GL_GetProcAddress("glGenTextures");
        glBindTexture = (PFN_GLBINDTEXTURE)SDL_GL_GetProcAddress("glBindTexture");
        glTexImage2D = (PFN_GLTEXIMAGE2D)SDL_GL_GetProcAddress("glTexImage2D");
        glTexParameteri = (PFN_GLTEXPARAMETERI)SDL_GL_GetProcAddress("glTexParameteri");
    }
}