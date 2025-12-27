#version 450 core

out vec4 FragColor;

in vec3 v_pos;
in vec3 v_normal;
in float v_texLayer;
in vec2 v_quadUV;

uniform sampler2DArray u_textureArray;
uniform bool u_showGrid;

void main() {
    vec2 uv_world;
    if (abs(v_normal.x) > 0.5) {
        uv_world = v_pos.yz;
    } else if (abs(v_normal.y) > 0.5) {
        uv_world = v_pos.xz;
    } else {
        uv_world = v_pos.xy;
    }

    vec4 color = texture(u_textureArray, vec3(uv_world, v_texLayer));
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
    float diff = max(dot(v_normal, lightDir), 0.4); 
    
    vec3 finalColor = color.rgb * diff;

    if (u_showGrid) {
        float thickness = 0.02;
        bool border = v_quadUV.x < thickness || v_quadUV.x > 1.0 - thickness ||
                      v_quadUV.y < thickness || v_quadUV.y > 1.0 - thickness;
        bool diag = abs(v_quadUV.x - v_quadUV.y) < thickness;
        
        if (border || diag) {
            finalColor = mix(finalColor, vec3(1.0, 1.0, 1.0), 0.7);
        }
    }

    FragColor = vec4(finalColor, 1.0);
}