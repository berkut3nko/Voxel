#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <cmath>
#include <cstring>

// ============================================================================
// 1. TINY MATH LIBRARY
// ============================================================================
struct Vec3 { float x, y, z; };
struct Mat4 { float m[16]; }; // Column-major

Mat4 mat4_identity() {
    Mat4 res;
    std::memset(res.m, 0, sizeof(float) * 16);
    res.m[0] = 1.0f; res.m[5] = 1.0f; res.m[10] = 1.0f; res.m[15] = 1.0f;
    return res;
}

Mat4 mat4_perspective(float fov, float aspect, float znear, float zfar) {
    Mat4 res = {};
    float tanHalfFov = tan(fov / 2.0f);
    res.m[0] = 1.0f / (aspect * tanHalfFov);
    res.m[5] = 1.0f / (tanHalfFov);
    res.m[10] = -(zfar + znear) / (zfar - znear);
    res.m[11] = -1.0f;
    res.m[14] = -(2.0f * zfar * znear) / (zfar - znear);
    return res;
}

Vec3 normalize(Vec3 v) {
    float len = sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
    if(len == 0) return {0,0,0};
    return {v.x/len, v.y/len, v.z/len};
}
Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x};
}
Vec3 sub(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
float dot(Vec3 a, Vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }

Mat4 mat4_lookAt(Vec3 eye, Vec3 center, Vec3 up) {
    Vec3 f = normalize(sub(center, eye));
    Vec3 s = normalize(cross(f, up));
    Vec3 u = cross(s, f);

    Mat4 res = mat4_identity();
    res.m[0] = s.x; res.m[4] = s.y; res.m[8] = s.z;
    res.m[1] = u.x; res.m[5] = u.y; res.m[9] = u.z;
    res.m[2] = -f.x; res.m[6] = -f.y; res.m[10] = -f.z;
    res.m[12] = -dot(s, eye);
    res.m[13] = -dot(u, eye);
    res.m[14] = dot(f, eye);
    return res;
}

// ============================================================================
// 2. OPENGL LOADER & UTILS
// ============================================================================
typedef void (APIENTRY *PFNGLGENVERTEXARRAYSPROC) (GLsizei n, GLuint *arrays);
typedef void (APIENTRY *PFNGLBINDVERTEXARRAYPROC) (GLuint array);
typedef void (APIENTRY *PFNGLGENBUFFERSPROC) (GLsizei n, GLuint *buffers);
typedef void (APIENTRY *PFNGLBINDBUFFERPROC) (GLenum target, GLuint buffer);
typedef void (APIENTRY *PFNGLBUFFERDATAPROC) (GLenum target, GLsizeiptr size, const void *data, GLenum usage);
typedef void (APIENTRY *PFNGLENABLEVERTEXATTRIBARRAYPROC) (GLuint index);
typedef void (APIENTRY *PFNGLVERTEXATTRIBPOINTERPROC) (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer);
typedef GLuint (APIENTRY *PFNGLCREATESHADERPROC) (GLenum type);
typedef void (APIENTRY *PFNGLSHADERSOURCEPROC) (GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length);
typedef void (APIENTRY *PFNGLCOMPILESHADERPROC) (GLuint shader);
typedef void (APIENTRY *PFNGLGETSHADERIVPROC) (GLuint shader, GLenum pname, GLint *params);
typedef void (APIENTRY *PFNGLGETSHADERINFOLOGPROC) (GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef GLuint (APIENTRY *PFNGLCREATEPROGRAMPROC) (void);
typedef void (APIENTRY *PFNGLATTACHSHADERPROC) (GLuint program, GLuint shader);
typedef void (APIENTRY *PFNGLLINKPROGRAMPROC) (GLuint program);
typedef void (APIENTRY *PFNGLGETPROGRAMIVPROC) (GLuint program, GLenum pname, GLint *params);
typedef void (APIENTRY *PFNGLGETPROGRAMINFOLOGPROC) (GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef void (APIENTRY *PFNGLUSEPROGRAMPROC) (GLuint program);
typedef GLint (APIENTRY *PFNGLGETUNIFORMLOCATIONPROC) (GLuint program, const GLchar *name);
typedef void (APIENTRY *PFNGLUNIFORM1IPROC) (GLint location, GLint v0);
typedef void (APIENTRY *PFNGLUNIFORMMATRIX4FVPROC) (GLint location, GLsizei count, GLboolean transpose, const GLfloat *value);

PFNGLGENVERTEXARRAYSPROC glGenVertexArrays;
PFNGLBINDVERTEXARRAYPROC glBindVertexArray;
PFNGLGENBuffersPROC glGenBuffers;
PFNGLBINDBUFFERPROC glBindBuffer;
PFNGLBUFFERDATAPROC glBufferData;
PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;
PFNGLCREATESHADERPROC glCreateShader;
PFNGLSHADERSOURCEPROC glShaderSource;
PFNGLCOMPILESHADERPROC glCompileShader;
PFNGLGETSHADERIVPROC glGetShaderiv;
PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;
PFNGLCREATEPROGRAMPROC glCreateProgram;
PFNGLATTACHSHADERPROC glAttachShader;
PFNGLLINKPROGRAMPROC glLinkProgram;
PFNGLGETPROGRAMIVPROC glGetProgramiv;
PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog;
PFNGLUSEPROGRAMPROC glUseProgram;
PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
PFNGLUNIFORM1IPROC glUniform1i;
PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv;

void LoadGLFunctions() {
    glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)SDL_GL_GetProcAddress("glGenVertexArrays");
    glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)SDL_GL_GetProcAddress("glBindVertexArray");
    glGenBuffers = (PFNGLGENBUFFERSPROC)SDL_GL_GetProcAddress("glGenBuffers");
    glBindBuffer = (PFNGLBINDBUFFERPROC)SDL_GL_GetProcAddress("glBindBuffer");
    glBufferData = (PFNGLBUFFERDATAPROC)SDL_GL_GetProcAddress("glBufferData");
    glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)SDL_GL_GetProcAddress("glEnableVertexAttribArray");
    glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)SDL_GL_GetProcAddress("glVertexAttribPointer");
    glCreateShader = (PFNGLCREATESHADERPROC)SDL_GL_GetProcAddress("glCreateShader");
    glShaderSource = (PFNGLSHADERSOURCEPROC)SDL_GL_GetProcAddress("glShaderSource");
    glCompileShader = (PFNGLCOMPILESHADERPROC)SDL_GL_GetProcAddress("glCompileShader");
    glGetShaderiv = (PFNGLGETSHADERIVPROC)SDL_GL_GetProcAddress("glGetShaderiv");
    glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)SDL_GL_GetProcAddress("glGetShaderInfoLog");
    glCreateProgram = (PFNGLCREATEPROGRAMPROC)SDL_GL_GetProcAddress("glCreateProgram");
    glAttachShader = (PFNGLATTACHSHADERPROC)SDL_GL_GetProcAddress("glAttachShader");
    glLinkProgram = (PFNGLLINKPROGRAMPROC)SDL_GL_GetProcAddress("glLinkProgram");
    glGetProgramiv = (PFNGLGETPROGRAMIVPROC)SDL_GL_GetProcAddress("glGetProgramiv");
    glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)SDL_GL_GetProcAddress("glGetProgramInfoLog");
    glUseProgram = (PFNGLUSEPROGRAMPROC)SDL_GL_GetProcAddress("glUseProgram");
    glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)SDL_GL_GetProcAddress("glGetUniformLocation");
    glUniform1i = (PFNGLUNIFORM1IPROC)SDL_GL_GetProcAddress("glUniform1i");
    glUniformMatrix4fv = (PFNGLUNIFORMMATRIX4FVPROC)SDL_GL_GetProcAddress("glUniformMatrix4fv");
}

std::string LoadFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return "";
    return std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

GLuint CreateShaderFunc(const std::string& src, GLenum type) {
    GLuint sh = glCreateShader(type);
    const char* csrc = src.c_str();
    glShaderSource(sh, 1, &csrc, nullptr);
    glCompileShader(sh);
    
    GLint success;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &success);
    if(!success) {
        char infoLog[512];
        glGetShaderInfoLog(sh, 512, nullptr, infoLog);
        std::cerr << "!!! SHADER ERROR (" << (type == GL_VERTEX_SHADER ? "VERT" : "FRAG") << ") !!!\n" << infoLog << std::endl;
    }
    return sh;
}

// ============================================================================
// 3. VOXEL DATA & MESHING
// ============================================================================

const int CHUNK_SIZE = 32;

struct Chunk {
    uint8_t blocks[CHUNK_SIZE][CHUNK_SIZE][CHUNK_SIZE];

    Chunk() {
        std::memset(blocks, 0, sizeof(blocks));
        for(int x=0; x<CHUNK_SIZE; x++) {
            for(int z=0; z<CHUNK_SIZE; z++) {
                int height = (int)(sin(x * 0.2) * 4 + 10);
                for(int y=0; y<height; y++) {
                    // Type 1 = Grass (Top), Type 2 = Dirt (Below)
                    blocks[x][y][z] = (y == height-1) ? 1 : 2; 
                }
            }
        }
    }
};

struct Vertex {
    float x, y, z;    // Position
    float nx, ny, nz; // Normal
    float type;       // Block ID
};

void GenerateMesh(const Chunk& chunk, std::vector<Vertex>& vertices) {
    for (int x=0; x<CHUNK_SIZE; x++) {
        for (int y=0; y<CHUNK_SIZE; y++) {
            for (int z=0; z<CHUNK_SIZE; z++) {
                uint8_t type = chunk.blocks[x][y][z];
                if (type == 0) continue;

                float fx = (float)x, fy = (float)y, fz = (float)z;
                float fType = (float)type;

                // FACE: FRONT (Z+) - Normal (0, 0, 1)
                if (z == CHUNK_SIZE-1 || chunk.blocks[x][y][z+1] == 0) {
                    vertices.push_back({fx, fy, fz+1,   0,0,1, fType});
                    vertices.push_back({fx+1, fy, fz+1, 0,0,1, fType});
                    vertices.push_back({fx+1, fy+1, fz+1, 0,0,1, fType});
                    vertices.push_back({fx, fy, fz+1,   0,0,1, fType});
                    vertices.push_back({fx+1, fy+1, fz+1, 0,0,1, fType});
                    vertices.push_back({fx, fy+1, fz+1, 0,0,1, fType});
                }

                // FACE: BACK (Z-) - Normal (0, 0, -1)
                if (z == 0 || chunk.blocks[x][y][z-1] == 0) {
                    vertices.push_back({fx+1, fy, fz,   0,0,-1, fType});
                    vertices.push_back({fx, fy, fz,     0,0,-1, fType});
                    vertices.push_back({fx, fy+1, fz,   0,0,-1, fType});
                    vertices.push_back({fx+1, fy, fz,   0,0,-1, fType});
                    vertices.push_back({fx, fy+1, fz,   0,0,-1, fType});
                    vertices.push_back({fx+1, fy+1, fz, 0,0,-1, fType});
                }

                // FACE: RIGHT (X+) - Normal (1, 0, 0)
                if (x == CHUNK_SIZE-1 || chunk.blocks[x+1][y][z] == 0) {
                    vertices.push_back({fx+1, fy, fz+1, 1,0,0, fType});
                    vertices.push_back({fx+1, fy, fz,   1,0,0, fType});
                    vertices.push_back({fx+1, fy+1, fz, 1,0,0, fType});
                    vertices.push_back({fx+1, fy, fz+1, 1,0,0, fType});
                    vertices.push_back({fx+1, fy+1, fz, 1,0,0, fType});
                    vertices.push_back({fx+1, fy+1, fz+1, 1,0,0, fType});
                }
                
                // FACE: LEFT (X-) - Normal (-1, 0, 0)
                if (x == 0 || chunk.blocks[x-1][y][z] == 0) {
                    vertices.push_back({fx, fy, fz,     -1,0,0, fType});
                    vertices.push_back({fx, fy, fz+1,   -1,0,0, fType});
                    vertices.push_back({fx, fy+1, fz+1, -1,0,0, fType});
                    vertices.push_back({fx, fy, fz,     -1,0,0, fType});
                    vertices.push_back({fx, fy+1, fz+1, -1,0,0, fType});
                    vertices.push_back({fx, fy+1, fz,   -1,0,0, fType});
                }

                // FACE: TOP (Y+) - Normal (0, 1, 0)
                if (y == CHUNK_SIZE-1 || chunk.blocks[x][y+1][z] == 0) {
                    vertices.push_back({fx, fy+1, fz+1,   0,1,0, fType});
                    vertices.push_back({fx+1, fy+1, fz+1, 0,1,0, fType});
                    vertices.push_back({fx+1, fy+1, fz,   0,1,0, fType});
                    vertices.push_back({fx, fy+1, fz+1,   0,1,0, fType});
                    vertices.push_back({fx+1, fy+1, fz,   0,1,0, fType});
                    vertices.push_back({fx, fy+1, fz,     0,1,0, fType});
                }

                // FACE: BOTTOM (Y-) - Normal (0, -1, 0)
                if (y == 0 || chunk.blocks[x][y-1][z] == 0) {
                    vertices.push_back({fx, fy, fz,     0,-1,0, fType});
                    vertices.push_back({fx+1, fy, fz,   0,-1,0, fType});
                    vertices.push_back({fx+1, fy, fz+1, 0,-1,0, fType});
                    vertices.push_back({fx, fy, fz,     0,-1,0, fType});
                    vertices.push_back({fx+1, fy, fz+1, 0,-1,0, fType});
                    vertices.push_back({fx, fy, fz+1,   0,-1,0, fType});
                }
            }
        }
    }
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_Window* window = SDL_CreateWindow("Voxel Vertex Playground", 1024, 768, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    SDL_GLContext context = SDL_GL_CreateContext(window);
    LoadGLFunctions();
    
    // MOUSE MODE SETUP:
    // Relative Mode is the correct way for FPS cameras.
    // It hides the cursor and gives infinite delta movement.
    // We try to enable it immediately.
    bool mouseCaptured = true;
    if (!SDL_SetWindowRelativeMouseMode(window, true)) {
        std::cerr << "Warning: Relative Mouse Mode failed! " << SDL_GetError() << std::endl;
        mouseCaptured = false;
    }

    Chunk myChunk;
    std::vector<Vertex> meshVertices;
    GenerateMesh(myChunk, meshVertices);

    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, meshVertices.size() * sizeof(Vertex), meshVertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    std::string vCode = LoadFile("src/shaders/voxel.vert.glsl");
    std::string fCode = LoadFile("src/shaders/voxel.frag.glsl");
    GLuint prog = glCreateProgram();
    GLuint vs = CreateShaderFunc(vCode, GL_VERTEX_SHADER);
    GLuint fs = CreateShaderFunc(fCode, GL_FRAGMENT_SHADER);
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glUseProgram(prog);

    // TEXTURE ATLAS (2x1)
    GLuint texID;
    glGenTextures(1, &texID); 
    glBindTexture(GL_TEXTURE_2D, texID); 
    uint8_t texData[2 * 1 * 3] = {
        0, 255, 0,     // Pixel 0: Green (Grass)
        139, 69, 19    // Pixel 1: Brown (Dirt)
    };
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 2, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, texData); 
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); 
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST); 
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); 
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); 

    glUniform1i(glGetUniformLocation(prog, "u_texture"), 0);

    float camX = 16.0f, camY = 30.0f, camZ = 60.0f;
    float yaw = -90.0f, pitch = -30.0f;

    bool running = true;
    
    while(running) {
        SDL_Event ev;
        while(SDL_PollEvent(&ev)) {
            if(ev.type == SDL_EVENT_QUIT) running = false;
            
            // ESC releases mouse, Click captures it
            if(ev.type == SDL_EVENT_KEY_DOWN && ev.key.key == SDLK_ESCAPE) {
                mouseCaptured = false;
                SDL_SetWindowRelativeMouseMode(window, false);
            }
            if(ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                if (!mouseCaptured) {
                    mouseCaptured = true;
                    SDL_SetWindowRelativeMouseMode(window, true);
                }
            }

            if(ev.type == SDL_EVENT_MOUSE_MOTION) {
                if (mouseCaptured) {
                    // Use RELATIVE motion from event. 
                    // This works even if cursor hits edge in Relative Mode.
                    yaw += ev.motion.xrel * 0.1f;
                    pitch -= ev.motion.yrel * 0.1f;
                    
                    if(pitch > 89.0f) pitch = 89.0f;
                    if(pitch < -89.0f) pitch = -89.0f;
                }
            }
        }

        float radYaw = yaw * 0.0174533f;
        float radPitch = pitch * 0.0174533f;
        
        Vec3 front = { 
            static_cast<float>(cos(radYaw)*cos(radPitch)), 
            static_cast<float>(sin(radPitch)), 
            static_cast<float>(sin(radYaw)*cos(radPitch)) 
        };
        front = normalize(front);

        const bool* keys = SDL_GetKeyboardState(nullptr);
        float speed = 0.5f;
        Vec3 right = normalize(cross(front, {0,1,0}));
        
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
        glDisable(GL_CULL_FACE);

        Mat4 model = mat4_identity();
        Mat4 view = mat4_lookAt({camX, camY, camZ}, {camX+front.x, camY+front.y, camZ+front.z}, {0,1,0});
        Mat4 proj = mat4_perspective(1.047f, (float)w/h, 0.1f, 1000.0f); 

        glUniformMatrix4fv(glGetUniformLocation(prog, "u_model"), 1, GL_FALSE, model.m);
        glUniformMatrix4fv(glGetUniformLocation(prog, "u_view"), 1, GL_FALSE, view.m);
        glUniformMatrix4fv(glGetUniformLocation(prog, "u_proj"), 1, GL_FALSE, proj.m);

        glBindVertexArray(VAO);
        glBindTexture(GL_TEXTURE_2D, texID);
        glDrawArrays(GL_TRIANGLES, 0, meshVertices.size());

        SDL_GL_SwapWindow(window);
    }

    return 0;
}