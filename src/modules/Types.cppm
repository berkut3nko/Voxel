module;
#include <cstdint>
export module VoxelGame.Types;

export namespace VoxelGame::Types {
    using VoxelType = uint8_t;

    // Маски бітів
    constexpr VoxelType MASK_TYPE     = 0xF8; 
    constexpr VoxelType MASK_FLAGS    = 0x07; 

    constexpr VoxelType FLAG_X        = 0x04;
    constexpr VoxelType FLAG_Y        = 0x02;
    constexpr VoxelType FLAG_Z        = 0x01;
    constexpr VoxelType FLAG_SOLID    = 0x80; 

    // Типи блоків
    constexpr VoxelType AIR      = 0x00; 
    constexpr VoxelType GRASS    = (1 << 3) | 0x07; 
    constexpr VoxelType DIRT     = (2 << 3) | 0x07; 
    constexpr VoxelType SNOW     = (3 << 3) | 0x07;
    constexpr VoxelType INTERNAL = (4 << 3) | 0x07; 
    
    constexpr VoxelType PILLAR   = (5 << 3) | 0x02; 
    constexpr VoxelType WALL_X   = (6 << 3) | 0x06; 
    constexpr VoxelType SLAB_XZ  = (7 << 3) | 0x05; 

    constexpr int CHUNK_SIZE = 32;

    inline bool IsTransparent(VoxelType voxel) {
        return (voxel & MASK_FLAGS) == 0;
    }
}