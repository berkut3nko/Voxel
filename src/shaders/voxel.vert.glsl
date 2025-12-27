#version 460 core

// --- SSBO Definitions ---

// Дані одного квада (8 байт)
struct GpuQuad {
    uint data1; // x, y, z, face, texLayer
    uint data2; // w, h
};

// Буфер усіх квадів світу
layout(std430, binding = 0) readonly buffer QuadBuffer {
    GpuQuad quads[];
};

// Інформація про чанк (позиція + зсув у буфері квадів)
struct ChunkInfo {
    int x, z;       // Світові координати чанка
    uint quadStart; // Індекс першого квада у QuadBuffer
    uint padding;   // Вирівнювання (16 байт)
};

// Буфер чанків
layout(std430, binding = 1) readonly buffer ChunkInfoBuffer {
    ChunkInfo chunks[];
};

// --- Uniforms ---
uniform mat4 u_view;
uniform mat4 u_proj;

// --- Outputs ---
out vec3 v_pos;
out vec3 v_normal;
out float v_texLayer;
out vec2 v_quadUV;

// Helper: Normal Decoding
vec3 getNormal(uint idx) {
    if (idx == 0u) return vec3(1, 0, 0);  // X+
    if (idx == 1u) return vec3(-1, 0, 0); // X-
    if (idx == 2u) return vec3(0, 1, 0);  // Y+
    if (idx == 3u) return vec3(0, -1, 0); // Y-
    if (idx == 4u) return vec3(0, 0, 1);  // Z+
    if (idx == 5u) return vec3(0, 0, -1); // Z-
    return vec3(0, 1, 0);
}

void main() {
    // 1. Отримуємо інформацію про поточний чанк
    // gl_BaseInstance встановлюється через параметр baseInstance у команді Indirect Draw.
    // Він вказує на індекс чанка у нашому ChunkInfoBuffer.
    ChunkInfo chunk = chunks[gl_BaseInstance];

    // 2. Отримуємо інформацію про поточний квад
    // gl_InstanceID - це індекс інстанса (квада) у поточному виклику Draw.
    // Глобальний індекс квада = (початок квадів чанка) + (поточний квад).
    uint quadIdx = chunk.quadStart + uint(gl_InstanceID);
    GpuQuad q = quads[quadIdx];

    // 3. Розпаковка даних квада (Bitwise Magic)
    // Data1 Layout:
    // Bits 0-7:   X
    // Bits 8-15:  Y
    // Bits 16-23: Z
    // Bits 24-26: Face (0..5)
    // Bits 27-31: Texture Layer (0..31)
    uint x = q.data1 & 0xFFu;
    uint y = (q.data1 >> 8u) & 0xFFu;
    uint z = (q.data1 >> 16u) & 0xFFu;
    uint face = (q.data1 >> 24u) & 0x7u;
    uint texLayer = (q.data1 >> 27u) & 0x1Fu;

    // Data2 Layout:
    // Bits 0-15:  Width
    // Bits 16-31: Height
    uint w = q.data2 & 0xFFFFu;
    uint h = (q.data2 >> 16u) & 0xFFFFu;

    // 4. Генерація вершини (Programmable Pulling)
    // gl_VertexID приходить з IBO: 0, 1, 2, 0, 2, 3 (стандартний квад).
    // Нам треба визначити UV координати (0 або 1) для кутів квада.
    // V0(0,0), V1(1,0), V2(1,1), V3(0,1)
    
    // Логіка для індексів 0,1,2,3:
    // 0: (0, 0)
    // 1: (1, 0)
    // 2: (1, 1)
    // 3: (0, 1)
    
    float u = (gl_VertexID == 1 || gl_VertexID == 2) ? 1.0 : 0.0;
    float v = (gl_VertexID == 2 || gl_VertexID == 3) ? 1.0 : 0.0;

    // Зберігаємо нормалізовані UV для сітки
    v_quadUV = vec2(u, v);

    // Масштабуємо UV на розмір квада для позиції
    float du = u * float(w);
    float dv = v * float(h);

    // 5. Обчислення локальної позиції вершини
    // Залежить від орієнтації грані (Face)
    vec3 localPos;

    if (face == 0u) { // X+ (Right) -> Plane YZ
        // Normal (1,0,0). Pos = x+1. U=Y, V=Z.
        localPos = vec3(float(x) + 1.0, float(y) + du, float(z) + dv);
    } 
    else if (face == 1u) { // X- (Left) -> Plane YZ
        // Normal (-1,0,0). Pos = x. U=Y, V=Z.
        localPos = vec3(float(x), float(y) + du, float(z) + dv);
    }
    else if (face == 2u) { // Y+ (Top) -> Plane XZ
        // Normal (0,1,0). Pos = y+1. U=Z, V=X (FIXED mapping)
        // Раніше ми міняли u/v місцями, тут дотримуємось логіки Meshing.cppm:
        // p2 (u=1, v=0) змінює Z. p4 (u=0, v=1) змінює X.
        // Отже u -> Z, v -> X.
        localPos = vec3(float(x) + dv, float(y) + 1.0, float(z) + du);
    }
    else if (face == 3u) { // Y- (Bottom) -> Plane XZ
        // Normal (0,-1,0). Pos = y. U=Z, V=X.
        localPos = vec3(float(x) + dv, float(y), float(z) + du);
    }
    else if (face == 4u) { // Z+ (Front) -> Plane XY
        // Normal (0,0,1). Pos = z+1. U=X, V=Y.
        localPos = vec3(float(x) + du, float(y) + dv, float(z) + 1.0);
    }
    else { // Z- (Back) -> Plane XY
        // Normal (0,0,-1). Pos = z. U=X, V=Y.
        localPos = vec3(float(x) + du, float(y) + dv, float(z));
    }

    // 6. Фінальна світова позиція
    // Додаємо зміщення чанка (chunk.x, chunk.z)
    vec3 chunkOffset = vec3(float(chunk.x), 0.0, float(chunk.z));
    vec3 worldPos = localPos + chunkOffset;

    // 7. Вивід
    gl_Position = u_proj * u_view * vec4(worldPos, 1.0);

    v_pos = worldPos;
    v_normal = getNormal(face);
    v_texLayer = float(texLayer);
}