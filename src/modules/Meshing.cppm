module;
#include <vector>
#include <cstring>
#include <cstdlib> 
#include <cstdint>
#include <iostream>
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
        uint32_t attributeData; // Normal(3), TextureLayer(8), Unused...
    };

    uint32_t packPos(int x, int y, int z) {
        return (x & 0xFF) | ((y & 0xFF) << 8) | ((z & 0xFF) << 16);
    }

    // Pack attributes: Normal (3 bits) + Texture Layer (8 bits)
    uint32_t packAttr(int normal, int texLayer) {
        return (normal & 0x07) | ((texLayer & 0xFF) << 3);
    }

    struct MeshingContext {
        const VoxelChunk* center;
        const VoxelChunk* neighbors[6]; 
    };

    VoxelType get_voxel_context(const MeshingContext& ctx, int x, int y, int z) {
        if (x >= 0 && x < CHUNK_SIZE && y >= 0 && y < CHUNK_SIZE && z >= 0 && z < CHUNK_SIZE) {
            return ctx.center->get(x, y, z);
        }
        
        if (x < 0) {
            if (ctx.neighbors[1]) return ctx.neighbors[1]->get(CHUNK_SIZE - 1, y, z);
            return INTERNAL; 
        }
        if (x >= CHUNK_SIZE) {
            if (ctx.neighbors[0]) return ctx.neighbors[0]->get(0, y, z);
            return INTERNAL;
        }
        if (y < 0) {
            if (ctx.neighbors[3]) return ctx.neighbors[3]->get(x, CHUNK_SIZE - 1, z);
            return INTERNAL;
        }
        if (y >= CHUNK_SIZE) {
            if (ctx.neighbors[2]) return ctx.neighbors[2]->get(x, 0, z);
            return INTERNAL;
        }
        if (z < 0) {
            if (ctx.neighbors[5]) return ctx.neighbors[5]->get(x, y, CHUNK_SIZE - 1);
            return INTERNAL;
        }
        if (z >= CHUNK_SIZE) {
            if (ctx.neighbors[4]) return ctx.neighbors[4]->get(x, y, 0);
            return INTERNAL;
        }
        return AIR; 
    }

    Quad build_quad_impl(const MeshingContext& ctx, int startU, int startV, VoxelType type, 
                         int axisBitU, int axisBitV, bool widthFirst, 
                         const VoxelType* maskPtr, const bool* visitedPtr) 
    {
        Quad q;
        q.type = type;
        q.x = startU; q.y = startV;
        q.w = 1; q.h = 1;

        auto get_mask = [&](int u, int v) { return maskPtr[u + v * CHUNK_SIZE]; };
        auto is_visited = [&](int u, int v) { return visitedPtr[u + v * CHUNK_SIZE]; };

        auto can_merge = [&](int u, int v) {
            if (is_visited(u, v)) return false;
            if (get_mask(u, v) != type) return false;
            return true;
        };

        if (widthFirst) {
            if (type & axisBitU) { 
                while (q.x + q.w < CHUNK_SIZE && can_merge(q.x + q.w, q.y)) q.w++;
            }
            if (type & axisBitV) {
                bool expand = true;
                while (expand && q.y + q.h < CHUNK_SIZE) {
                    int nextV = q.y + q.h;
                    for (int k = 0; k < q.w; ++k) {
                        if (!can_merge(q.x + k, nextV)) { expand = false; break; }
                    }
                    if (expand) q.h++;
                }
            }
        } else {
            if (type & axisBitV) {
                while (q.y + q.h < CHUNK_SIZE && can_merge(q.x, q.y + q.h)) q.h++;
            }
            if (type & axisBitU) {
                bool expand = true;
                while (expand && q.x + q.w < CHUNK_SIZE) {
                    int nextU = q.x + q.w;
                    for (int k = 0; k < q.h; ++k) {
                        if (!can_merge(nextU, q.y + k)) { expand = false; break; }
                    }
                    if (expand) q.w++;
                }
            }
        }
        return q;
    }

    std::vector<Quad> GenerateQuads(const MeshingContext& ctx) {
        std::vector<Quad> resultQuads;
        VoxelType mask[CHUNK_SIZE * CHUNK_SIZE];
        bool visited[CHUNK_SIZE * CHUNK_SIZE];

        for (int d = 0; d < 3; ++d) {
            int u = (d + 1) % 3;
            int v = (d + 2) % 3;
            int x[3] = {0,0,0};
            int q[3] = {0,0,0};
            q[d] = 1;

            int axisFlagU = 0, axisFlagV = 0;
            if (d == 0) { axisFlagU = FLAG_Y; axisFlagV = FLAG_Z; }
            else if (d == 1) { axisFlagU = FLAG_Z; axisFlagV = FLAG_X; }
            else { axisFlagU = FLAG_X; axisFlagV = FLAG_Y; }

            for (x[d] = -1; x[d] < CHUNK_SIZE; ++x[d]) {
                for (int side = 0; side < 2; ++side) {
                    int visibleCount = 0;
                    std::memset(mask, 0, sizeof(mask));

                    for (x[v] = 0; x[v] < CHUNK_SIZE; ++x[v]) {
                        for (x[u] = 0; x[u] < CHUNK_SIZE; ++x[u]) {
                            
                            VoxelType curr = get_voxel_context(ctx, x[0], x[1], x[2]);
                            VoxelType next = get_voxel_context(ctx, x[0] + q[0], x[1] + q[1], x[2] + q[2]);

                            bool cSolid = !IsTransparent(curr);
                            bool nSolid = !IsTransparent(next);
                            
                            int maskIdx = x[u] + x[v] * CHUNK_SIZE;

                            if (side == 0) { 
                                if (x[d] >= 0 && x[d] < CHUNK_SIZE) {
                                    if (cSolid && !nSolid) mask[maskIdx] = curr;
                                }
                            } else { 
                                int nextPos = x[d] + 1;
                                if (nextPos >= 0 && nextPos < CHUNK_SIZE) {
                                    if (!cSolid && nSolid) mask[maskIdx] = next;
                                }
                            }
                            
                            if (mask[maskIdx] != 0) visibleCount++;
                        }
                    }

                    if (visibleCount == 0) continue;

                    std::memset(visited, 0, sizeof(visited));
                    for (int j = 0; j < CHUNK_SIZE; ++j) {
                        for (int i = 0; i < CHUNK_SIZE; ++i) {
                            int idx = i + j * CHUNK_SIZE;
                            VoxelType t = mask[idx];
                            if (t != 0 && !visited[idx]) {
                                Quad q1 = build_quad_impl(ctx, i, j, t, axisFlagU, axisFlagV, true, mask, visited);
                                Quad q2 = build_quad_impl(ctx, i, j, t, axisFlagU, axisFlagV, false, mask, visited);
                                Quad best = ((q1.w * q1.h) >= (q2.w * q2.h)) ? q1 : q2;

                                for (int h = 0; h < best.h; ++h) {
                                    for (int w = 0; w < best.w; ++w) {
                                        visited[(best.x + w) + (best.y + h) * CHUNK_SIZE] = true;
                                    }
                                }

                                best.z = x[d]; 
                                int realPos[3];
                                realPos[u] = best.x;
                                realPos[v] = best.y;
                                realPos[d] = (side == 0) ? x[d] : x[d] + 1;
                                
                                best.x = realPos[0];
                                best.y = realPos[1];
                                best.z = realPos[2];
                                best.face = d * 2 + (side == 1 ? 1 : 0); 

                                resultQuads.push_back(best);
                            }
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
            
            // Отримуємо шар текстури з типу вокселя
            int texLayer = GetTextureLayer(q.type);
            if(texLayer < 0) texLayer = 0; // Fallback

            int face = q.face; // 0..5
            
            struct P { int x,y,z; };
            P p1, p2, p3, p4;

            switch(face) {
                case 0: // X+
                    p1 = {x0+1, y0,   z0+h};
                    p2 = {x0+1, y0,   z0};
                    p3 = {x0+1, y0+w, z0};
                    p4 = {x0+1, y0+w, z0+h};
                    break;
                case 1: // X-
                    p1 = {x0, y0,   z0};
                    p2 = {x0, y0,   z0+h};
                    p3 = {x0, y0+w, z0+h};
                    p4 = {x0, y0+w, z0};
                    break;
                case 2: // Y+
                    p1 = {x0,   y0+1, z0};
                    p2 = {x0,   y0+1, z0+w};
                    p3 = {x0+h, y0+1, z0+w};
                    p4 = {x0+h, y0+1, z0};
                    break;
                case 3: // Y-
                    p1 = {x0,   y0, z0+w};
                    p2 = {x0,   y0, z0};
                    p3 = {x0+h, y0, z0};
                    p4 = {x0+h, y0, z0+w};
                    break;
                case 4: // Z+
                    p1 = {x0,   y0,   z0+1};
                    p2 = {x0+w, y0,   z0+1};
                    p3 = {x0+w, y0+h, z0+1};
                    p4 = {x0,   y0+h, z0+1};
                    break;
                case 5: // Z-
                    p1 = {x0+w, y0,   z0};
                    p2 = {x0,   y0,   z0};
                    p3 = {x0,   y0+h, z0};
                    p4 = {x0+w, y0+h, z0};
                    break;
            }

            uint32_t attr = packAttr(face, texLayer);

            vertices.push_back({ packPos(p1.x, p1.y, p1.z), attr });
            vertices.push_back({ packPos(p2.x, p2.y, p2.z), attr });
            vertices.push_back({ packPos(p3.x, p3.y, p3.z), attr });

            vertices.push_back({ packPos(p1.x, p1.y, p1.z), attr });
            vertices.push_back({ packPos(p3.x, p3.y, p3.z), attr });
            vertices.push_back({ packPos(p4.x, p4.y, p4.z), attr });
        }
        return vertices;
    }
}