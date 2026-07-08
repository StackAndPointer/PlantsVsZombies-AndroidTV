/*
 * Copyright (C) 2023-2026  PvZ TV Touch Team
 *
 * This file is part of PlantsVsZombies-AndroidTV.
 *
 * PlantsVsZombies-AndroidTV is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * PlantsVsZombies-AndroidTV is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
 * Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * PlantsVsZombies-AndroidTV.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef PVZ_LAWN_SYSTEM_SAVE_GAME_H
#define PVZ_LAWN_SYSTEM_SAVE_GAME_H

#include "PvZ/STL/string.h"
#include "PvZ/SexyAppFramework/Buffer.h"
#include "PvZ/Symbols.h"

class Board;

#define SAVE_FILE_MAGIC_NUMBER 0xFEEDDEAD
#define SAVE_FILE_VERSION 8
#define SAVE_FILE_DATE 0

struct SaveFileHeader {
    unsigned int mMagicNumber;
    unsigned int mBuildVersion;
    unsigned int mBuildDate;
};

class SaveGameContext {
public:
    Sexy::Buffer mBuffer;
    bool mFailed;
    bool mReading;

    ~SaveGameContext() = default;

    void SyncInt(int &theInt) {
        reinterpret_cast<void (*)(SaveGameContext *, int &)>(SaveGameContext_SyncIntAddr)(this, theInt);
    }
    void SyncBytes(void *theDest, int theReadSize) {
        reinterpret_cast<void (*)(SaveGameContext *, void *, int)>(SaveGameContext_SyncBytesAddr)(this, theDest, theReadSize);
    }
    void SyncUint(unsigned int &theUint) {
        reinterpret_cast<void (*)(SaveGameContext *, unsigned int &)>(SaveGameContext_SyncUintAddr)(this, theUint);
    }

    void SyncReanimationDef(ReanimatorDefinition *&theDefinition);
};
inline void SyncBoard(SaveGameContext *theContext, Board *theBoard) {
    reinterpret_cast<void (*)(SaveGameContext *, Board *)>(SyncBoardAddr)(theContext, theBoard);
}


bool LawnSaveGame_Original(Board *theBoard, const pvzstl::string &theFilePath);
bool LawnLoadGame_Original(Board *theBoard, SaveGameContext *theContext);
void FixBoardAfterLoad(Board *board);
bool LawnSaveGame(Board *theBoard, const pvzstl::string &theFilePath);
bool LawnLoadGame(Board *theBoard, SaveGameContext *theContext);
inline void GetSavedGameName(const pvzstl::string &name, GameMode theGameMode, int theProfileId, int theId) {
    reinterpret_cast<void (*)(const pvzstl::string &, GameMode, int, int)>(GetSavedGameNameAddr)(name, theGameMode, theProfileId, theId);
}

inline void (*old_FixBoardAfterLoad)(Board *board);

#endif // PVZ_LAWN_SYSTEM_SAVE_GAME_H
