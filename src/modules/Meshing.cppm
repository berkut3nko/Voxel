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

    // Структура для SSBO (8 байт), вирівняна як uvec2 у шейдері
    struct GpuQuad {
        uint32_t data1; // x,y,z,face,tex
        uint32_t data2; // w,h
    };

    // Упаковка даних для SSBO
    uint32_t packData1(int x, int y, int z, int face, int texLayer) {
        return (x & 0xFF) | 
               ((y & 0xFF) << 8) | 
               ((z & 0xFF) << 16) |
               ((face & 0x07) << 24) |
               ((texLayer & 0x1F) << 27);
    }

    uint32_t packData2(int w, int h) {
        return (w & 0xFFFF) | ((h & 0xFFFF) << 16);
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

    // Замість Triangulate - збираємо дані для SSBO
    std::vector<GpuQuad> BuildSSBOData(const std::vector<Quad>& quads) {
        std::vector<GpuQuad> gpuQuads;
        gpuQuads.reserve(quads.size());

        for (const auto& q : quads) {
            int texLayer = GetTextureLayer(q.type);
            if(texLayer < 0) texLayer = 0;

            uint32_t d1 = packData1(q.x, q.y, q.z, q.face, texLayer);
            uint32_t d2 = packData2(q.w, q.h);

            gpuQuads.push_back({d1, d2});
        }
        return gpuQuads;
    }
}