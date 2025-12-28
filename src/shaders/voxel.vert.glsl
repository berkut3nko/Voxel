#version 450 core
#extension GL_ARB_shader_draw_parameters : require

struct GpuQuad {
    uint data; 
};

layout(std430, binding = 0) readonly buffer QuadBuffer {
    GpuQuad quads[];
};

struct ChunkInfo {
    int packedXZ;
    uint quadStart;
    uint scale; // Scale of blocks (1, 2, 4)
};

layout(std430, binding = 1) readonly buffer ChunkInfoBuffer {
    ChunkInfo chunks[];
};

uniform mat4 u_view;
uniform mat4 u_proj;

out vec3 v_pos;
out vec3 v_normal;
out float v_texLayer;
out vec2 v_quadUV;
out vec3 v_debugColor; 

vec3 getNormal(uint idx) {
    if (idx == 0u) return vec3(1, 0, 0); 
    if (idx == 1u) return vec3(-1, 0, 0);
    if (idx == 2u) return vec3(0, 1, 0); 
    if (idx == 3u) return vec3(0, -1, 0);
    if (idx == 4u) return vec3(0, 0, 1); 
    if (idx == 5u) return vec3(0, 0, -1);
    return vec3(0, 1, 0);
}

void main() {
    ChunkInfo chunk = chunks[gl_DrawIDARB];
    float scale = float(chunk.scale); // 1.0, 2.0, or 4.0
    
    int chunkX = (int(chunk.packedXZ) << 16) >> 16;
    int chunkZ = int(chunk.packedXZ) >> 16;
    
    uint quadIdx = chunk.quadStart + uint(gl_InstanceID);
    GpuQuad q = quads[quadIdx];
    uint d = q.data;

    // Unpack Grid Coordinates (Block indices)
    uint x = d & 0x1Fu;
    uint y = (d >> 5u) & 0x1Fu;
    uint z = (d >> 10u) & 0x1Fu;
    uint face = (d >> 15u) & 0x7u;
    uint w_raw = (d >> 18u) & 0xFu;
    uint h_raw = (d >> 22u) & 0xFu;
    uint texLayer = (d >> 26u) & 0x3Fu;

    // Convert block count to world size
    float w = float(w_raw + 1u) * scale; 
    float h = float(h_raw + 1u) * scale;

    float u_uv = (gl_VertexID == 1 || gl_VertexID == 2) ? 1.0 : 0.0;
    float v_uv = (gl_VertexID == 2 || gl_VertexID == 3) ? 1.0 : 0.0;

    v_quadUV = vec2(u_uv, v_uv);

    // Convert block index to world position
    vec3 basePos = vec3(float(x), float(y), float(z)) * scale;

    // Add offset for positive faces (move from start of block to end of block)
    if (face == 0u || face == 2u || face == 4u) {
        if (face == 0u) basePos.x += scale;
        if (face == 2u) basePos.y += scale;
        if (face == 4u) basePos.z += scale;
    }

    vec3 localPos;
    if (face == 0u || face == 1u) { // X
        localPos = basePos + vec3(0.0, u_uv*w, v_uv*h);
    } else if (face == 2u || face == 3u) { // Y
        localPos = basePos + vec3(v_uv*h, 0.0, u_uv*w);
    } else { // Z
        localPos = basePos + vec3(u_uv*w, v_uv*h, 0.0);
    }

    vec3 chunkOffset = vec3(float(chunkX) * 32.0, 0.0, float(chunkZ) * 32.0);
    vec3 worldPos = localPos + chunkOffset;

    gl_Position = u_proj * u_view * vec4(worldPos, 1.0);

    v_normal = getNormal(face);
    v_texLayer = float(texLayer);
    v_pos = worldPos;
    v_debugColor = v_normal * 0.5 + 0.5; 
}