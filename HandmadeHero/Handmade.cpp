#include "Handmade.h"
#include "Handmade_Tile.cpp"
#include "Handmade_Random.h"

internal_func void GameOutputSound(game_state* gameState, game_soundOutput_buffer *pSoundBuffer, int toneHz)
{
	int16 toneVolume = 3000;
	int wavePeriod = pSoundBuffer->samplesPerSecond / toneHz;

	int16 *pSampleOut = pSoundBuffer->pSamples;
	for (int sampleIndex = 0; sampleIndex < pSoundBuffer->sampleCount; ++sampleIndex)
	{
#if 0
		real32 sineValue = sinf(gameState->tSine);
		int16 sampleValue = int16(sineValue * toneVolume);
#else
		int16 sampleValue = 0;
#endif
		*pSampleOut++ = sampleValue;
		*pSampleOut++ = sampleValue;

#if 0
		gameState->tSine += (2.f * Pi32 * 1.f) / (real32)wavePeriod;
		if(gameState->tSine > 2.f*Pi32)
		{
			gameState->tSine -= 2.0f*Pi32;
		}
#endif
	}
}

internal_func void
DrawRectangle(game_offscreen_buffer *buffer, real32 realMinX, real32 realMinY, real32 realMaxX, real32 realMaxY, real32 r, real32 g, real32 b)
{
	int32 minX = RoundReal32ToInt32(realMinX);
	int32 minY = RoundReal32ToInt32(realMinY);
	int32 maxX = RoundReal32ToInt32(realMaxX);
	int32 maxY = RoundReal32ToInt32(realMaxY);

	if(minX < 0)
	{
		minX = 0;
	}
	if(minY < 0)
	{
		minY = 0;
	}
	if(maxX > buffer->width)
	{
		maxX = buffer->width;
	}
	if(maxY > buffer->height)
	{
		maxY = buffer->height;
	}

	uint32 color = RoundReal32ToUint32(r * 255.f) << 16 | RoundReal32ToUint32(g * 255.f) << 8 | RoundReal32ToUint32(b * 255.f) << 0;

	uint8* row = (uint8*)buffer->memory + minX * buffer->bytesPerPixel + minY * buffer->pitch;

	for(int y = minY; y < maxY; ++y)
	{
		uint32 *pixel = (uint32*)row;
		for (int x = minX; x < maxX; ++x)
		{
			*pixel++ = color;
		}
		row += buffer->pitch;
	}
}

internal_func void
DrawBitmap(game_offscreen_buffer* buffer, loaded_bitmap* bitmap, real32 realX, real32 realY, int32 alignX = 0, int32 alignY = 0)
{
	realX -= (real32)alignX;
	realY -= (real32)alignY;

	int32 minX = RoundReal32ToInt32(realX);
	int32 minY = RoundReal32ToInt32(realY);
	int32 maxX = RoundReal32ToInt32(realX + bitmap->width);
	int32 maxY = RoundReal32ToInt32(realY + bitmap->height);

	int32 sourceOffsetX = 0;
	if (minX < 0)
	{
		sourceOffsetX = -minX;
		minX = 0;
	}

	int32 sourceOffsetY = 0;
	if (minY < 0)
	{
		sourceOffsetY = -minY;
		minY = 0;
	}
	if (maxX > buffer->width)
	{
		maxX = buffer->width;
	}
	if (maxY > buffer->height)
	{
		maxY = buffer->height;
	}

	//TODO: SourceRow needs to be changed based on clipping
	uint32* sourceRow = bitmap->pixels + (bitmap->width * (bitmap->height - 1));
	sourceRow += -(bitmap->width * sourceOffsetY) + sourceOffsetX;
	uint8* destRow = (uint8*)buffer->memory + minX * buffer->bytesPerPixel + minY * buffer->pitch;
	for (int32 y = minY; y < maxY; ++y)
	{
		uint32* dest = (uint32*)destRow;
		uint32* source = (uint32*)sourceRow;
		for (int32 x = minX; x < maxX; ++x)
		{
			real32 a = (real32)((*source >> 24) & 0xFF) / 255.0f;
			real32 sr = (real32)((*source >> 16) & 0xFF);
			real32 sg = (real32)((*source >> 8) & 0xFF);
			real32 sb = (real32)((*source >> 0) & 0xFF);

			real32 dr = (real32)((*dest >> 16) & 0xFF);
			real32 dg = (real32)((*dest >> 8) & 0xFF);
			real32 db = (real32)((*dest >> 0) & 0xFF);

			real32 r = ((1.0f - a) * dr) + (a * sr);
			real32 g = ((1.0f - a) * dg) + (a * sg);
			real32 b = ((1.0f - a) * db) + (a * sb);

			*dest = (((uint32)(r + 0.5f)) << 16 |
					 ((uint32)(g + 0.5f)) << 8  |
					 ((uint32)(b + 0.5f)) << 0  );

			++dest;
			++source;
		}

		destRow += buffer->pitch;
		sourceRow -= bitmap->width;
	}
}

#pragma pack(push, 1)
struct bitmap_header
{
	uint16 fileType;
	uint32 fileSize;
	uint16 reserved1;
	uint16 reserved2;
	uint32 bitmapOffset;
	uint32 size;
	int32 width;
	int32 height;
	uint16 planes;
	uint16 bitsPerPixel;
	uint32 compression;
	uint32 sizeOfBitmap;
	int32 horzResolution;
	int32 vertResolution;
	uint32 colorsUsed;
	uint32 colorsImportant;

	uint32 redMask;
	uint32 greenMask;
	uint32 blueMask;
};
#pragma pack(pop)

internal_func loaded_bitmap
DEBUGLoadBMP(thread_context *thread, debug_platform_read_entire_file *ReadEntireFile, char *fileName)
{
	loaded_bitmap result = {};

	//Byte order in memory: AA BB GG RR, bottom up

	debug_read_file_result readResult = ReadEntireFile(thread, fileName);
	if (readResult.contentsSize != 0)
	{
		bitmap_header *header = (bitmap_header *)readResult.contents;
		uint32 *pixels = (uint32 *)((uint8 *)readResult.contents + header->bitmapOffset);
		result.pixels = pixels;
		result.width = header->width;
		result.height = header->height;

		Assert(header->compression == 3);

		uint32 redMask = header->redMask;
		uint32 greenMask = header->greenMask;
		uint32 blueMask = header->blueMask;
		uint32 alphaMask = ~(redMask | greenMask | blueMask);

		bit_scan_result redShift = FindLeastSignificantSetBit(redMask);
		bit_scan_result greenShift = FindLeastSignificantSetBit(greenMask);
		bit_scan_result blueShift = FindLeastSignificantSetBit(blueMask);
		bit_scan_result alphaShift = FindLeastSignificantSetBit(alphaMask);

		Assert(redShift.found);
		Assert(greenShift.found);
		Assert(blueShift.found);
		Assert(alphaShift.found);

		uint32* sourceDest = pixels;
		for (int32 y = 0; y < header->height; ++y)
		{
			for (int32 x = 0; x < header->width; ++x)
			{
				uint32 c = *sourceDest;
				*sourceDest++ = (((c >> alphaShift.index) & 0xFF) << 24) |
								(((c >> redShift.index) & 0xFF) << 16) |
								(((c >> greenShift.index) & 0xFF) << 8) |
								(((c >> blueShift.index) & 0xFF) << 0);
			}
		}
	}

	return result;
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
	Assert(&input->controllers[0].terminator - &input->controllers[0].buttons[0] == ArrayCount(input->controllers[0].buttons));
	Assert(sizeof(game_state) <= memory->permanentStorageSize);

	real32 playerHeight = 1.4f;
	real32 playerWidth = playerHeight * 0.75f;

	game_state* gameState = (game_state*)memory->pPermanentStorage;
	if(!memory->isInitialized)
	{
		gameState->backdrop = DEBUGLoadBMP(thread, memory->DEBUG_PlatformReadEntireFile, (char*)"test/test_background.bmp");

		hero_bitmaps* bitmap;

		bitmap = gameState->heroBitmaps;
		bitmap->head = DEBUGLoadBMP(thread, memory->DEBUG_PlatformReadEntireFile, (char*)"test/test_hero_right_head.bmp");
		bitmap->cape = DEBUGLoadBMP(thread, memory->DEBUG_PlatformReadEntireFile, (char*)"test/test_hero_right_cape.bmp");
		bitmap->torso = DEBUGLoadBMP(thread, memory->DEBUG_PlatformReadEntireFile, (char*)"test/test_hero_right_torso.bmp");
		bitmap->alignX = 72;
		bitmap->alignY = 182;
		++bitmap;

		bitmap->head = DEBUGLoadBMP(thread, memory->DEBUG_PlatformReadEntireFile, (char*)"test/test_hero_back_head.bmp");
		bitmap->cape = DEBUGLoadBMP(thread, memory->DEBUG_PlatformReadEntireFile, (char*)"test/test_hero_back_cape.bmp");
		bitmap->torso = DEBUGLoadBMP(thread, memory->DEBUG_PlatformReadEntireFile, (char*)"test/test_hero_back_torso.bmp");
		bitmap->alignX = 72;
		bitmap->alignY = 182;
		++bitmap;

		bitmap->head = DEBUGLoadBMP(thread, memory->DEBUG_PlatformReadEntireFile, (char*)"test/test_hero_left_head.bmp");
		bitmap->cape = DEBUGLoadBMP(thread, memory->DEBUG_PlatformReadEntireFile, (char*)"test/test_hero_left_cape.bmp");
		bitmap->torso = DEBUGLoadBMP(thread, memory->DEBUG_PlatformReadEntireFile, (char*)"test/test_hero_left_torso.bmp");
		bitmap->alignX = 72;
		bitmap->alignY = 182;
		++bitmap;

		bitmap->head = DEBUGLoadBMP(thread, memory->DEBUG_PlatformReadEntireFile, (char*)"test/test_hero_front_head.bmp");
		bitmap->cape = DEBUGLoadBMP(thread, memory->DEBUG_PlatformReadEntireFile, (char*)"test/test_hero_front_cape.bmp");
		bitmap->torso = DEBUGLoadBMP(thread, memory->DEBUG_PlatformReadEntireFile, (char*)"test/test_hero_front_torso.bmp");
		bitmap->alignX = 72;
		bitmap->alignY = 182;

		gameState->cameraP.absTileX = 17 / 2;
		gameState->cameraP.absTileY = 9 / 2;

		gameState->playerP.absTileX = 1;
		gameState->playerP.absTileY = 3;
		gameState->playerP.offsetX = 5;
		gameState->playerP.offsetY = 0;

		InitializeArena(&gameState->worldArena, memory->permanentStorageSize - sizeof(game_state), 
			(uint8 *)memory->pPermanentStorage +sizeof(game_state));

		gameState->world = PushStruct(&gameState->worldArena, world);
		world *world = gameState->world;
		world->tileMap = PushStruct(&gameState->worldArena, tile_map);

		tile_map *tileMap = world->tileMap;

		tileMap->chunkShift = 4;
		tileMap->chunkMask = (1 << tileMap->chunkShift) - 1;
		tileMap->chunkDim = (1 << tileMap->chunkShift);

		tileMap->tileChunkCountX = 128;
		tileMap->tileChunkCountY = 128;
		tileMap->tileChunkCountZ = 2;
		tileMap->tileChunks = PushArray(&gameState->worldArena, tileMap->tileChunkCountX*tileMap->tileChunkCountY*tileMap->tileChunkCountZ, tile_chunk);

		tileMap->tileSideInMeters = 1.4f;

		uint32 randomNumberIndex = 0;
		uint32 tilesPerWidth = 17;
		uint32 tilesPerHeight = 9;
		uint32 screenX = 0;
		uint32 screenY = 0;
		uint32 absTileZ = 0;

		bool32 doorLeft = false;
		bool32 doorRight = false;
		bool32 doorTop = false;
		bool32 doorBottom = false;
		bool32 doorUp = false;
		bool32 doorDown = false;
		for(uint32 screenIndex = 0; screenIndex < 100; ++screenIndex)
		{ 
			//TODO: Random number generator
			Assert(randomNumberIndex < ArrayCount(randomNumberTable));
			uint32 randomChoice;
			if(doorUp || doorDown)
			{
				randomChoice = randomNumberTable[randomNumberIndex++] % 2;
			}
			else
			{
				randomChoice = randomNumberTable[randomNumberIndex++] % 3;
			}

			bool32 createdZDoor = false;
			if(randomChoice == 2)
			{
				createdZDoor = true;
				if(absTileZ == 0)
				{
					doorUp = true;
				}
				else
				{
					doorDown = true;
				}
			}
			else if(randomChoice == 1)
			{
				doorRight = true;
			}
			else
			{
				doorTop = true;
			}

			for(uint32 tileY = 0; tileY < tilesPerHeight; ++tileY)
			{
				for(uint32 tileX = 0; tileX < tilesPerWidth; ++tileX)
				{
					uint32 absTileX = (screenX * tilesPerWidth) + tileX;
					uint32 absTileY = (screenY * tilesPerHeight) + tileY;

					uint32 tileValue = 1;
					if (tileX == 0 && (!doorLeft || tileY != (tilesPerHeight / 2)))
					{
						tileValue = 2;
					}
					
					if((tileX == (tilesPerWidth - 1)) && (!doorRight || tileY != tilesPerHeight / 2))
					{
						tileValue = 2;
					}

					if (tileY == 0 && (!doorBottom || tileX != tilesPerWidth / 2))
					{
						tileValue = 2;
					}

					if((tileY == (tilesPerHeight - 1)) && (!doorTop || tileX != tilesPerWidth / 2))
					{
						tileValue = 2;
					}

					if(tileX == 10 && tileY == 6)
					{
						if (doorUp)
						{
							tileValue = 3;
						}

						if (doorDown)
						{
							tileValue = 4;
						}
					}

					SetTileValue(&gameState->worldArena, world->tileMap, absTileX, absTileY, absTileZ, tileValue);
				}
			}

			doorLeft = doorRight;
			doorBottom = doorTop;

			if (createdZDoor)
			{
				doorUp = !doorUp;
				doorDown = !doorDown;
			}
			else
			{
				doorUp = false;
				doorDown = false;
			}

			doorRight = false;
			doorTop = false;

			if(randomChoice == 2)
			{
				if(absTileZ == 0)
				{
					absTileZ = 1;
				}
				else
				{
					absTileZ = 0;
				}
			}
			else if(randomChoice == 1)
			{
				screenX += 1;
			}
			else
			{
				screenY += 1;
			}
		}

		memory->isInitialized = true;
	}

	world *world = gameState->world;
	tile_map *tileMap = world->tileMap;

	int32 tileSideInPixels = 60;
	real32 metersToPixels = (real32)tileSideInPixels / (real32)tileMap->tileSideInMeters;

	for (int controllerIdx = 0; controllerIdx < ArrayCount(input->controllers); controllerIdx++)
	{
		game_controller_input *pController = GetController(input, controllerIdx);
		if (pController->isAnalog)
		{
		}
		else
		{
			real32 dPlayerX = 0.f;
			real32 dPlayerY = 0.f;

			if(pController->moveUp.endedDown)
			{
				gameState->heroFacingDirection = 1;
				dPlayerY = 1.f;
			}
			if (pController->moveDown.endedDown)
			{
				gameState->heroFacingDirection = 3;
				dPlayerY = -1.f;
			}
			if (pController->moveRight.endedDown)
			{
				gameState->heroFacingDirection = 0;
				dPlayerX = 1.f;
			}
			if (pController->moveLeft.endedDown)
			{
				gameState->heroFacingDirection = 2;
				dPlayerX = -1.f;
			}
			real32 playerSpeed = 2.0f;

			if(pController->actionUp.endedDown)
			{
				playerSpeed = 10.f;
			}
			dPlayerX *= playerSpeed;
			dPlayerY *= playerSpeed;

			tile_map_position newPlayerP = gameState->playerP;
			newPlayerP.offsetX += dPlayerX * input->dtForFrame;
			newPlayerP.offsetY += dPlayerY * input->dtForFrame;
			newPlayerP = RecanonicalizePosition(tileMap, newPlayerP);

			tile_map_position playerLeft = newPlayerP;
			playerLeft.offsetX -= 0.5f*playerWidth;
			playerLeft = RecanonicalizePosition(tileMap, playerLeft);

			tile_map_position playerRight = newPlayerP;
			playerRight.offsetX += 0.5f*playerWidth;
			playerRight = RecanonicalizePosition(tileMap, playerRight);

			if(IsTileMapPointEmpty(tileMap, newPlayerP) &&  IsTileMapPointEmpty(tileMap, playerLeft) && IsTileMapPointEmpty(tileMap, playerRight))
			{
				if(!AreOnSameTile(&gameState->playerP, &newPlayerP))
				{
					uint32 newTileValue = GetTileValue(tileMap, newPlayerP);
					if(newTileValue == 3)
					{
						++newPlayerP.absTileZ;
					}
					else if(newTileValue == 4)
					{
						--newPlayerP.absTileZ;
					}
				}
				gameState->playerP = newPlayerP;
			}

			gameState->cameraP.absTileZ = gameState->playerP.absTileZ;

			tile_map_difference diff = Substract(tileMap, &gameState->playerP, &gameState->cameraP);
			if (diff.dX > 9.0f * tileMap->tileSideInMeters)
			{
				gameState->cameraP.absTileX += 17;
			}
			if (diff.dX < -9.0f * tileMap->tileSideInMeters)
			{
				gameState->cameraP.absTileX -= 17;
			}
			if (diff.dY > 5.0f * tileMap->tileSideInMeters)
			{
				gameState->cameraP.absTileY += 9;
			}
			if (diff.dY < -5.0f * tileMap->tileSideInMeters)
			{
				gameState->cameraP.absTileY -= 9;
			}

			gameState->cameraP.absTileX;
			gameState->cameraP.absTileY;
		}
	}

	DrawBitmap(buffer, &gameState->backdrop, 0, 0);

	real32 screenCenterX = 0.5f*(real32)buffer->width;
	real32 screenCenterY = 0.5f*(real32)buffer->height;

	for(int32 relRow =  -10; relRow < 10; ++relRow)
	{
		for(int32 relColumn = -20; relColumn < 20; ++relColumn)
		{
			uint32 column = relColumn + gameState->cameraP.absTileX;
			uint32 row = relRow + gameState->cameraP.absTileY;
			uint32 tileID = GetTileValue(tileMap, column, row, gameState->cameraP.absTileZ);

			if (tileID > 1)
			{
				real32 gray = 0.5f;
				if (tileID == 2) gray = 1.f;

				if (tileID > 2) gray = 0.25f;

				if (row == gameState->cameraP.absTileY && column == gameState->cameraP.absTileX) gray = 0;

				real32 cenX = screenCenterX - metersToPixels*gameState->cameraP.offsetX + ((real32)relColumn)*tileSideInPixels;
				real32 cenY = screenCenterY + metersToPixels*gameState->cameraP.offsetY - ((real32)relRow)*tileSideInPixels;
				real32 minX = cenX - 0.5f*tileSideInPixels;
				real32 minY = cenY - 0.5f*tileSideInPixels;
				real32 maxX = cenX + 0.5f*tileSideInPixels;
				real32 maxY = cenY + 0.5f*tileSideInPixels;

				DrawRectangle(buffer, minX, minY, maxX, maxY, gray, gray, gray);
			}
		}
	}

	tile_map_difference diff = Substract(tileMap, &gameState->playerP, &gameState->cameraP);

	real32 playerR = 1.f;
	real32 playerG = 1.f;
	real32 playerB = 0.f;

	real32 playerGroundPointX = screenCenterX + (metersToPixels*diff.dX);
	real32 playerGroundPointY = screenCenterY - (metersToPixels*diff.dY);

	real32 playerLeft = playerGroundPointX - metersToPixels*0.5f*playerWidth;
	real32 playerTop = playerGroundPointY - metersToPixels*playerHeight;

	DrawRectangle(buffer, playerLeft, playerTop, playerLeft + metersToPixels * playerWidth, playerTop + metersToPixels * playerHeight, playerR, playerG, playerB);

	hero_bitmaps* pHeroBitmaps = &gameState->heroBitmaps[gameState->heroFacingDirection];
	DrawBitmap(buffer, &pHeroBitmaps->torso, playerGroundPointX, playerGroundPointY, pHeroBitmaps->alignX, pHeroBitmaps->alignY);
	DrawBitmap(buffer, &pHeroBitmaps->cape, playerGroundPointX, playerGroundPointY, pHeroBitmaps->alignX, pHeroBitmaps->alignY);
	DrawBitmap(buffer, &pHeroBitmaps->head, playerGroundPointX, playerGroundPointY, pHeroBitmaps->alignX, pHeroBitmaps->alignY);
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
	game_state* gameState = (game_state*)memory->pPermanentStorage;

	GameOutputSound(gameState, soundBuffer, 400);
}