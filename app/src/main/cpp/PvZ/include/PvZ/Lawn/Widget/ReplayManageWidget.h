/*
 * Copyright (C) 2023-2026  PvZ TV Touch Team
 */

#ifndef PVZ_LAWN_WIDGET_REPLAY_MANAGE_WIDGET_H
#define PVZ_LAWN_WIDGET_REPLAY_MANAGE_WIDGET_H

#include "PvZ/SexyAppFramework/Widget/ButtonListener.h"
#include "PvZ/SexyAppFramework/Widget/ScrollWidget.h"
#include "PvZ/SexyAppFramework/Widget/Widget.h"

class GameButton;
class LawnApp;
class ReplayListContentWidget;

class ReplayManageWidget : public Sexy::Widget {
public:
    explicit ReplayManageWidget(LawnApp *app, Sexy::ButtonListener *buttonListener);
    ~ReplayManageWidget();

    void Draw(Sexy::Graphics *g);
    void StartReplayByIndex(int index);

public:
    LawnApp *mApp;
    Sexy::ButtonListener *mButtonListener;
    Sexy::ScrollWidget *mScrollWidget;
    ReplayListContentWidget *mScrollContent;
    GameButton *mCloseButton;
    bool mZombieBackground;
};

#endif // PVZ_LAWN_WIDGET_REPLAY_MANAGE_WIDGET_H
