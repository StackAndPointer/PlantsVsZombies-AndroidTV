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

#include "PvZ/Lawn/Widget/TitleScreen.h"
#include "Homura/Logger.h"
#include "PvZ/GlobalVariable.h"
#include "PvZ/Lawn/Common/ConstEnums.h"
#include "PvZ/SexyAppFramework/Graphics/Graphics.h"
#include "PvZ/Symbols.h"

void TitleScreen::Draw(Sexy::Graphics *graphics) {
    old_TitleScreen_Draw(this, graphics);
    // LOGD("%d",Sexy::IMAGE_ESRB_RATING);
    // if (Sexy::IMAGE_ESRB_RATING)
    // DrawImage(graphics,Sexy::IMAGE_ESRB_RATING,0,0);
    // LOGD("draw");
    // int *q = nullptr;
    // q[1] = 1;
}

void TitleScreen::Update() {
    old_TitleScreen_Update(this);
    // LOGD("Update");
    // int *q = nullptr;
    // q[1] = 1;
}

void TitleScreen::SwitchState(TitleState state, int duration) {
    mTitleState = state;
    mTitleStateDuration = duration;
    mTitleStateCounter = duration;
}

void TitleScreen::_constructor(LawnApp *theApp) {
    old_TitleScreen_TitleScreen(this, theApp);
    if (jumpLogo) {
        mNeedPlayLogo = false;
    }
    //    bool option = Sexy::GetEnvOption("LAWN_NO_INTRO_VIDEO", false);
    //    LOG_DEBUG("{}",option);
}