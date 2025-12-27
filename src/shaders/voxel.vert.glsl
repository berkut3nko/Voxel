#version 330 core

layout (location = 0) in uint aPackedPos;
layout (location = 1) in uint aPackedAttr;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_proj;

out vec3 v_pos;      // Світова позиція (для UV)
out vec3 v_normal;   // Нормаль
out float v_texLayer; // Індекс шару

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
    float x = float(aPackedPos & 0xFFu);
    float y = float((aPackedPos >> 8u) & 0xFFu);
    float z = float((aPackedPos >> 16u) & 0xFFu);
    
    vec3 localPos = vec3(x, y, z);
    vec4 worldPos = u_model * vec4(localPos, 1.0);

    gl_Position = u_proj * u_view * worldPos;

    uint normIdx = aPackedAttr & 0x7u;
    v_normal = getNormal(normIdx);

    // Розпаковка шару: біти 3-10
    v_texLayer = float((aPackedAttr >> 3u) & 0xFFu);

    v_pos = worldPos.xyz;
}