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

#include "PvZ/Lawn/Board/ShovelRedirectWidget.h"
#include "Homura/MemberUtils.h"
#include "PvZ/Lawn/Board/Board.h"
#include "PvZ/Symbols.h"

#include <cstring>

#include <mutex>

ShovelRedirectWidget::ShovelRedirectWidget(Board *board) {
    Widget::_constructor();

    static void *sShovelRedirectWidgetVTable[122];
    static std::once_flag vtableInitFlag;
    std::call_once(vtableInitFlag, [this] {
        std::memcpy(sShovelRedirectWidgetVTable, vTable, sizeof(sShovelRedirectWidgetVTable));
        sShovelRedirectWidgetVTable[0] = (void *)homura::ExtractMemFuncPtr(&ShovelRedirectWidget::_destructor);
        sShovelRedirectWidgetVTable[1] = (void *)homura::ExtractMemFuncPtr(&ShovelRedirectWidget::_destructor2);
        sShovelRedirectWidgetVTable[36] = (void *)homura::ExtractMemFuncPtr(&ShovelRedirectWidget::Draw);
        sShovelRedirectWidgetVTable[76] = (void *)homura::ExtractMemFuncPtr(&ShovelRedirectWidget::MouseDown);
        sShovelRedirectWidgetVTable[79] = (void *)homura::ExtractMemFuncPtr(&ShovelRedirectWidget::MouseUp);
        sShovelRedirectWidgetVTable[81] = (void *)homura::ExtractMemFuncPtr(&ShovelRedirectWidget::MouseDrag);
    });
    vTable = sShovelRedirectWidgetVTable;

    mBoard = board;
    mClip = false;
}

ShovelRedirectWidget::~ShovelRedirectWidget() {
    Widget::_destructor();
}

void ShovelRedirectWidget::_destructor() {
    Widget::_destructor();
}

void ShovelRedirectWidget::_destructor2() {
    delete this;
}

void ShovelRedirectWidget::Draw(Sexy::Graphics *g) {}

void ShovelRedirectWidget::MouseDown(int x, int y, int theClickCount) {
    if (mBoard == nullptr) {
        return;
    }
    mBoard->MouseDown(x + mX - mBoard->mX, y + mY - mBoard->mY, theClickCount);
}

void ShovelRedirectWidget::MouseUp(int x, int y, int theClickCount) {
    if (mBoard == nullptr) {
        return;
    }
    mBoard->MouseUp(x + mX - mBoard->mX, y + mY - mBoard->mY, theClickCount);
}

void ShovelRedirectWidget::MouseDrag(int x, int y) {
    if (mBoard == nullptr) {
        return;
    }
    mBoard->MouseDrag(x + mX - mBoard->mX, y + mY - mBoard->mY);
}
