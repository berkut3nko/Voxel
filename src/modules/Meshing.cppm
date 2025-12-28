module;
#include <vector>
#include <cstring>
#include <cstdlib> 
#include <cstdint>
#include <iostream>
#include <bit> 
#include <array>
#include <type_traits>
#include <cmath>
#include <algorithm>
export module VoxelGame.Meshing;

import VoxelGame.Types;
import VoxelGame.World;

using namespace VoxelGame::Types;
using namespace VoxelGame::World;

export namespace VoxelGame::Meshing {

    struct Quad {
        uint8_t x, y, z;
        uint8_t w, h;
        uint8_t face; 
        uint8_t texLayer;
    };

    struct GpuQuad {
        uint32_t data; 
    };

    uint32_t packQuad(const Quad& q) {
        uint32_t d = 0;
        d |= (q.x & 0x1F);
        d |= (q.y & 0x1F) << 5;
        d |= (q.z & 0x1F) << 10;
        d |= (q.face & 0x07) << 15;
        d |= ((q.w - 1) & 0x0F) << 18; 
        d |= ((q.h - 1) & 0x0F) << 22; 
        d |= (q.texLayer & 0x3F) << 26;
        return d;
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

    template <typename T, size_t N>
    std::vector<BinaryQuad> GreedyMeshBinaryPlane(std::array<T, N> data) {
        std::vector<BinaryQuad> quads;
        using UT = std::make_unsigned_t<T>;
        
        for (int row = 0; row < N; ++row) {
            while (data[row] != 0) {
                int u = std::countr_zero(static_cast<UT>(data[row]));
                int w_u = std::countr_one(static_cast<UT>(data[row] >> u));
                
                // Limit max width to 16 blocks (4 bits)
                if (w_u > 16) w_u = 16;

                UT h_mask_bits;
                if (w_u == sizeof(T) * 8) {
                    h_mask_bits = static_cast<UT>(~0);
                } else {
                    h_mask_bits = (static_cast<UT>(1) << w_u) - 1;
                }

                UT mask = h_mask_bits << u;
                
                int h_v = 1; 
                while (row + h_v < N) {
                    if (h_v >= 16) break; // Limit max height

                    UT next_row_bits = (static_cast<UT>(data[row + h_v]) >> u) & h_mask_bits;
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
        // Check pointers before accessing neighbors!
        if (x < 0) return (ctx.neighbors[1]) ? ctx.neighbors[1]->get(CHUNK_SIZE - 1, y, z) : INTERNAL; 
        if (x >= CHUNK_SIZE) return (ctx.neighbors[0]) ? ctx.neighbors[0]->get(0, y, z) : INTERNAL;
        if (y < 0) return (ctx.neighbors[3]) ? ctx.neighbors[3]->get(x, CHUNK_SIZE - 1, z) : INTERNAL;
        if (y >= CHUNK_SIZE) return (ctx.neighbors[2]) ? ctx.neighbors[2]->get(x, 0, z) : INTERNAL;
        if (z < 0) return (ctx.neighbors[5]) ? ctx.neighbors[5]->get(x, y, CHUNK_SIZE - 1) : INTERNAL;
        if (z >= CHUNK_SIZE) return (ctx.neighbors[4]) ? ctx.neighbors[4]->get(x, y, 0) : INTERNAL;
        return AIR; 
    }

    VoxelType ScanDominantVoxel(const MeshingContext& ctx, 
                               int u_start, int v_start, int d_start,
                               int u_ax, int v_ax, int d_ax, 
                               int step, int scan_dir) {
        if (step == 1) {
            int pos[3];
            pos[u_ax] = u_start;
            pos[v_ax] = v_start;
            pos[d_ax] = d_start;
            return get_voxel_context(ctx, pos[0], pos[1], pos[2]);
        }

        int weights[MAX_MATERIAL_TYPES + 1] = {0};
        bool foundAnySolid = false;

        for (int du = 0; du < step; ++du) {
            for (int dv = 0; dv < step; ++dv) {
                for (int k = 0; k < step; ++k) {
                    int pos[3];
                    pos[u_ax] = u_start + du;
                    pos[v_ax] = v_start + dv;
                    pos[d_ax] = d_start + (k * scan_dir); 

                    VoxelType t = get_voxel_context(ctx, pos[0], pos[1], pos[2]);
                    
                    if (!IsTransparent(t)) {
                        int matID = GetMaterialID(t);
                        if (matID >= MAX_MATERIAL_TYPES) matID = 0;
                        if (matID == 4) break; 

                        int w = (k == 0) ? 10 : ((k == 1) ? 5 : 1);
                        weights[matID] += w;
                        foundAnySolid = true;
                        break; 
                    }
                }
            }
        }

        if (!foundAnySolid) return AIR;

        int max_w = -1;
        int winner_id = 0;

        for (int i = 0; i < MAX_MATERIAL_TYPES; ++i) {
            if (weights[i] > max_w) {
                max_w = weights[i];
                winner_id = i;
            }
        }
        if (winner_id == 0) return AIR;
        return (winner_id << 3) | 0x07;
    }

    template <typename T, size_t N>
    std::vector<Quad> GenerateQuadsInternal(const MeshingContext& ctx, int step) {
        std::vector<Quad> resultQuads;
        const int GRID_SIZE = N; 

        for (int d = 0; d < 3; ++d) {
            int u_ax = (d + 1) % 3;
            int v_ax = (d + 2) % 3;
            int l_x[3] = {0, 0, 0}; 

            for (l_x[d] = -1; l_x[d] < GRID_SIZE; ++l_x[d]) {
                for (int side = 0; side < 2; ++side) {
                    std::vector<std::array<T, N>> material_masks(MAX_MATERIAL_TYPES, {0});
                    bool any_face = false;

                    for (l_x[v_ax] = 0; l_x[v_ax] < GRID_SIZE; ++l_x[v_ax]) {     
                        for (l_x[u_ax] = 0; l_x[u_ax] < GRID_SIZE; ++l_x[u_ax]) { 
                            int world_boundary_d = (l_x[d] + 1) * step;
                            int world_u = l_x[u_ax] * step;
                            int world_v = l_x[v_ax] * step;

                            VoxelType curr = ScanDominantVoxel(ctx, world_u, world_v, world_boundary_d - 1, u_ax, v_ax, d, step, -1);
                            VoxelType next = ScanDominantVoxel(ctx, world_u, world_v, world_boundary_d,     u_ax, v_ax, d, step,  1);

                            bool cSolid = !IsTransparent(curr);
                            bool nSolid = !IsTransparent(next);
                            VoxelType faceType = AIR;

                            if (side == 0) { 
                                if (l_x[d] >= 0 && l_x[d] < GRID_SIZE) { if (cSolid && !nSolid) faceType = curr; }
                            } else { 
                                int nextPos = l_x[d] + 1;
                                if (nextPos >= 0 && nextPos < GRID_SIZE) { if (!cSolid && nSolid) faceType = next; }
                            }

                            if (faceType != AIR) {
                                int matID = GetMaterialID(faceType);
                                if (matID < MAX_MATERIAL_TYPES) {
                                    material_masks[matID][l_x[v_ax]] |= (static_cast<T>(1) << l_x[u_ax]);
                                    any_face = true;
                                }
                            }
                        }
                    }

                    if (!any_face) continue;

                    for (int matID = 0; matID < MAX_MATERIAL_TYPES; ++matID) {
                        bool empty = true;
                        for(auto val : material_masks[matID]) if(val != 0) { empty = false; break; }
                        if(empty) continue;

                        auto binaryQuads = GreedyMeshBinaryPlane<T, N>(material_masks[matID]);

                        for (const auto& bq : binaryQuads) {
                            Quad q_final;
                            q_final.texLayer = (uint8_t)GetTextureLayer((matID << 3) | 0x07);
                            
                            int l_pos[3]; 
                            l_pos[u_ax] = bq.u; 
                            l_pos[v_ax] = bq.v; 
                            
                            // FIX: Зберігаємо координати в "Блоках" (Grid Units), а не метрах.
                            // Для Side 0 (X+): грань належить блоку l_x[d]
                            // Для Side 1 (X-): грань належить блоку l_x[d] + 1
                            
                            if (side == 0) {
                                l_pos[d] = l_x[d]; 
                                q_final.face = (uint8_t)(d * 2); 
                            } else {
                                l_pos[d] = l_x[d] + 1; 
                                q_final.face = (uint8_t)(d * 2 + 1); 
                            }

                            // Координати завжди в межах [0..N], тому влазять в 5 біт
                            q_final.x = (uint8_t)l_pos[0];
                            q_final.y = (uint8_t)l_pos[1];
                            q_final.z = (uint8_t)l_pos[2];
                            q_final.w = (uint8_t)bq.w_u; 
                            q_final.h = (uint8_t)bq.h_v; 

                            resultQuads.push_back(q_final);
                        }
                    }
                }
            }
        }
        return resultQuads;
    }

    std::vector<Quad> GenerateQuads(const MeshingContext& ctx, int lod) {
        if (lod == 1) return GenerateQuadsInternal<uint32_t, 32>(ctx, 1);
        if (lod == 2) return GenerateQuadsInternal<uint16_t, 16>(ctx, 2);
        if (lod == 3) return GenerateQuadsInternal<uint8_t, 8>(ctx, 4);
        return GenerateQuadsInternal<uint32_t, 32>(ctx, 1);
    }

    std::vector<GpuQuad> BuildSSBOData(const std::vector<Quad>& quads) {
        std::vector<GpuQuad> gpuQuads;
        gpuQuads.reserve(quads.size());
        for (const auto& q : quads) gpuQuads.push_back({ packQuad(q) });
        return gpuQuads;
    }

    int GetChunkLOD(float distance) {
        if (distance > 192.0f) return 3; 
        if (distance > 96.0f) return 2;  
        return 1;
    }
}