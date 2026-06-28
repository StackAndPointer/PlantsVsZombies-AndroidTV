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

#include "PvZ/TodLib/Common/TodFoley.h"
#include "PvZ/GlobalVariable.h"

#include <algorithm>
#include <mutex>

void TodFoleyInitialize(FoleyParams *theFoleyParamArray, int theFoleyParamArraySize) {
    gFoleyParamArray = theFoleyParamArray;
    gFoleyParamArraySize = theFoleyParamArraySize;
}

FoleyParams *LookupFoley(FoleyType theFoleyType) {
    return &gFoleyParamArray[theFoleyType];
}

auto GetNewLawnFoleyParamArray() -> FoleyParams (&)[FoleyType::EXTENDED_NUM_FOLEY] {
    static FoleyParams newArray[FoleyType::EXTENDED_NUM_FOLEY] = {};

    static std::once_flag flag;
    std::call_once(flag, [] {
        std::ranges::copy(gLawnFoleyParamArray, newArray);

        const FoleyParams extendedArray[FoleyType::EXTENDED_NUM_FOLEY - FoleyType::NUM_FOLEY] = {
            {FoleyType::FOLEY_MENU_LEFT, 0.0f, {&Sexy::SOUND_MENU_L_ST}, 1U},
            {FoleyType::FOLEY_MENU_CENTRE, 0.0f, {&Sexy::SOUND_MENU_C_ST}, 1U},
            {FoleyType::FOLEY_MENU_RIGHT, 0.0f, {&Sexy::SOUND_MENU_R_ST}, 1U},
            {FoleyType::FOLEY_ALLSTAR_TACKLE, 10.0f, {&addonSounds.allstardbl}, 0U},
            {FoleyType::FOLEY_THRILLER, 0.0f, {&addonSounds.thriller}, 6U},
            {FOLEY_EXPLOER_IGNITE, 0.0f, {&addonSounds.exploer}, 0U},
            {FoleyType::FOLEY_ICEBERG, 0.0f, {&addonSounds.iceberg}, 0U},
        };
        std::ranges::copy(extendedArray, newArray + FoleyType::NUM_FOLEY);
    });

    return newArray;
}
