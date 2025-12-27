#version 460 core

out vec4 FragColor;

in vec3 v_pos;
in vec3 v_normal;
in float v_texLayer;
in vec2 v_quadUV; // Локальні UV (0..1) для малювання сітки

uniform sampler2DArray u_textureArray;
uniform bool u_showGrid; // Перемикач сітки

void main() {
    // 1. Triplanar Mapping Logic
    // Вибираємо площину проекції текстури залежно від нормалі
    vec2 uv_world;
    if (abs(v_normal.x) > 0.5) {
        uv_world = v_pos.yz; // Проекція на YZ для бічних стінок X
    } else if (abs(v_normal.y) > 0.5) {
        uv_world = v_pos.xz; // Проекція на XZ для підлоги/стелі
    } else {
        uv_world = v_pos.xy; // Проекція на XY для стінок Z
    }

    // 2. Texture Sampling
    // Використовуємо 3-й компонент (layer) для вибору текстури з масиву
    vec4 color = texture(u_textureArray, vec3(uv_world, v_texLayer));

    // 3. Simple Lighting
    // Спрямоване світло
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
    // Дифузне + Амбієнт (мінімум 0.4, щоб не було чорних тіней)
    float diff = max(dot(v_normal, lightDir), 0.4); 
    
    vec3 finalColor = color.rgb * diff;

    // 4. Debug Grid (Малювання рамок навколо квадів)
    if (u_showGrid) {
        float thickness = 0.02; // Товщина лінії
        
        // Межі квада (u=0, u=1, v=0, v=1)
        bool border = v_quadUV.x < thickness || v_quadUV.x > 1.0 - thickness ||
                      v_quadUV.y < thickness || v_quadUV.y > 1.0 - thickness;
        
        // Діагональ (опціонально, показує тріангуляцію)
        bool diag = abs(v_quadUV.x - v_quadUV.y) < thickness;
        
        if (border || diag) {
            // Змішуємо з білим кольором
            finalColor = mix(finalColor, vec3(1.0, 1.0, 1.0), 0.7);
        }
    }

    FragColor = vec4(finalColor, 1.0);
}