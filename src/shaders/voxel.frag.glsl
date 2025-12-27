#version 330 core

out vec4 FragColor;

in vec3 v_pos;
in vec3 v_normal;
in float v_texLayer;

// Texture Array: [u, v, layer]
uniform sampler2DArray u_textureArray;

void main() {
    // 1. Triplanar mapping approximation
    vec2 uv;
    if (abs(v_normal.x) > 0.5) {
        uv = v_pos.yz;
    } else if (abs(v_normal.y) > 0.5) {
        uv = v_pos.xz;
    } else {
        uv = v_pos.xy;
    }

    // 2. Texture coordinates logic
    // Since we use Texture Arrays, we don't need atlasing logic or padding!
    // Wrapping works natively per layer.
    // Просто використовуємо uv як є (з повторенням)
    
    // 3. Sample from Array
    // vec3 coord = vec3(u, v, layer)
    vec4 color = texture(u_textureArray, vec3(uv, v_texLayer));

    // Просте освітлення
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
    float diff = max(dot(v_normal, lightDir), 0.4); 

    FragColor = vec4(color.rgb * diff, 1.0);
}