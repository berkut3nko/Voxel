module;
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
export module VoxelGame.GL;

// Скасовуємо макроси
#ifdef GL_SHADER_STORAGE_BUFFER
#undef GL_SHADER_STORAGE_BUFFER
#endif
#ifdef GL_DRAW_INDIRECT_BUFFER
#undef GL_DRAW_INDIRECT_BUFFER
#endif
#ifdef GL_SAMPLES_PASSED
#undef GL_SAMPLES_PASSED
#endif
#ifdef GL_ANY_SAMPLES_PASSED
#undef GL_ANY_SAMPLES_PASSED
#endif
#ifdef GL_QUERY_RESULT
#undef GL_QUERY_RESULT
#endif
#ifdef GL_QUERY_RESULT_AVAILABLE
#undef GL_QUERY_RESULT_AVAILABLE
#endif
#ifdef GL_ATOMIC_COUNTER_BUFFER
#undef GL_ATOMIC_COUNTER_BUFFER
#endif

export namespace VoxelGame::GL {
    // --- Typedefs ---
    typedef void (APIENTRY *PFN_GLGENVERTEXARRAYS) (GLsizei n, GLuint *arrays);
    typedef void (APIENTRY *PFN_GLBINDVERTEXARRAY) (GLuint array);
    typedef void (APIENTRY *PFN_GLGENBUFFERS) (GLsizei n, GLuint *buffers);
    typedef void (APIENTRY *PFN_GLBINDBUFFER) (GLenum target, GLuint buffer);
    typedef void (APIENTRY *PFN_GLBUFFERDATA) (GLenum target, GLsizeiptr size, const void *data, GLenum usage);
    typedef void (APIENTRY *PFN_GLBUFFERSUBDATA) (GLenum target, GLintptr offset, GLsizeiptr size, const void *data);
    typedef void (APIENTRY *PFN_GLGETBUFFERSUBDATA) (GLenum target, GLintptr offset, GLsizeiptr size, void *data);
    typedef void (APIENTRY *PFN_GLENABLEVERTEXATTRIBARRAY) (GLuint index);
    typedef void (APIENTRY *PFN_GLVERTEXATTRIBPOINTER) (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer);
    typedef void (APIENTRY *PFN_GLVERTEXATTRIBIPOINTER) (GLuint index, GLint size, GLenum type, GLsizei stride, const void *pointer);
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
    typedef void (APIENTRY *PFN_GLUNIFORM1UI) (GLint location, GLuint v0);
    typedef void (APIENTRY *PFN_GLUNIFORM1F) (GLint location, GLfloat v0); // Added
    typedef void (APIENTRY *PFN_GLUNIFORM3F) (GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
    typedef void (APIENTRY *PFN_GLUNIFORM4FV) (GLint location, GLsizei count, const GLfloat *value);
    typedef void (APIENTRY *PFN_GLUNIFORMMATRIX4FV) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);
    typedef void (APIENTRY *PFN_GLGENTEXTURES) (GLsizei n, GLuint *textures);
    typedef void (APIENTRY *PFN_GLBINDTEXTURE) (GLenum target, GLuint texture);
    typedef void (APIENTRY *PFN_GLTEXIMAGE2D) (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels);
    typedef void (APIENTRY *PFN_GLTEXIMAGE3D) (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const void *pixels);
    typedef void (APIENTRY *PFN_GLTEXSUBIMAGE2D) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels); // Added
    typedef void (APIENTRY *PFN_GLTEXSUBIMAGE3D) (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void *pixels);
    typedef void (APIENTRY *PFN_GLTEXPARAMETERI) (GLenum target, GLenum pname, GLint param);
    typedef void (APIENTRY *PFN_GLBINDBUFFERBASE) (GLenum target, GLuint index, GLuint buffer);
    typedef void (APIENTRY *PFN_GLMULTIDRAWARRAYSINDIRECT) (GLenum mode, const void *indirect, GLsizei drawcount, GLsizei stride);
    typedef void (APIENTRY *PFN_GLMULTIDRAWELEMENTSINDIRECT) (GLenum mode, GLenum type, const void *indirect, GLsizei drawcount, GLsizei stride);

    typedef void (APIENTRY *PFN_GLACTIVETEXTURE) (GLenum texture);
    typedef void (APIENTRY *PFN_GLDRAWELEMENTS) (GLenum mode, GLsizei count, GLenum type, const void *indices);
    typedef void (APIENTRY *PFN_GLVIEWPORT) (GLint x, GLint y, GLsizei width, GLsizei height);
    typedef void (APIENTRY *PFN_GLCLEAR) (GLbitfield mask);
    typedef void (APIENTRY *PFN_GLCLEARCOLOR) (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
    typedef void (APIENTRY *PFN_GLENABLE) (GLenum cap);
    typedef void (APIENTRY *PFN_GLDISABLE) (GLenum cap); // Added
    typedef void (APIENTRY *PFN_GLCULLFACE) (GLenum mode);
    typedef void (APIENTRY *PFN_GLFRONTFACE) (GLenum mode);
    typedef void (APIENTRY *PFN_GLDISPATCHCOMPUTE) (GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z);
    typedef void (APIENTRY *PFN_GLMEMORYBARRIER) (GLbitfield barriers);

    // Occlusion Query
    typedef void (APIENTRY *PFN_GLGENQUERIES) (GLsizei n, GLuint *ids);
    typedef void (APIENTRY *PFN_GLDELETEQUERIES) (GLsizei n, const GLuint *ids);
    typedef void (APIENTRY *PFN_GLBEGINQUERY) (GLenum target, GLuint id);
    typedef void (APIENTRY *PFN_GLENDQUERY) (GLenum target);
    typedef void (APIENTRY *PFN_GLGETQUERYOBJECTUIV) (GLuint id, GLenum pname, GLuint *params);
    typedef void (APIENTRY *PFN_GLGETQUERYOBJECTIV) (GLuint id, GLenum pname, GLint *params);
    typedef void (APIENTRY *PFN_GLCOLORMASK) (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
    typedef void (APIENTRY *PFN_GLDEPTHMASK) (GLboolean flag);

    // --- Constants ---
    constexpr GLenum SHADER_STORAGE_BUFFER = 0x90D2;
    constexpr GLenum DRAW_INDIRECT_BUFFER = 0x8F3F;
    constexpr GLenum ATOMIC_COUNTER_BUFFER = 0x92C0;
    
    // --- Structs ---
    struct DrawElementsIndirectCommand {
        GLuint  count;
        GLuint  instanceCount;
        GLuint  firstIndex;
        GLuint  baseVertex;
        GLuint  baseInstance;
    };

    // --- Function Pointers ---
    PFN_GLGENVERTEXARRAYS glGenVertexArrays;
    PFN_GLBINDVERTEXARRAY glBindVertexArray;
    PFN_GLGENBUFFERS glGenBuffers;
    PFN_GLBINDBUFFER glBindBuffer;
    PFN_GLBUFFERDATA glBufferData;
    PFN_GLBUFFERSUBDATA glBufferSubData;
    PFN_GLGETBUFFERSUBDATA glGetBufferSubData;
    PFN_GLENABLEVERTEXATTRIBARRAY glEnableVertexAttribArray;
    PFN_GLVERTEXATTRIBPOINTER glVertexAttribPointer;
    PFN_GLVERTEXATTRIBIPOINTER glVertexAttribIPointer;

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
    PFN_GLUNIFORM1UI glUniform1ui;
    PFN_GLUNIFORM1F glUniform1f; // Added
    PFN_GLUNIFORM3F glUniform3f;
    PFN_GLUNIFORM4FV glUniform4fv;
    PFN_GLUNIFORMMATRIX4FV glUniformMatrix4fv;
    PFN_GLGENTEXTURES glGenTextures;
    PFN_GLBINDTEXTURE glBindTexture;
    PFN_GLTEXIMAGE2D glTexImage2D;
    PFN_GLTEXIMAGE3D glTexImage3D;
    PFN_GLTEXSUBIMAGE2D glTexSubImage2D; // Added
    PFN_GLTEXSUBIMAGE3D glTexSubImage3D;
    PFN_GLTEXPARAMETERI glTexParameteri;
    
    PFN_GLBINDBUFFERBASE glBindBufferBase;
    PFN_GLMULTIDRAWARRAYSINDIRECT glMultiDrawArraysIndirect;
    PFN_GLMULTIDRAWELEMENTSINDIRECT glMultiDrawElementsIndirect;

    PFN_GLACTIVETEXTURE glActiveTexture;
    PFN_GLDRAWELEMENTS glDrawElements;
    PFN_GLVIEWPORT glViewport;
    PFN_GLCLEAR glClear;
    PFN_GLCLEARCOLOR glClearColor;
    PFN_GLENABLE glEnable;
    PFN_GLDISABLE glDisable; // Added
    PFN_GLCULLFACE glCullFace;
    PFN_GLFRONTFACE glFrontFace;
    PFN_GLDISPATCHCOMPUTE glDispatchCompute;
    PFN_GLMEMORYBARRIER glMemoryBarrier;

    PFN_GLGENQUERIES glGenQueries;
    PFN_GLDELETEQUERIES glDeleteQueries;
    PFN_GLBEGINQUERY glBeginQuery;
    PFN_GLENDQUERY glEndQuery;
    PFN_GLGETQUERYOBJECTUIV glGetQueryObjectuiv;
    PFN_GLGETQUERYOBJECTIV glGetQueryObjectiv;
    PFN_GLCOLORMASK glColorMask;
    PFN_GLDEPTHMASK glDepthMask;

    void LoadFunctions() {
        glGenVertexArrays = (PFN_GLGENVERTEXARRAYS)SDL_GL_GetProcAddress("glGenVertexArrays");
        glBindVertexArray = (PFN_GLBINDVERTEXARRAY)SDL_GL_GetProcAddress("glBindVertexArray");
        glGenBuffers = (PFN_GLGENBUFFERS)SDL_GL_GetProcAddress("glGenBuffers");
        glBindBuffer = (PFN_GLBINDBUFFER)SDL_GL_GetProcAddress("glBindBuffer");
        glBufferData = (PFN_GLBUFFERDATA)SDL_GL_GetProcAddress("glBufferData");
        glBufferSubData = (PFN_GLBUFFERSUBDATA)SDL_GL_GetProcAddress("glBufferSubData");
        glGetBufferSubData = (PFN_GLGETBUFFERSUBDATA)SDL_GL_GetProcAddress("glGetBufferSubData");
        glEnableVertexAttribArray = (PFN_GLENABLEVERTEXATTRIBARRAY)SDL_GL_GetProcAddress("glEnableVertexAttribArray");
        glVertexAttribPointer = (PFN_GLVERTEXATTRIBPOINTER)SDL_GL_GetProcAddress("glVertexAttribPointer");
        glVertexAttribIPointer = (PFN_GLVERTEXATTRIBIPOINTER)SDL_GL_GetProcAddress("glVertexAttribIPointer");

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
        glUniform1ui = (PFN_GLUNIFORM1UI)SDL_GL_GetProcAddress("glUniform1ui");
        glUniform1f = (PFN_GLUNIFORM1F)SDL_GL_GetProcAddress("glUniform1f"); // Loaded
        glUniform3f = (PFN_GLUNIFORM3F)SDL_GL_GetProcAddress("glUniform3f");
        glUniform4fv = (PFN_GLUNIFORM4FV)SDL_GL_GetProcAddress("glUniform4fv");
        glUniformMatrix4fv = (PFN_GLUNIFORMMATRIX4FV)SDL_GL_GetProcAddress("glUniformMatrix4fv");
        glGenTextures = (PFN_GLGENTEXTURES)SDL_GL_GetProcAddress("glGenTextures");
        glBindTexture = (PFN_GLBINDTEXTURE)SDL_GL_GetProcAddress("glBindTexture");
        glTexImage2D = (PFN_GLTEXIMAGE2D)SDL_GL_GetProcAddress("glTexImage2D");
        glTexImage3D = (PFN_GLTEXIMAGE3D)SDL_GL_GetProcAddress("glTexImage3D");
        glTexSubImage2D = (PFN_GLTEXSUBIMAGE2D)SDL_GL_GetProcAddress("glTexSubImage2D"); // Loaded
        glTexSubImage3D = (PFN_GLTEXSUBIMAGE3D)SDL_GL_GetProcAddress("glTexSubImage3D");
        glTexParameteri = (PFN_GLTEXPARAMETERI)SDL_GL_GetProcAddress("glTexParameteri");
        
        glBindBufferBase = (PFN_GLBINDBUFFERBASE)SDL_GL_GetProcAddress("glBindBufferBase");
        glMultiDrawArraysIndirect = (PFN_GLMULTIDRAWARRAYSINDIRECT)SDL_GL_GetProcAddress("glMultiDrawArraysIndirect");
        glMultiDrawElementsIndirect = (PFN_GLMULTIDRAWELEMENTSINDIRECT)SDL_GL_GetProcAddress("glMultiDrawElementsIndirect");

        glActiveTexture = (PFN_GLACTIVETEXTURE)SDL_GL_GetProcAddress("glActiveTexture");
        glDrawElements = (PFN_GLDRAWELEMENTS)SDL_GL_GetProcAddress("glDrawElements");
        glViewport = (PFN_GLVIEWPORT)SDL_GL_GetProcAddress("glViewport");
        glClear = (PFN_GLCLEAR)SDL_GL_GetProcAddress("glClear");
        glClearColor = (PFN_GLCLEARCOLOR)SDL_GL_GetProcAddress("glClearColor");
        glEnable = (PFN_GLENABLE)SDL_GL_GetProcAddress("glEnable");
        glDisable = (PFN_GLDISABLE)SDL_GL_GetProcAddress("glDisable"); // Loaded
        glCullFace = (PFN_GLCULLFACE)SDL_GL_GetProcAddress("glCullFace");
        glFrontFace = (PFN_GLFRONTFACE)SDL_GL_GetProcAddress("glFrontFace");
        glDispatchCompute = (PFN_GLDISPATCHCOMPUTE)SDL_GL_GetProcAddress("glDispatchCompute");
        glMemoryBarrier = (PFN_GLMEMORYBARRIER)SDL_GL_GetProcAddress("glMemoryBarrier");

        glGenQueries = (PFN_GLGENQUERIES)SDL_GL_GetProcAddress("glGenQueries");
        glDeleteQueries = (PFN_GLDELETEQUERIES)SDL_GL_GetProcAddress("glDeleteQueries");
        glBeginQuery = (PFN_GLBEGINQUERY)SDL_GL_GetProcAddress("glBeginQuery");
        glEndQuery = (PFN_GLENDQUERY)SDL_GL_GetProcAddress("glEndQuery");
        glGetQueryObjectuiv = (PFN_GLGETQUERYOBJECTUIV)SDL_GL_GetProcAddress("glGetQueryObjectuiv");
        glGetQueryObjectiv = (PFN_GLGETQUERYOBJECTIV)SDL_GL_GetProcAddress("glGetQueryObjectiv");
        glColorMask = (PFN_GLCOLORMASK)SDL_GL_GetProcAddress("glColorMask");
        glDepthMask = (PFN_GLDEPTHMASK)SDL_GL_GetProcAddress("glDepthMask");
    }
}