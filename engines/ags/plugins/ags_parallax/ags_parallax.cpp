/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or(at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "ags/plugins/ags_parallax/ags_parallax.h"
#include "ags/shared/core/platform.h"

namespace AGS3 {
namespace Plugins {
namespace AGSParallax {

const unsigned int Magic = 0xCAFE0000;
const unsigned int Version = 2;
const unsigned int SaveMagic = Magic + Version;

IAGSEngine *AGSParallax::_engine;
int AGSParallax::_screenWidth;
int AGSParallax::_screenHeight;
int AGSParallax::_screenColorDepth;
bool AGSParallax::_enabled;
Sprite AGSParallax::_sprites[MAX_SPRITES];


AGSParallax::AGSParallax() : DLL() {
	_engine = nullptr;
	_screenWidth = 320;
	_screenHeight = 200;
	_screenColorDepth = 32;
	_enabled = false;

	DLL_METHOD(AGS_GetPluginName);
	DLL_METHOD(AGS_EngineStartup);
	DLL_METHOD(AGS_EngineOnEvent);
}

const char *AGSParallax::AGS_GetPluginName() {
	return "Parallax plugin recreation";
}

void AGSParallax::AGS_EngineStartup(IAGSEngine *engine) {
	_engine = engine;

	if (_engine->version < 13)
		_engine->AbortGame("Engine interface is too old, need newer version of AGS.");

	SCRIPT_METHOD("pxDrawSprite");
	SCRIPT_METHOD("pxDeleteSprite");

	_engine->RequestEventHook(AGSE_PREGUIDRAW);
	_engine->RequestEventHook(AGSE_PRESCREENDRAW);
	_engine->RequestEventHook(AGSE_ENTERROOM);
	_engine->RequestEventHook(AGSE_SAVEGAME);
	_engine->RequestEventHook(AGSE_RESTOREGAME);

	Initialize();
}

int AGSParallax::AGS_EngineOnEvent(int event, int data) {
	if (event == AGSE_PREGUIDRAW) {
		Draw(true);
	} else if (event == AGSE_PRESCREENDRAW) {
		Draw(false);
	} else if (event == AGSE_ENTERROOM) {
		// Reset all _sprites
		Initialize();
	} else if (event == AGSE_PRESCREENDRAW) {
		// Get screen size once here
		_engine->GetScreenDimensions(&_screenWidth, &_screenHeight, &_screenColorDepth);
		_engine->UnrequestEventHook(AGSE_PRESCREENDRAW);
	} else if (event == AGSE_RESTOREGAME) {
		RestoreGame(data);
	} else if (event == AGSE_SAVEGAME) {
		SaveGame(data);
	}

	return 0;
}

size_t AGSParallax::engineFileRead(void *ptr, size_t size, size_t count, long fileHandle) {
	auto totalBytes = _engine->FRead(ptr, size * count, fileHandle);
	return totalBytes / size;
}

size_t AGSParallax::engineFileWrite(const void *ptr, size_t size, size_t count, long fileHandle) {
	auto totalBytes = _engine->FWrite(const_cast<void *>(ptr), size * count, fileHandle);
	return totalBytes / size;
}

void AGSParallax::RestoreGame(long fileHandle) {
	unsigned int SaveVersion = 0;
	engineFileRead(&SaveVersion, sizeof(SaveVersion), 1, fileHandle);

	if (SaveVersion != SaveMagic) {
		_engine->AbortGame("ags_parallax: bad save.");
	}

	// TODO: This seems endian/packing unsafe
	engineFileRead(_sprites, sizeof(Sprite), MAX_SPRITES, fileHandle);
	engineFileRead(&_enabled, sizeof(bool), 1, fileHandle);
}

void AGSParallax::SaveGame(long file) {
	warning("TODO: AGSParallax::SaveGame is endian unsafe");
	engineFileWrite(&SaveMagic, sizeof(SaveMagic), 1, file);
	engineFileWrite(_sprites, sizeof(Sprite), MAX_SPRITES, file);
	engineFileWrite(&_enabled, sizeof(bool), 1, file);
}


void AGSParallax::Initialize() {
	memset(_sprites, 0, sizeof(Sprite) * MAX_SPRITES);

	int i;
	for (i = 0; i < MAX_SPRITES; i++)
		_sprites[i].slot = -1;

	_enabled = false;
}


void AGSParallax::Draw(bool foreground) {
	if (!_enabled)
		return;

	BITMAP *bmp;
	int i;

	int offsetX = 0;
	int offsetY = 0;
	_engine->ViewportToRoom(&offsetX, &offsetY);

	for (i = 0; i < MAX_SPRITES; i++) {
		if (_sprites[i].slot > -1) {
			if (foreground) {
				if (_sprites[i].speed > 0) {
					bmp = _engine->GetSpriteGraphic(_sprites[i].slot);
					if (bmp)
						_engine->BlitBitmap(_sprites[i].x - offsetX - (_sprites[i].speed * offsetX / 100), _sprites[i].y, bmp, 1);
				}
			} else {
				if (_sprites[i].speed <= 0) {
					bmp = _engine->GetSpriteGraphic(_sprites[i].slot);
					if (bmp)
						_engine->BlitBitmap(_sprites[i].x - offsetX - (_sprites[i].speed * offsetX / 1000), _sprites[i].y, bmp, 1);
				}
			}
		}
	}
}


void AGSParallax::pxDrawSprite(int id, int x, int y, int slot, int speed) {
#ifdef DEBUG
	char buffer[200];
	sprintf(buffer, "%s %d %d %d %d %d\n", "pxDrawSprite", id, x, y, slot, speed);
	_engine->PrintDebugConsole(buffer);
#endif

	if ((id < 0) || (id >= MAX_SPRITES))
		return;

	if ((speed < -MAX_SPEED) || (speed > MAX_SPEED))
		speed = 0;

	_sprites[id].x = x;
	_sprites[id].y = y;
	_sprites[id].slot = slot;
	_sprites[id].speed = speed;

	_engine->RoomToViewport(&_sprites[id].x, &_sprites[id].y);

	_enabled = true;
}


void AGSParallax::pxDeleteSprite(int id) {
#ifdef DEBUG
	char buffer[200];
	sprintf(buffer, "%s %d\n", "pxDeleteSprite", id);
	_engine->PrintDebugConsole(buffer);
#endif

	if ((id < 0) || (id >= MAX_SPRITES))
		return;

	_sprites[id].slot = -1;
}

} // namespace AGSParallax
} // namespace Plugins
} // namespace AGS3