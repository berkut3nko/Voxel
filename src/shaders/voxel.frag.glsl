#version 330 core

out vec4 FragColor;

in vec3 v_pos;
in vec3 v_normal;
in float v_type;
in vec3 v_color; // NEW

uniform sampler2D u_texture;

void main() {
    // Просте дифузне освітлення, щоб бачити грані
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
    float diff = max(dot(v_normal, lightDir), 0.3); // 0.3 - ambient

    // Використовуємо випадковий колір замість текстури
    vec3 finalColor = v_color * diff;

    // Додаємо легку обводку (за бажанням, але для greedy meshing і так буде видно)
    // Просто виводимо колір
    FragColor = vec4(finalColor, 1.0);
}