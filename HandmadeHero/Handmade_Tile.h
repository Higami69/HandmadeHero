#pragma once

struct tile_map_position
{
	uint32 absTileX;
	uint32 absTileY;
	uint32 absTileZ;

	real32 offsetX;
	real32 offsetY;
};

struct tile_chunk_position
{
	uint32 tileChunkX;
	uint32 tileChunkY;
	uint32 tileChunkZ;

	uint32 chunkRelTileX;
	uint32 chunkRelTileY;
};

struct tile_chunk
{
	uint32 *tiles;
};

struct tile_map
{
	uint32 chunkShift;
	uint32 chunkMask;
	uint32 chunkDim;

	real32 tileSideInMeters;

	uint32 tileChunkCountX;
	uint32 tileChunkCountY;
	uint32 tileChunkCountZ;
	tile_chunk *tileChunks;
};