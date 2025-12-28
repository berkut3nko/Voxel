#version 450 core
layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;

struct ChunkInput {
    int packedXZ; 
    uint quadStart; 
    uint quadCount; 
    int maxHeight;
    uint scale;     // Added scale
    int padding;
};

struct DrawCommand {
    uint count;         
    uint instanceCount; 
    uint firstIndex;    
    uint baseVertex;    
    uint baseInstance;  
};

layout(std430, binding = 1) readonly buffer ChunkBuffer {
    ChunkInput chunks[];
};

layout(std430, binding = 2) writeonly buffer DrawCommandBuffer {
    DrawCommand commands[];
};

struct ChunkInfoShader {
    int packedXZ;
    uint quadStart;
    uint scale; // Pass to vertex shader
};
layout(std430, binding = 3) writeonly buffer VisibleChunkBuffer {
    ChunkInfoShader visibleChunks[];
};

layout(binding = 0, offset = 0) uniform atomic_uint u_drawnCount;

uniform mat4 u_viewProj;
uniform vec3 u_camPos;
uniform vec3 u_viewDir; 
uniform uint u_chunkCount;
uniform vec4 u_frustumPlanes[6]; 

uniform sampler2D u_heightMap; 
uniform float u_renderDist;    

bool IsAABBInFrustum(vec3 minPos, vec3 maxPos) {
    for (int i = 0; i < 6; i++) {
        vec4 plane = u_frustumPlanes[i];
        vec3 p = minPos;
        if (plane.x >= 0) p.x = maxPos.x;
        if (plane.y >= 0) p.y = maxPos.y;
        if (plane.z >= 0) p.z = maxPos.z;

        if (dot(plane.xyz, p) + plane.w < 0) {
            return false;
        }
    }
    return true;
}

bool IsOccludedByHorizon(int cx, int cz, int maxHeight) {
    float CHUNK_SIZE = 32.0;
    vec2 chunkPos = vec2(cx, cz);
    vec2 camChunkPos = u_camPos.xz / CHUNK_SIZE; 

    float dist = length(chunkPos - camChunkPos);
    if (dist < 1.5) return false; 

    vec2 dir = normalize(chunkPos - camChunkPos);
    int steps = int(dist);
    float maxTanTheta = -10000.0;
    
    for (int i = 1; i < steps; ++i) {
        vec2 samplePos = camChunkPos + dir * float(i);
        float mapSize = 2.0 * u_renderDist + 1.0;
        vec2 uv = (samplePos + vec2(u_renderDist)) / mapSize;
        
        if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) continue;

        float h = textureLod(u_heightMap, uv, 0.0).r * 255.0;
        float distToSample = length(samplePos * CHUNK_SIZE - u_camPos.xz);
        if (distToSample < 1.0) continue;

        float tanTheta = (h - u_camPos.y) / distToSample;
        if (tanTheta > maxTanTheta) maxTanTheta = tanTheta;
    }

    float distToTarget = length(chunkPos * CHUNK_SIZE - u_camPos.xz);
    float targetTanTheta = (float(maxHeight) - u_camPos.y) / distToTarget;

    if (targetTanTheta < maxTanTheta - 0.02) return true; 
    return false;
}

void main() {
    uint idx = gl_GlobalInvocationID.x;
    if (idx >= u_chunkCount) return;

    ChunkInput chunk = chunks[idx];
    if (chunk.quadCount == 0) return;

    int chunkX = (int(chunk.packedXZ) << 16) >> 16;
    int chunkZ = int(chunk.packedXZ) >> 16;

    float SIZE = 32.0; 
    vec3 minPos = vec3(float(chunkX) * SIZE, 0.0, float(chunkZ) * SIZE);
    vec3 maxPos = minPos + vec3(SIZE, float(chunk.maxHeight), SIZE);
    vec3 chunkCenter = minPos + vec3(SIZE * 0.5, float(chunk.maxHeight)*0.5, SIZE * 0.5);

    float currentMaxDist = (SIZE * (u_renderDist + 2.0)); 

    if (u_viewDir.y < -0.2) { 
        vec3 dirToChunk = normalize(chunkCenter - u_camPos);
        if (dot(u_viewDir, dirToChunk) > 0.7) {
             currentMaxDist = SIZE * 1024.0;
        }
    }

    float distSq = dot(minPos.xz + vec2(16.0) - u_camPos.xz, minPos.xz + vec2(16.0) - u_camPos.xz);
    if (distSq > currentMaxDist * currentMaxDist) return; 

    if (!IsAABBInFrustum(minPos, maxPos)) return;
    if (u_viewDir.y > -0.9 && IsOccludedByHorizon(chunkX, chunkZ, chunk.maxHeight)) return;

    uint outIndex = atomicCounterIncrement(u_drawnCount);

    DrawCommand cmd;
    cmd.count = 6;
    cmd.instanceCount = chunk.quadCount;
    cmd.firstIndex = 0;
    cmd.baseVertex = 0;
    cmd.baseInstance = 0; 
    
    commands[outIndex] = cmd;

    ChunkInfoShader info;
    info.packedXZ = chunk.packedXZ;
    info.quadStart = chunk.quadStart;
    info.scale = chunk.scale; // Pass scale
    
    visibleChunks[outIndex] = info;
}