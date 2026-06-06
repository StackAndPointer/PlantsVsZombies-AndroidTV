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

#include "PvZ/Lawn/Widget/CreditScreen.h"
#include "PvZ/Lawn/LawnApp.h"
#include "PvZ/TodLib/Common/TodStringFile.h"

using namespace Sexy;

void CreditScreen::_constructor(LawnApp *theApp, bool theBool) {
    old_CreditScreen__constructor(this, theApp, theBool);

    gCreditScreenBackButton = MakeButton(1000, this, this, "[MAIN_MENU_BUTTON]");
    gCreditScreenBackButton->Resize(725, 0, 170, 50);
}

void CreditScreen::_destructor() {
    gLawnApp->SafeDeleteWidget(gCreditScreenBackButton);
    gCreditScreenBackButton = nullptr;

    old_CreditScreen__destructor(this);
}

void CreditScreen::AddedToManager(WidgetManager *theWidgetManager) {
    old_CreditScreen_AddedToManager(this, theWidgetManager);

    AddWidget(gCreditScreenBackButton);
}

void CreditScreen::RemovedFromManager(WidgetManager *theWidgetManager) {
    mFocusedChildWidget = gCreditScreenBackButton; // 修复触摸CreditScreen后点击按钮退出就会闪退的BUG,虽然不知道为什么
    RemoveWidget(gCreditScreenBackButton);

    old_CreditScreen_RemovedFromManager(this, theWidgetManager);
}

void CreditScreen::ButtonDepress(int theId) {
    if (theId == 1000) {
        LawnApp *lawnApp = gLawnApp;
        lawnApp->mCreditScreen->PauseCredits();
    }
}
