module;
#include <vector>
#include <cmath>
#include <algorithm>
#include <tuple>
#include <optional>
export module VoxelGame.World;

import VoxelGame.Types;

using namespace VoxelGame::Types;

export namespace VoxelGame::World {

    // --- Component: Chunk Data ---
    struct VoxelChunk {
        std::vector<VoxelType> voxels;

        VoxelChunk() {
            voxels.resize(CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE, AIR);
        }

        [[nodiscard]] constexpr size_t index(int x, int y, int z) const {
            return x + (y * CHUNK_SIZE) + (z * CHUNK_SIZE * CHUNK_SIZE);
        }

        [[nodiscard]] VoxelType get(int x, int y, int z) const {
            if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE)
                return AIR;
            return voxels[index(x, y, z)];
        }

        void set(int x, int y, int z, VoxelType type) {
            if (x >= 0 && x < CHUNK_SIZE && y >= 0 && y < CHUNK_SIZE && z >= 0 && z < CHUNK_SIZE) {
                voxels[index(x, y, z)] = type;
            }
        }
    };

    // --- System: Terrain Generation ---
    namespace TerrainSystem {
        float lerp(float a, float b, float t) { return a + t * (b - a); }
        float fade(float t) { return t * t * t * (t * (t * 6 - 15) + 10); }
        float grad(int hash, float x, float y) {
            int h = hash & 15; float u = h<8?x:y; float v = h<4?y:(h==12||h==14?x:0);
            return ((h&1)==0?u:-u)+((h&2)==0?v:-v);
        }
        float perlin(float x, float z, int seed) {
            auto p = [&](int i) { i+=seed; i=(i<<13)^i; return (i*(i*i*15731+789221)+1376312589)&255; };
            int X=(int)floor(x)&255, Z=(int)floor(z)&255;
            x-=floor(x); z-=floor(z);
            float u=fade(x), v=fade(z);
            return lerp(lerp(grad(p(X)+Z,x,z), grad(p(X+1)+Z,x-1,z),u), lerp(grad(p(X)+Z+1,x,z-1), grad(p(X+1)+Z+1,x-1,z-1),u),v);
        }

        void Generate(VoxelChunk& chunk, int seed, int chunkX, int chunkZ) {
            std::fill(chunk.voxels.begin(), chunk.voxels.end(), AIR);

            for (int x = 0; x < CHUNK_SIZE; ++x) {
                for (int z = 0; z < CHUNK_SIZE; ++z) {
                    float gx = (float)(chunkX * CHUNK_SIZE + x);
                    float gz = (float)(chunkZ * CHUNK_SIZE + z);
                    
                    float noiseVal = perlin(gx * 0.15f, gz * 0.15f, seed);
                    int height = 8 + (int)(noiseVal * 8.0f);
                    height = std::clamp(height, 1, CHUNK_SIZE - 1);

                    for (int y = 0; y <= height; ++y) {
                        if (y == height) {
                            if (y < 6) chunk.set(x, y, z, GRASS); 
                            else if (y < 12) chunk.set(x, y, z, DIRT); 
                            else chunk.set(x, y, z, SNOW);
                        } else {
                            chunk.set(x, y, z, INTERNAL);
                        }
                    }
                    
                    if (x > 5 && x < 10 && z == 5 && height < 20) {
                         for(int ph = 1; ph <= 5; ph++) 
                            chunk.set(x, height + ph, z, PILLAR);
                    }

                    if (z == 20 && x > 10 && x < 25 && height < 20) {
                        for(int wh = 1; wh <= 3; wh++)
                            chunk.set(x, height + wh, z, WALL_X);
                    }
                }
            }
        }
    }
}