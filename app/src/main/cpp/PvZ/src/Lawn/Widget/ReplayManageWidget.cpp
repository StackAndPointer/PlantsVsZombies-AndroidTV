/*
 * Copyright (C) 2023-2026  PvZ TV Touch Team
 */

#include "PvZ/Lawn/Widget/ReplayManageWidget.h"
#include "Homura/HookUtils.h"
#include "Homura/Logger.h"
#include "PvZ/Lawn/LawnApp.h"
#include "PvZ/Lawn/Widget/GameButton.h"
#include "PvZ/TodLib/Common/TodCommon.h"
#include "PvZ/TodLib/Common/TodStringFile.h"

#include <array>
#include <cassert>
#include <cstring>

using namespace Sexy;


void *gReplayManageWidgetVTable[122];
void *gReplayListContentWidgetVTable[122];

class ReplayListContentWidget : public Widget {
public:
    struct ReplayItem {
        const char *title;
        const char *meta;
    };
    static constexpr ReplayItem kSamples[] = {
        {"[Demo] Roof Endless Wave 40", "2026-05-20 21:38  |  16m 42s  |  Hard"},
        {"[Demo] Pool VS Counterpush", "2026-05-21 19:03  |  08m 15s  |  P2P"},
        {"[Demo] Speedrun Day Any%", "2026-05-24 13:27  |  05m 49s  |  No Pause"},
        {"[Demo] Last Stand Economy", "2026-05-25 09:12  |  11m 06s  |  Replay"},
        {"[Demo] Survival Fog Drill", "2026-05-25 23:41  |  09m 31s  |  Relay"},
        {"[Demo] Roof Endless Wave 40", "2026-05-20 21:38  |  16m 42s  |  Hard"},
        {"[Demo] Pool VS Counterpush", "2026-05-21 19:03  |  08m 15s  |  P2P"},
        {"[Demo] Speedrun Day Any%", "2026-05-24 13:27  |  05m 49s  |  No Pause"},
        {"[Demo] Last Stand Economy", "2026-05-25 09:12  |  11m 06s  |  Replay"},
        {"[Demo] Survival Fog Drill", "2026-05-25 23:41  |  09m 31s  |  Relay"},
        {"[Demo] Roof Endless Wave 40", "2026-05-20 21:38  |  16m 42s  |  Hard"},
        {"[Demo] Pool VS Counterpush", "2026-05-21 19:03  |  08m 15s  |  P2P"},
        {"[Demo] Speedrun Day Any%", "2026-05-24 13:27  |  05m 49s  |  No Pause"},
        {"[Demo] Last Stand Economy", "2026-05-25 09:12  |  11m 06s  |  Replay"},
        {"[Demo] Survival Fog Drill", "2026-05-25 23:41  |  09m 31s  |  Relay"},
    };

public:
    int mTotalItems;

public:
    ReplayListContentWidget() {
        Init();
        //        Resize(0, 0, 800, 600);
    }

    ~ReplayListContentWidget() {
        _destructor();
    }

    void Draw(Graphics *g) {


        //        g->SetColor(Color(25, 25, 25, 220));
        //        g->FillRect(Rect(0, 0, mWidth, mHeight));

        int y = 0;
        for (const auto &item : kSamples) {
            //            g->SetColor(Color(45, 45, 45, 255));
            //            g->FillRect(Rect(16, y, mWidth - 32, 78));
            //            g->SetColor(Color(80, 80, 80, 255));
            //            g->DrawRect(Rect(16, y, mWidth - 32, 78));

            TodDrawString(g, item.title, 32, y + 30, FONT_DWARVENTODCRAFT18, Color(255, 244, 130), DrawStringJustification::DS_ALIGN_LEFT);
            TodDrawString(g, item.meta, 32, y + 56, FONT_HOUSEOFTERROR16, Color(220, 220, 220), DrawStringJustification::DS_ALIGN_LEFT);
            y += 90;
        }
    }

private:
    void Init() {
        _constructor();
        static bool uninitialized = true;
        if (uninitialized) {
            size_t kVTableBytes = sizeof(void *) * std::size(gReplayListContentWidgetVTable);
            std::memcpy(gReplayListContentWidgetVTable, vTable, sizeof(void *) * std::size(gReplayListContentWidgetVTable));
            homura::HookVirtualFunc(gReplayListContentWidgetVTable, 36, &ReplayListContentWidget::Draw, nullptr);
            uninitialized = false;
        }

        vTable = reinterpret_cast<int *>(gReplayListContentWidgetVTable);
        mTotalItems = std::size(kSamples);
    }
};


ReplayManageWidget::ReplayManageWidget(LawnApp *app, ButtonListener *buttonListener) {
    static bool uninitialized = true;
    _constructor();
    if (uninitialized) {
        constexpr size_t kVTableBytes = sizeof(void *) * std::size(gReplayManageWidgetVTable);
        std::memcpy(gReplayManageWidgetVTable, vTable, sizeof(void *) * std::size(gReplayManageWidgetVTable));
        homura::HookVirtualFunc(gReplayManageWidgetVTable, 36, &ReplayManageWidget::Draw, nullptr);
        uninitialized = false;
    }
    vTable = reinterpret_cast<int *>(gReplayManageWidgetVTable);

    mApp = app;
    Resize(LawnApp::FULLSCREEN_RECT.mX, LawnApp::FULLSCREEN_RECT.mY, LawnApp::FULLSCREEN_RECT.mWidth, LawnApp::FULLSCREEN_RECT.mHeight);
    mClip = true;

    mScrollWidget = new ScrollWidget();
    mScrollWidget->Resize(0, 150, mWidth, mHeight - 150);
    mScrollWidget->SetScrollMode(ScrollWidget::ScrollMode::Vertical);
    mScrollWidget->EnableBounce(false);
    AddWidget(mScrollWidget);
    mScrollContent = new ReplayListContentWidget();
    mScrollContent->Resize(0, 0, mScrollWidget->mWidth, mScrollContent->mTotalItems * 90);

    mScrollWidget->AddWidget(mScrollContent);
    mScrollWidget->ScrollToMin(false);
    mCloseButton = MakeButton(1100, buttonListener, this, "[CLOSE]");
    mCloseButton->Resize(1000, 564, 170, 50);
    mZombieBackground = Rand(2);
    AddWidget(mCloseButton);
    TodLoadResources("DelayLoad_Almanac");
}

ReplayManageWidget::~ReplayManageWidget() {
    _destructor();
}

void ReplayManageWidget::Draw(Graphics *g) {
    g->DrawImage(mZombieBackground ? IMAGE_ALMANAC_ZOMBIEBACK : IMAGE_ALMANAC_PLANTBACK, 0, 0);
    TodDrawString(g, "[Replay Manager]", 640, 110, FONT_DWARVENTODCRAFT24, Color(255, 248, 195), DrawStringJustification::DS_ALIGN_CENTER);
}
