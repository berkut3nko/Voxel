#version 330 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in float aType;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_proj;

// Передаємо у фрагментний шейдер
out vec3 v_pos;    // Позиція у світі
out vec3 v_normal; // Нормаль грані
out float v_type;  // Тип блоку (1.0, 2.0...)

void main() {
    // Обчислюємо позицію
    vec4 worldPos = u_model * vec4(aPos, 1.0);
    gl_Position = u_proj * u_view * worldPos;
    
    // Передаємо дані далі
    v_pos = worldPos.xyz;
    v_normal = aNormal;
    v_type = aType;
}