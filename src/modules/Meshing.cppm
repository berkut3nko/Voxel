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
        VoxelType type;
        int x, y, z;
        int w, h;
        int face; 
    };

    struct GpuQuad {
        uint32_t data1; 
        uint32_t data2; 
    };

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

    template <typename T, size_t N>
    std::vector<BinaryQuad> GreedyMeshBinaryPlane(std::array<T, N> data) {
        std::vector<BinaryQuad> quads;
        using UT = std::make_unsigned_t<T>;
        
        for (int row = 0; row < N; ++row) {
            while (data[row] != 0) {
                int u = std::countr_zero(static_cast<UT>(data[row]));
                int w_u = std::countr_one(static_cast<UT>(data[row] >> u));
                UT h_mask_bits = (w_u == sizeof(T)*8) ? static_cast<UT>(~0) : ((static_cast<UT>(1) << w_u) - 1);
                UT mask = h_mask_bits << u;
                
                int h_v = 1; 
                while (row + h_v < N) {
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
        if (x < 0) return (ctx.neighbors[1]) ? ctx.neighbors[1]->get(CHUNK_SIZE - 1, y, z) : INTERNAL; 
        if (x >= CHUNK_SIZE) return (ctx.neighbors[0]) ? ctx.neighbors[0]->get(0, y, z) : INTERNAL;
        if (y < 0) return (ctx.neighbors[3]) ? ctx.neighbors[3]->get(x, CHUNK_SIZE - 1, z) : INTERNAL;
        if (y >= CHUNK_SIZE) return (ctx.neighbors[2]) ? ctx.neighbors[2]->get(x, 0, z) : INTERNAL;
        if (z < 0) return (ctx.neighbors[5]) ? ctx.neighbors[5]->get(x, y, CHUNK_SIZE - 1) : INTERNAL;
        if (z >= CHUNK_SIZE) return (ctx.neighbors[4]) ? ctx.neighbors[4]->get(x, y, 0) : INTERNAL;
        return AIR; 
    }

    // --- Сканування вглиб для пошуку поверхневого матеріалу ---
    // d_start - координата початку сканування (поверхня)
    // scan_dir - напрямок сканування вглиб блоку (-1 або +1)
    VoxelType ScanDominantVoxel(const MeshingContext& ctx, 
                               int u_start, int v_start, int d_start,
                               int u_ax, int v_ax, int d_ax, 
                               int step, int scan_dir) {
        // Оптимізація для LOD 1 (step=1) - просто беремо воксель
        if (step == 1) {
            int pos[3];
            pos[u_ax] = u_start;
            pos[v_ax] = v_start;
            pos[d_ax] = d_start;
            return get_voxel_context(ctx, pos[0], pos[1], pos[2]);
        }

        int counts[MAX_MATERIAL_TYPES + 1] = {0};
        
        // Ітеруємося по поверхні грані (step x step)
        for (int du = 0; du < step; ++du) {
            for (int dv = 0; dv < step; ++dv) {
                
                // Raycast вглиб для кожного "пікселя" супер-вокселя
                for (int k = 0; k < step; ++k) {
                    int pos[3];
                    pos[u_ax] = u_start + du;
                    pos[v_ax] = v_start + dv;
                    pos[d_ax] = d_start + (k * scan_dir); 

                    VoxelType t = get_voxel_context(ctx, pos[0], pos[1], pos[2]);
                    
                    // Якщо знайшли непрозорий блок - це поверхня для цього променя
                    if (!IsTransparent(t)) {
                        int matID = GetMaterialID(t);
                        if (matID >= MAX_MATERIAL_TYPES) matID = 0;
                        
                        counts[matID]++;
                        break; // Зупиняємося, бо глибші вокселі закриті цим
                    }
                    // Якщо прозорий (AIR), продовжуємо йти вглиб (k++)
                }
            }
        }

        // Знаходимо переможця (Majority Vote)
        int max_count = -1;
        int winner_id = 0;

        for (int i = 0; i < MAX_MATERIAL_TYPES; ++i) {
            if (counts[i] > max_count) {
                max_count = counts[i];
                winner_id = i;
            }
        }

        // Якщо перемогло повітря або нічого не знайдено
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
            int q[3] = {0, 0, 0}; 
            q[d] = 1;

            for (l_x[d] = -1; l_x[d] < GRID_SIZE; ++l_x[d]) {
                for (int side = 0; side < 2; ++side) {
                    
                    std::vector<std::array<T, N>> material_masks(MAX_MATERIAL_TYPES, {0});
                    bool any_face = false;

                    for (l_x[v_ax] = 0; l_x[v_ax] < GRID_SIZE; ++l_x[v_ax]) {     
                        for (l_x[u_ax] = 0; l_x[u_ax] < GRID_SIZE; ++l_x[u_ax]) { 
                            
                            // Світова координата межі між супер-вокселями
                            int world_boundary_d = (l_x[d] + 1) * step;

                            int world_u = l_x[u_ax] * step;
                            int world_v = l_x[v_ax] * step;

                            // curr: знаходиться "зліва" від межі. 
                            // Ми дивимось на його грань +d. Отже, скануємо від межі (boundary-1) ВНИЗ/ВЛІВО (-1).
                            VoxelType curr = ScanDominantVoxel(ctx, world_u, world_v, world_boundary_d - 1, u_ax, v_ax, d, step, -1);
                            
                            // next: знаходиться "справа" від межі.
                            // Ми дивимось на його грань -d. Отже, скануємо від межі (boundary) ВВЕРХ/ВПРАВО (+1).
                            VoxelType next = ScanDominantVoxel(ctx, world_u, world_v, world_boundary_d,     u_ax, v_ax, d, step,  1);

                            bool cSolid = !IsTransparent(curr);
                            bool nSolid = !IsTransparent(next);
                            
                            VoxelType faceType = AIR;

                            if (side == 0) { 
                                if (l_x[d] >= 0 && l_x[d] < GRID_SIZE) {
                                    if (cSolid && !nSolid) faceType = curr;
                                }
                            } else { 
                                int nextPos = l_x[d] + 1;
                                if (nextPos >= 0 && nextPos < GRID_SIZE) {
                                    if (!cSolid && nSolid) faceType = next;
                                }
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
                        auto binaryQuads = GreedyMeshBinaryPlane<T, N>(material_masks[matID]);

                        for (const auto& bq : binaryQuads) {
                            Quad q_final;
                            q_final.type = (matID << 3) | 0x07; 
                            
                            int l_pos[3]; 
                            l_pos[u_ax] = bq.u; 
                            l_pos[v_ax] = bq.v; 
                            
                            if (side == 0) {
                                l_pos[d] = l_x[d] + 1; 
                                q_final.face = d * 2; 
                            } else {
                                l_pos[d] = l_x[d] + 1; 
                                q_final.face = d * 2 + 1; 
                            }

                            q_final.x = l_pos[0] * step;
                            q_final.y = l_pos[1] * step;
                            q_final.z = l_pos[2] * step;
                            q_final.w = bq.w_u * step; 
                            q_final.h = bq.h_v * step; 

                            resultQuads.push_back(q_final);
                        }
                    }
                }
            }
        }
        return resultQuads;
    }

    std::vector<Quad> GenerateQuads(const MeshingContext& ctx, int lod) {
        if (lod == 1) {
            return GenerateQuadsInternal<uint32_t, 32>(ctx, 1);
        }
        else if (lod == 2) {
            return GenerateQuadsInternal<uint16_t, 16>(ctx, 2);
        }
        else if (lod == 3) {
            return GenerateQuadsInternal<uint8_t, 8>(ctx, 4);
        }
        return GenerateQuadsInternal<uint32_t, 32>(ctx, 1);
    }

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

    int GetChunkLOD(float distance) {
        if (distance > 192.0f) return 3; 
        if (distance > 96.0f) return 2;  
        return 1;
    }
}