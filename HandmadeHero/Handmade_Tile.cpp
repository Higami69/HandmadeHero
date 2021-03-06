inline tile_chunk*
GetTileChunk(tile_map *tileMap, uint32 tileChunkX, uint32 tileChunkY, uint32 tileChunkZ)
{
	tile_chunk *tileChunk = 0;

	if (tileChunkX >= 0 && tileChunkX < tileMap->tileChunkCountX && 
		tileChunkY >= 0 && tileChunkY < tileMap->tileChunkCountY && 
		tileChunkZ >= 0 && tileChunkZ < tileMap->tileChunkCountZ)
	{
		tileChunk = &tileMap->tileChunks[tileChunkZ*tileMap->tileChunkCountY*tileMap->tileChunkCountX + tileChunkY*tileMap->tileChunkCountX + tileChunkX];
	}
	return tileChunk;
}

inline uint32
GetTileValueUnchecked(tile_map *tileMap, tile_chunk *tileChunk, uint32 tileX, uint32 tileY)
{
	Assert(tileChunk);
	Assert(tileX < tileMap->chunkDim);
	Assert(tileY < tileMap->chunkDim);

	uint32 tileChunkValue = tileChunk->tiles[tileY*tileMap->chunkDim + tileX];
	return tileChunkValue;
}
inline void
SetTileValueUnchecked(tile_map *tileMap, tile_chunk *tileChunk, uint32 tileX, uint32 tileY, uint32 tileValue)
{
	Assert(tileChunk);
	Assert(tileX < tileMap->chunkDim);
	Assert(tileY < tileMap->chunkDim);

	tileChunk->tiles[tileY*tileMap->chunkDim + tileX] = tileValue;
}

inline uint32 GetTileValue(tile_map *tileMap, tile_chunk* tileChunk, uint32 testTileX, uint32 testTileY)
{
	uint32 tileChunkValue = 0;
	if (tileChunk && tileChunk->tiles)
	{
		tileChunkValue = GetTileValueUnchecked(tileMap, tileChunk, testTileX, testTileY);
	}

	return tileChunkValue;
}
inline void SetTileValue(tile_map *tileMap, tile_chunk* tileChunk, uint32 testTileX, uint32 testTileY, uint32 tileValue)
{
	if (tileChunk && tileChunk->tiles)
	{
		SetTileValueUnchecked(tileMap, tileChunk, testTileX, testTileY, tileValue);
	}
}

inline tile_chunk_position GetChunkPositionFor(tile_map *tileMap, uint32 absTileX, uint32 absTileY, uint32 absTileZ)
{
	tile_chunk_position result;

	result.tileChunkX = absTileX >> tileMap->chunkShift;
	result.tileChunkY = absTileY >> tileMap->chunkShift;
	result.tileChunkZ = absTileZ;
	result.chunkRelTileX = absTileX & tileMap->chunkMask;
	result.chunkRelTileY = absTileY & tileMap->chunkMask;

	return result;
}

internal_func uint32 GetTileValue(tile_map *tileMap, uint32 absTileX, uint32 absTileY, uint32 absTileZ)
{
	tile_chunk_position chunkPos = GetChunkPositionFor(tileMap, absTileX, absTileY, absTileZ);
	tile_chunk *tileChunk = GetTileChunk(tileMap, chunkPos.tileChunkX, chunkPos.tileChunkY, chunkPos.tileChunkZ);
	uint32 tileChunkValue = GetTileValue(tileMap, tileChunk, chunkPos.chunkRelTileX, chunkPos.chunkRelTileY);

	return tileChunkValue;
}

internal_func uint32 GetTileValue(tile_map *tileMap, tile_map_position pos)
{
	uint32 result = GetTileValue(tileMap, pos.absTileX, pos.absTileY, pos.absTileZ);

	return result;
}

internal_func bool32 IsTileMapPointEmpty(tile_map* tileMap, tile_map_position pos)
{
	uint32 tileChunkValue = GetTileValue(tileMap, pos.absTileX, pos.absTileY, pos.absTileZ);
	bool32 empty = (tileChunkValue == 1) || (tileChunkValue == 3) || (tileChunkValue == 4);

	return empty;
}

internal_func void
SetTileValue(memory_arena *arena, tile_map *tileMap, uint32 absTileX, uint32 absTileY, uint32 absTileZ, uint32 tileValue)
{
	tile_chunk_position chunkPos = GetChunkPositionFor(tileMap, absTileX, absTileY, absTileZ);
	tile_chunk *tileChunk = GetTileChunk(tileMap, chunkPos.tileChunkX, chunkPos.tileChunkY, chunkPos.tileChunkZ);

	Assert(tileChunk);

	if(!tileChunk->tiles)
	{
		uint32 tileCount = tileMap->chunkDim*tileMap->chunkDim;
		tileChunk->tiles = PushArray(arena, tileCount, uint32);
		for (uint32 tileIndex = 0; tileIndex < tileCount; ++tileIndex)
		{
			tileChunk->tiles[tileIndex] = 1;
		}
	}

	SetTileValue(tileMap, tileChunk, chunkPos.chunkRelTileX, chunkPos.chunkRelTileY, tileValue);
}

inline void RecanonicalizeCoord(tile_map *tileMap, uint32 *tile, real32 *tileRel)
{
	int32 offset = RoundReal32ToInt32(*tileRel / tileMap->tileSideInMeters);
	*tile += offset;
	*tileRel -= offset * tileMap->tileSideInMeters;

	Assert(*tileRel >= -0.5f*tileMap->tileSideInMeters);
	Assert(*tileRel <= 0.5f*tileMap->tileSideInMeters);
}

inline tile_map_position
RecanonicalizePosition(tile_map *tileMap, tile_map_position pos)
{
	tile_map_position result = pos;

	RecanonicalizeCoord(tileMap, &result.absTileX, &result.offsetX);
	RecanonicalizeCoord(tileMap, &result.absTileY, &result.offsetY);

	return result;
}

inline bool32 AreOnSameTile(tile_map_position *a, tile_map_position *b)
{
	bool32 result = (a->absTileX == b->absTileX) && (a->absTileY == b->absTileY) && (a->absTileZ == b->absTileZ);

	return result;
}

inline tile_map_difference
Substract(tile_map* tileMap, tile_map_position* a, tile_map_position* b)
{
	tile_map_difference result;

	real32 dTileX = (real32)a->absTileX - (real32)b->absTileX;
	real32 dTileY = (real32)a->absTileY - (real32)b->absTileY;
	real32 dTileZ = (real32)a->absTileZ - (real32)b->absTileZ;

	result.dX = (tileMap->tileSideInMeters*dTileX) + (a->offsetX - b->offsetX);
	result.dY = (tileMap->tileSideInMeters*dTileY) + (a->offsetY - b->offsetY);
	result.dZ = tileMap->tileSideInMeters*dTileZ;

	return result;
}