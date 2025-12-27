#version 330 core

out vec4 FragColor;

in vec3 v_pos;
in vec3 v_normal;
in float v_type;

uniform sampler2D u_texture;

void main() {
    // 1. Визначаємо базові UV
    vec2 uv;
    if (abs(v_normal.x) > 0.5) {
        uv = v_pos.yz;
    } else if (abs(v_normal.y) > 0.5) {
        uv = v_pos.xz;
    } else {
        uv = v_pos.xy;
    }

    // 2. Отримуємо дробову частину (0..1)
    uv = fract(uv);

    // 3. FIX ARTIFACTS: Стискаємо UV, щоб не брати пікселі з сусіднього тайлу
    // Замість 0.0...1.0 беремо 0.01...0.99
    // Це запобігає "кровоточенню" текстури (bleeding)
    float padding = 0.02;
    uv = uv * (1.0 - 2.0 * padding) + padding;

    // 4. Масштабуємо для атласу (у нас 2 тайли по горизонталі)
    // Ширина одного тайлу = 0.5
    uv.x *= 0.5;

    // 5. Зміщення для типу блоку
    if (v_type > 1.5) { // Dirt
        uv.x += 0.5;
    } else {            // Grass
        uv.x += 0.0;
    }

    FragColor = texture(u_texture, uv);
}