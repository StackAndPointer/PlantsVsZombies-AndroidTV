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

#ifndef PVZ_LAWN_WIDGET_REPLAY_MANAGE_WIDGET_H
#define PVZ_LAWN_WIDGET_REPLAY_MANAGE_WIDGET_H

#include "PvZ/SexyAppFramework/Widget/Widget.h"

class GameButton;
class LawnApp;
class ReplayListContentWidget;

namespace Sexy {
class ButtonListener;
class ScrollWidget;
} // namespace Sexy

class ReplayManageWidget : public Sexy::Widget {
public:
    LawnApp *mApp;
    Sexy::ButtonListener *mButtonListener;
    Sexy::ScrollWidget *mScrollWidget;
    ReplayListContentWidget *mScrollContent;
    GameButton *mCloseButton;
    bool mZombieBackground;

    explicit ReplayManageWidget(LawnApp *app, Sexy::ButtonListener *buttonListener);
    ~ReplayManageWidget();

    void Draw(Sexy::Graphics *g);
    void StartReplayByIndex(int index);

protected:
    void _destructor();
    void _destructor2();
};

#endif // PVZ_LAWN_WIDGET_REPLAY_MANAGE_WIDGET_H
