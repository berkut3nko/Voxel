module;
#include <vector>
#include <map>
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
        int chunkX, chunkZ; // World coordinates of the chunk

        VoxelChunk(int cx = 0, int cz = 0) : chunkX(cx), chunkZ(cz) {
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

    // --- World Container ---
    struct WorldManager {
        // Simple map: key = (x, z), value = Chunk
        // Using a flat map or hashing for simplicity
        std::map<std::pair<int, int>, VoxelChunk> chunks;

        VoxelChunk* getChunk(int cx, int cz) {
            auto it = chunks.find({cx, cz});
            if (it != chunks.end()) {
                return &it->second;
            }
            return nullptr;
        }

        VoxelChunk& createChunk(int cx, int cz) {
            // Emplace constructs in place
            auto [it, inserted] = chunks.try_emplace({cx, cz}, cx, cz);
            return it->second;
        }
        
        void clear() {
            chunks.clear();
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

        void Generate(VoxelChunk& chunk, int seed) {
            std::fill(chunk.voxels.begin(), chunk.voxels.end(), AIR);

            for (int x = 0; x < CHUNK_SIZE; ++x) {
                for (int z = 0; z < CHUNK_SIZE; ++z) {
                    // Global coordinates for smooth noise across chunks
                    float gx = (float)(chunk.chunkX * CHUNK_SIZE + x);
                    float gz = (float)(chunk.chunkZ * CHUNK_SIZE + z);
                    
                    // Lower frequency for wider hills
                    float noiseVal = perlin(gx * 0.05f, gz * 0.05f, seed);
                    int height = 10 + (int)(noiseVal * 10.0f);
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
                    
                    // Simple features relative to chunk logic just for demo
                    // We disable complex structures spanning chunks for simplicity here
                    if (height < 20 && (x+z)%15 == 0) {
                         for(int ph = 1; ph <= 3; ph++) 
                            chunk.set(x, height + ph, z, PILLAR);
                    }
                }
            }
        }
    }
}