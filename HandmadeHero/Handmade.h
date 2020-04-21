#ifndef HANDMADE_H

#include "Handmade_Platform.h"

#define internal_func static
#define local_persist static
#define global_var static

#define Pi32 3.14159265359f

#if HANDMADE_SLOW
#define Assert(expression) if(!(expression)) {*(int *)0 = 0;}
#else
#define Assert(expression)
#endif

#define Kilobytes(value) ((value) * 1024)
#define Megabytes(value) (Kilobytes(value) * 1024)
#define Gigabytes(value) (Megabytes(value) * 1024)
#define Terabytes(value) (Gigabytes(value) * 1024)

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

inline uint32 SafeTruncateUint64(uint64 value)
{
	Assert(value <= 0xFFFFFFFF);
	return (uint32)value;
}

inline game_controller_input* GetController(game_input *input, int controllerIndex)
{
	Assert(controllerIndex < ArrayCount(input->controllers));
	return &input->controllers[controllerIndex];
}

//
//
//

struct memory_arena
{
	memory_index size;
	uint8 *base;
	memory_index used;
};

internal_func void
InitializeArena(memory_arena *arena, memory_index size, uint8 *base)
{
	arena->size = size;
	arena->base = base;
	arena->used = 0;
}

#define PushStruct(arena, type) (type*)PushSize_(arena, sizeof(type))
#define PushArray(arena, count, type) (type *)PushSize_(arena, count*sizeof(type))
void *
PushSize_(memory_arena *arena, memory_index size)
{
	Assert((arena->used + size) <= arena->size);
	void *result = arena->base + arena->used;
	arena->used += size;

	return(result);
}

#include "Handmade_Intrinsics.h"
#include "Handmade_Tile.h"

struct world
{
	tile_map *tileMap;
};
struct loaded_bitmap
{
	int32 width;
	int32 height;
	uint32* pixels;
};

struct hero_bitmaps
{
	int32 alignX;
	int32 alignY;

	loaded_bitmap head;
	loaded_bitmap cape;
	loaded_bitmap torso;
};

struct game_state
{
	memory_arena worldArena;
	world *world;

	tile_map_position cameraP;
	tile_map_position playerP;

	loaded_bitmap backdrop;

	uint32 heroFacingDirection;
	hero_bitmaps heroBitmaps[4];
};

#define HANDMADE_H
#endif