/*
 * Copyright (C) 2023-2026  PvZ TV Touch Team
 *
 * This file is part of PlantsVsZombies-AndroidTV.
 */

#ifndef PVZ_LAWN_WIDGET_REPLAY_CONTROLS_WIDGET_H
#define PVZ_LAWN_WIDGET_REPLAY_CONTROLS_WIDGET_H

#include "PvZ/SexyAppFramework/Widget/Widget.h"

class Board;

class ReplayControlsWidget : public Sexy::Widget {
public:
    static constexpr int kX = 0;
    static constexpr int kInitialY = -40;
    static constexpr int kWidth = 800;
    static constexpr int kHeight = 40;

    Board *mBoard = nullptr;
    int mStartLevelTick = -2;

    explicit ReplayControlsWidget(Board *board);
    ~ReplayControlsWidget();

    void Draw(Sexy::Graphics *g);
    void MouseDown(int x, int y, int theClickCount);
    void MouseDrag(int x, int y);
    void MouseUp(int x, int y, int theClickCount);

protected:
    static bool Contains(const Sexy::Rect &rect, int x, int y);
    static void DrawButton(Sexy::Graphics *g, const Sexy::Rect &rect, const char *label);

    Board *GetCurrentBoard() const;
    int GetStartLevelTick();
    bool ShowSkipSetupButton() const;
    Sexy::Rect PauseButtonRect() const;
    Sexy::Rect SpeedButtonRect() const;
    Sexy::Rect ForwardButtonRect() const;
    Sexy::Rect RestartButtonRect() const;
    Sexy::Rect SkipSetupButtonRect() const;

    void _destructor();
    void _destructor2();
};

#endif // PVZ_LAWN_WIDGET_REPLAY_CONTROLS_WIDGET_H
