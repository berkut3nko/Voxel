module;
#include <vector>
#include <cstring>
#include <cstdlib> 
#include <cstdint>
#include <iostream>
#include <bit> 
#include <array>
export module VoxelGame.Meshing;

import VoxelGame.Types;
import VoxelGame.World;

using namespace VoxelGame::Types;
using namespace VoxelGame::World;

export namespace VoxelGame::Meshing {

    struct Quad {
        VoxelType type;
        int x, y, z;
        int w, h;
        int face; 
    };

    struct PackedVertex {
        uint32_t positionData; 
        uint32_t attributeData; 
        uint32_t quadUV;        
    };

    uint32_t packPos(int x, int y, int z) {
        return (x & 0xFF) | ((y & 0xFF) << 8) | ((z & 0xFF) << 16);
    }

    uint32_t packAttr(int normal, int texLayer) {
        return (normal & 0x07) | ((texLayer & 0xFF) << 3);
    }

    uint32_t packUV(float u, float v) {
        uint16_t ui = (uint16_t)(u * 65535.0f);
        uint16_t vi = (uint16_t)(v * 65535.0f);
        return (uint32_t)ui | ((uint32_t)vi << 16);
    }

    struct MeshingContext {
        const VoxelChunk* center;
        const VoxelChunk* neighbors[6]; 
    };

    struct BinaryQuad {
        int u;   
        int v;   
        int w_u; 
        int h_v; 
    };

    std::vector<BinaryQuad> greedy_mesh_binary_plane(std::array<uint32_t, 32> data) {
        std::vector<BinaryQuad> quads;
        for (int row = 0; row < 32; ++row) {
            while (data[row] != 0) {
                int u = std::countr_zero(data[row]);
                int w_u = std::countr_one(data[row] >> u);
                uint32_t h_mask_bits = (w_u == 32) ? 0xFFFFFFFF : ((1u << w_u) - 1);
                uint32_t mask = h_mask_bits << u;
                int h_v = 1; 
                while (row + h_v < 32) {
                    uint32_t next_row_bits = (data[row + h_v] >> u) & h_mask_bits;
                    if (next_row_bits != h_mask_bits) break;
                    data[row + h_v] &= ~mask;
                    h_v++;
                }
                data[row] &= ~mask;
                quads.push_back({u, row, w_u, h_v});
            }
        }
        return quads;
    }

    VoxelType get_voxel_context(const MeshingContext& ctx, int x, int y, int z) {
        if (x >= 0 && x < CHUNK_SIZE && y >= 0 && y < CHUNK_SIZE && z >= 0 && z < CHUNK_SIZE) {
            return ctx.center->get(x, y, z);
        }
        if (x < 0) return (ctx.neighbors[1]) ? ctx.neighbors[1]->get(CHUNK_SIZE - 1, y, z) : INTERNAL; 
        if (x >= CHUNK_SIZE) return (ctx.neighbors[0]) ? ctx.neighbors[0]->get(0, y, z) : INTERNAL;
        if (y < 0) return (ctx.neighbors[3]) ? ctx.neighbors[3]->get(x, CHUNK_SIZE - 1, z) : INTERNAL;
        if (y >= CHUNK_SIZE) return (ctx.neighbors[2]) ? ctx.neighbors[2]->get(x, 0, z) : INTERNAL;
        if (z < 0) return (ctx.neighbors[5]) ? ctx.neighbors[5]->get(x, y, CHUNK_SIZE - 1) : INTERNAL;
        if (z >= CHUNK_SIZE) return (ctx.neighbors[4]) ? ctx.neighbors[4]->get(x, y, 0) : INTERNAL;
        return AIR; 
    }

    std::vector<Quad> GenerateQuads(const MeshingContext& ctx) {
        std::vector<Quad> resultQuads;

        for (int d = 0; d < 3; ++d) {
            int u_ax = (d + 1) % 3;
            int v_ax = (d + 2) % 3;
            
            int x[3] = {0, 0, 0}; 
            int q[3] = {0, 0, 0}; 
            q[d] = 1;

            for (x[d] = -1; x[d] < CHUNK_SIZE; ++x[d]) {
                for (int side = 0; side < 2; ++side) {
                    
                    std::vector<std::array<uint32_t, 32>> material_masks(MAX_MATERIAL_TYPES, {0});
                    bool any_face = false;

                    for (x[v_ax] = 0; x[v_ax] < CHUNK_SIZE; ++x[v_ax]) {     
                        for (x[u_ax] = 0; x[u_ax] < CHUNK_SIZE; ++x[u_ax]) { 
                            
                            VoxelType curr = get_voxel_context(ctx, x[0], x[1], x[2]);
                            VoxelType next = get_voxel_context(ctx, x[0] + q[0], x[1] + q[1], x[2] + q[2]);

                            bool cSolid = !IsTransparent(curr);
                            bool nSolid = !IsTransparent(next);
                            
                            VoxelType faceType = AIR;

                            if (side == 0) { 
                                if (x[d] >= 0 && x[d] < CHUNK_SIZE) {
                                    if (cSolid && !nSolid) faceType = curr;
                                }
                            } else { 
                                int nextPos = x[d] + 1;
                                if (nextPos >= 0 && nextPos < CHUNK_SIZE) {
                                    if (!cSolid && nSolid) faceType = next;
                                }
                            }

                            if (faceType != AIR) {
                                int matID = GetMaterialID(faceType);
                                if (matID < MAX_MATERIAL_TYPES) {
                                    material_masks[matID][x[v_ax]] |= (1u << x[u_ax]);
                                    any_face = true;
                                }
                            }
                        }
                    }

                    if (!any_face) continue;

                    for (int matID = 0; matID < MAX_MATERIAL_TYPES; ++matID) {
                        auto binaryQuads = greedy_mesh_binary_plane(material_masks[matID]);

                        for (const auto& bq : binaryQuads) {
                            Quad q_final;
                            q_final.type = (matID << 3) | 0x07; 
                            
                            int pos[3];
                            pos[u_ax] = bq.u; 
                            pos[v_ax] = bq.v; 
                            
                            if (side == 0) {
                                pos[d] = x[d] + 1; 
                                q_final.face = d * 2; 
                            } else {
                                pos[d] = x[d] + 1; 
                                q_final.face = d * 2 + 1; 
                            }

                            q_final.x = pos[0];
                            q_final.y = pos[1];
                            q_final.z = pos[2];
                            q_final.w = bq.w_u; 
                            q_final.h = bq.h_v; 

                            resultQuads.push_back(q_final);
                        }
                    }
                }
            }
        }
        return resultQuads;
    }

    std::vector<PackedVertex> Triangulate(const std::vector<Quad>& quads) {
        std::vector<PackedVertex> vertices;
        vertices.reserve(quads.size() * 6);

        for (const auto& q : quads) {
            int x0 = q.x;
            int y0 = q.y;
            int z0 = q.z;
            int w = q.w; 
            int h = q.h; 
            
            int texLayer = GetTextureLayer(q.type);
            if(texLayer < 0) texLayer = 0;

            int face = q.face; 
            struct P { int x,y,z; };
            P p1, p2, p3, p4;

            // FIX: Remove redundant +1 for Positive Faces (X+, Y+, Z+)
            // GenerateQuads already sets x0/y0/z0 to the correct plane boundary.

            switch(face) {
                case 0: // X+ (Right) -> FIXED
                    // Normal (1,0,0)
                    // x0 is already x+1 from GenerateQuads
                    p1 = {x0,   y0,   z0};
                    p2 = {x0,   y0+w, z0};
                    p3 = {x0,   y0+w, z0+h};
                    p4 = {x0,   y0,   z0+h};
                    break;

                case 1: // X- (Left)
                    // Normal (-1,0,0)
                    p1 = {x0, y0,   z0};
                    p2 = {x0, y0,   z0+h};
                    p3 = {x0, y0+w, z0+h};
                    p4 = {x0, y0+w, z0};
                    break;

                case 2: // Y+ (Top) -> FIXED previously
                    // Normal (0,1,0)
                    p1 = {x0,   y0, z0};
                    p2 = {x0,   y0, z0+w}; 
                    p3 = {x0+h, y0, z0+w}; 
                    p4 = {x0+h, y0, z0};
                    break;

                case 3: // Y- (Bottom)
                    // Normal (0,-1,0)
                    p1 = {x0,   y0, z0};
                    p2 = {x0+h, y0, z0};
                    p3 = {x0+h, y0, z0+w};
                    p4 = {x0,   y0, z0+w};
                    break;

                case 4: // Z+ (Front) -> FIXED
                    // Normal (0,0,1)
                    // z0 is already z+1
                    p1 = {x0,   y0,   z0};
                    p2 = {x0+w, y0,   z0};
                    p3 = {x0+w, y0+h, z0};
                    p4 = {x0,   y0+h, z0};
                    break;

                case 5: // Z- (Back)
                    // Normal (0,0,-1)
                    p1 = {x0,   y0,   z0};
                    p2 = {x0,   y0+h, z0};
                    p3 = {x0+w, y0+h, z0};
                    p4 = {x0+w, y0,   z0};
                    break;
            }

            uint32_t attr = packAttr(face, texLayer);

            uint32_t uv00 = packUV(0.0f, 0.0f);
            uint32_t uv10 = packUV(1.0f, 0.0f);
            uint32_t uv11 = packUV(1.0f, 1.0f);
            uint32_t uv01 = packUV(0.0f, 1.0f);

            // Triangle 1: p1-p2-p3 (00 - 10 - 11)
            vertices.push_back({ packPos(p1.x, p1.y, p1.z), attr, uv00 });
            vertices.push_back({ packPos(p2.x, p2.y, p2.z), attr, uv10 });
            vertices.push_back({ packPos(p3.x, p3.y, p3.z), attr, uv11 });

            // Triangle 2: p1-p3-p4 (00 - 11 - 01)
            vertices.push_back({ packPos(p1.x, p1.y, p1.z), attr, uv00 });
            vertices.push_back({ packPos(p3.x, p3.y, p3.z), attr, uv11 });
            vertices.push_back({ packPos(p4.x, p4.y, p4.z), attr, uv01 });
        }
        return vertices;
    }
}