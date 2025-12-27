#version 450 core
#extension GL_ARB_shader_draw_parameters : require

struct GpuQuad {
    uint data1; 
    uint data2; 
};

layout(std430, binding = 0) readonly buffer QuadBuffer {
    GpuQuad quads[];
};

struct ChunkInfo {
    int x, z;
    uint quadStart;
    uint padding;
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
    uint quadIdx = chunk.quadStart + uint(gl_InstanceID);
    GpuQuad q = quads[quadIdx];

    uint x = q.data1 & 0xFFu;
    uint y = (q.data1 >> 8u) & 0xFFu;
    uint z = (q.data1 >> 16u) & 0xFFu;
    uint face = (q.data1 >> 24u) & 0x7u;
    uint texLayer = (q.data1 >> 27u) & 0x1Fu;

    float w = float(q.data2 & 0xFFFFu);
    float h = float((q.data2 >> 16u) & 0xFFFFu);

    // Vertex Logic (0..5 vertices from IBO)
    float u = (gl_VertexID == 1 || gl_VertexID == 2) ? 1.0 : 0.0;
    float v = (gl_VertexID == 2 || gl_VertexID == 3) ? 1.0 : 0.0;

    v_quadUV = vec2(u, v);

    vec3 localPos;
    
    // --- AXIS MAPPING & WINDING ORDER CORRECTION ---
    // Meshing.cppm Axis Mapping:
    // d=0 (X Faces): U=Y (Size w), V=Z (Size h)
    // d=1 (Y Faces): U=Z (Size w), V=X (Size h)
    // d=2 (Z Faces): U=X (Size w), V=Y (Size h)
    
    // FIXED: Removed +1.0 offset for faces 0, 2, 4 because
    // x/y/z from Meshing.cppm already represent the face plane coordinate.

    if (face == 0u) { // X+
        // Normal (1,0,0). CCW: Y then Z.
        // Y += u*w, Z += v*h
        localPos = vec3(float(x), float(y) + u*w, float(z) + v*h);
    } 
    else if (face == 1u) { // X-
        // Normal (-1,0,0). CCW requires Z then Y.
        // Y must still take 'w', Z must take 'h'.
        // Swap u/v control: Y += v*w, Z += u*h.
        localPos = vec3(float(x), float(y) + v*w, float(z) + u*h);
    }
    else if (face == 2u) { // Y+
        // Normal (0,1,0). CCW: Z then X.
        // Z += u*w, X += v*h
        localPos = vec3(float(x) + v*h, float(y), float(z) + u*w);
    }
    else if (face == 3u) { // Y-
        // Normal (0,-1,0). CCW requires X then Z.
        // Z must take 'w', X must take 'h'.
        // Swap u/v control: Z += v*w, X += u*h.
        localPos = vec3(float(x) + u*h, float(y), float(z) + v*w);
    }
    else if (face == 4u) { // Z+
        // Normal (0,0,1). CCW: X then Y.
        // X += u*w, Y += v*h
        localPos = vec3(float(x) + u*w, float(y) + v*h, float(z));
    }
    else { // Z-
        // Normal (0,0,-1). CCW requires Y then X.
        // X must take 'w', Y must take 'h'.
        // Swap u/v control: X += v*w, Y += u*h.
        localPos = vec3(float(x) + v*w, float(y) + u*h, float(z));
    }

    vec3 chunkOffset = vec3(float(chunk.x), 0.0, float(chunk.z));
    vec3 worldPos = localPos + chunkOffset;

    gl_Position = u_proj * u_view * vec4(worldPos, 1.0);

    v_normal = getNormal(face);
    v_texLayer = float(texLayer);
    v_pos = worldPos;
}