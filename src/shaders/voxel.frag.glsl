#version 330 core

out vec4 FragColor;

in vec3 v_pos;
in vec3 v_normal;
in float v_texLayer;
in vec2 v_quadUV; // Local coordinates (0..w, 0..h)

uniform sampler2DArray u_textureArray;
uniform bool u_showGrid; // Toggle for grid

void main() {
    // 1. Texturing Logic
    vec2 uv_world;
    if (abs(v_normal.x) > 0.5) uv_world = v_pos.yz;
    else if (abs(v_normal.y) > 0.5) uv_world = v_pos.xz;
    else uv_world = v_pos.xy;

    vec4 color = texture(u_textureArray, vec3(uv_world, v_texLayer));
    vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
    float diff = max(dot(v_normal, lightDir), 0.4); 
    vec3 finalColor = color.rgb * diff;

    // 2. Debug Grid Logic
    if (u_showGrid) {
        // Edge detection
        // Since v_quadUV is interpolated, it represents pixel position on the quad surface
        // We need 'w' and 'h' to know where the edges are.
        // Wait, we don't passed w/h as uniforms, but v_quadUV at corners are (0,0), (w,0), etc.
        // But inside the fragment shader we don't know 'w' and 'h' directly for THIS fragment easily without flat inputs.
        // HOWEVER, we can use derivatives! dFdx/dFdy give us rate of change per pixel.
        
        // Simpler approach:
        // We want to draw lines at integer coordinates? No, we want outline of the QUAD.
        // But we don't know the max U and V in the fragment shader easily.
        
        // ALTERNATIVE: Barycentric coordinates logic.
        // Or simply: drawing the diagonal.
        // The diagonal connects (0,0) and (w,h).
        // The equation is roughly: v / h = u / w  => v*w = u*h.
        // Still need w and h.
        
        // Let's pass w and h as well? No, packing is full.
        // Actually, we can cheat. We can use texture coordinates that go from 0..1 for the whole quad!
        // But we passed 0..w and 0..h to keep metric correctness?
        // Let's change packing in Meshing.cppm? No, metrics are good.
        
        // Let's deduce edges using standard GL derivatives or a simpler trick.
        // Wireframe via geometry shader is best, but here...
        // Let's just draw pixels where coordinate is close to integer? That would draw voxel grid.
        // The user asked for "quad diagonals".
        
        // Let's try to infer if we are on a diagonal.
        // The two triangles forming the quad share an edge.
        // Triangle 1: (0,0)-(w,0)-(w,h). Diagonal is (0,0) to (w,h).
        // Triangle 2: (0,0)-(w,h)-(0,h). Diagonal is (0,0) to (w,h).
        // Wait, the shared edge in my triangulation (1-2-3 and 1-3-4) connects p1(0,0) and p3(w,h).
        // So the diagonal is the line passing through (0,0) and (u,v).
        // If (v_quadUV.x / v_quadUV.y) is constant? No.
        
        // Hack: Use `gl_BarycentricCoord`? Not available in core 330 easily without extensions.
        
        // Let's just create a grid effect on unit blocks.
        // This will visualize the voxels.
        // To visualize the greedy quad, we need to know its boundaries.
        // Since we don't pass W/H to frag, we can't easily draw the far edges (u=w, v=h).
        // We ONLY know we are at u,v. We know u=0 and v=0 are edges.
        
        // BUT! We can visualize the diagonal if we change how we pass UVs.
        // If we passed normalized UVs (0..1), diagonal is u=v.
        // Let's change Meshing.cppm to pass 0..1 for this debug feature?
        // Or better: Let's assume the user wants to see the structure.
        // If we draw a line where `fract(v_pos)` is close to 0, we see voxels.
        
        // To see QUADS, we really need normalized UVs.
        // Let's change Meshing.cppm to store `uint32_t quadDims` (W, H) in `aPackedUV`?
        // We are already storing u,v values which are corners...
        // Ah, in Triangulate:
        // p1(0,0), p3(w,h).
        // If we simply check `abs(v_quadUV.x * h - v_quadUV.y * w)` ... we still need w,h.
        
        // Let's modify Meshing.cppm to pack (W, H) in the vertex data as well?
        // We have used all 3 attributes?
        // Pos: 1 uint. Attr: 1 uint. UV: 1 uint.
        // We can add another uint `aQuadSize`.
        // Or pack differently.
        
        // Let's try a visual trick.
        // The diagonal is the edge shared by the two triangles.
        // In most triangulation (0,0, 1,0, 1,1) and (0,0, 1,1, 0,1), the diagonal is (0,0)-(1,1).
        // We can create a "barycentric-like" effect.
        // Let's assume we want to highlight the edge `p1-p3`.
        // We can pass a generic "Edge Distance" attribute.
        // Vertex 1 (p1): dist = 0.
        // Vertex 2 (p2): dist = 1.
        // Vertex 3 (p3): dist = 0.
        // Vertex 4 (p4): dist = 1.
        // Then in shader, if value is near 0, we are on the diagonal!
        // This works for the diagonal.
        // For the border... that's harder without W/H.
        
        // Let's do the "Edge Distance" for diagonal!
        // And for borders... u=0 and v=0 are easy. u=w and v=h are hard.
        // But wait, since we use `GL_REPEAT` for textures, we don't necessarily need metric UVs for textures in THIS variable.
        // We use `v_pos` for texture mapping!
        // So `v_quadUV` is FREE for us to put whatever we want!
        // Let's put Normalized UVs (0..1) into `v_quadUV`!
        // Then borders are u=0, u=1, v=0, v=1. Diagonal is abs(u-v) < epsilon.
        
        // Let's change Meshing.cppm to pass NORMALIZED UVs (0, 1).
    }
    
    // Using the Normalized UV logic from updated Meshing.cppm (see below)
    if (u_showGrid) {
        float thickness = 0.02;
        // Borders
        bool border = v_quadUV.x < thickness || v_quadUV.x > 1.0 - thickness ||
                      v_quadUV.y < thickness || v_quadUV.y > 1.0 - thickness;
        // Diagonal (u approx v)
        bool diag = abs(v_quadUV.x - v_quadUV.y) < thickness;
        
        if (border || diag) {
            finalColor = mix(finalColor, vec3(1.0, 1.0, 1.0), 0.8);
        }
    }

    FragColor = vec4(finalColor, 1.0);
}