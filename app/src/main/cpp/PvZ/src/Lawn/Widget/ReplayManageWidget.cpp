/*
 * Copyright (C) 2023-2026  PvZ TV Touch Team
 */

#include "PvZ/Lawn/Widget/ReplayManageWidget.h"
#include "Homura/HookUtils.h"
#include "Homura/Logger.h"
#include "PvZ/GlobalVariable.h"
#include "PvZ/Lawn/Board/Challenge.h"
#include "PvZ/Lawn/Board/SeedPacket.h"
#include "PvZ/Lawn/LawnApp.h"
#include "PvZ/Lawn/Widget/ChallengeScreen.h"
#include "PvZ/Lawn/Widget/GameButton.h"
#include "PvZ/Lawn/Widget/WaitForSecondPlayerDialog.h"
#include "PvZ/NetPlay.h"
#include "PvZ/ReplaySystem.h"
#include "PvZ/TodLib/Common/TodCommon.h"
#include "PvZ/TodLib/Common/TodStringFile.h"

#include <array>
#include <cassert>
#include <cstring>
#include <sstream>
#include <vector>

using namespace Sexy;

namespace {
constexpr int kReplayRowHeight = 140;

std::vector<SeedType> ParseDeck(const std::string &text) {
    std::vector<SeedType> out;
    std::stringstream ss(text);
    std::string token;
    while (std::getline(ss, token, ',')) {
        if (token.empty()) {
            continue;
        }
        int v = std::atoi(token.c_str());
        out.push_back(static_cast<SeedType>(v));
    }
    return out;
}
} // namespace


void *gReplayManageWidgetVTable[122];
void *gReplayListContentWidgetVTable[122];

class ReplayListContentWidget : public Widget {
public:
    ReplayManageWidget *mOwner = nullptr;
    std::vector<ReplayMetaInfo> mReplays;

public:
    int mTotalItems;

public:
    explicit ReplayListContentWidget(ReplayManageWidget *owner) {
        mOwner = owner;
        Init();
    }

    ~ReplayListContentWidget() {
        _destructor();
    }

    void Draw(Graphics *g) {
        int y = 0;
        for (const auto &item : mReplays) {
            const int rowTop = y;
            const int rowMidY = rowTop + kReplayRowHeight / 2;
            const int centerX = mWidth / 2;
            const int sideMargin = 200;
            const int leftNameX = sideMargin;
            const int leftDeckX = sideMargin;
            const int rightDeckX = mWidth - sideMargin - (6 * 50);
            const int rightDeckRightmostX = mWidth - sideMargin - 50;
            const int rightNameX = mWidth - sideMargin;
            const int secs = item.durationTicks / 100;
            const int mm = secs / 60;
            const int ss = secs % 60;
            const char *plantPlayer = "Unknown";
            const char *zombiePlayer = "Unknown";
            if (item.hostCamp == "Plant") {
                plantPlayer = item.hostName.c_str();
                zombiePlayer = item.guestName.c_str();
            } else if (item.hostCamp == "Zombie") {
                plantPlayer = item.guestName.c_str();
                zombiePlayer = item.hostName.c_str();
            }
            const bool versionMismatch = item.netplayVersion != NETPLAY_VERSION;
            bool plantWin = item.winnerName == plantPlayer;
            bool zombieWin = item.winnerName == zombiePlayer;

            TodDrawString(g, plantPlayer, leftNameX, rowTop + 26, FONT_DWARVENTODCRAFT18, Color(170, 255, 170), DrawStringJustification::DS_ALIGN_LEFT);
            TodDrawString(g, zombiePlayer, rightNameX, rowTop + 26, FONT_DWARVENTODCRAFT18, Color(255, 170, 170), DrawStringJustification::DS_ALIGN_RIGHT);

            const pvzstl::string timeText = StrFormat(TodStringTranslate("[REPLAY_TIME_FMT]").c_str(), item.createdAt.c_str(), mm, ss);

            pvzstl::string bgText;
            switch (item.vsBackground) {
                case 0:
                    bgText = TodStringTranslate("[MP_VS_DAY]");
                    break;
                case 1:
                    bgText = TodStringTranslate("[MP_VS_NIGHT]");
                    break;
                case 2:
                    bgText = TodStringTranslate("[MP_VS_POOL_DAY]");
                    break;
                case 3:
                    bgText = TodStringTranslate("[MP_VS_POOL_NIGHT]");
                    break;
                case 4:
                    bgText = TodStringTranslate("[MP_VS_ROOF]");
                    break;
                case -1:
                    bgText = TodStringTranslate("[MP_VS_SHUFFLE_MODE]");
                    break;
                default:
                    bgText = "Unknown";
                    break;
            };
            TodDrawString(g, timeText, centerX, rowTop + 35, FONT_DWARVENTODCRAFT18, Color(255, 244, 130), DrawStringJustification::DS_ALIGN_CENTER);
            TodDrawString(g, bgText, centerX, rowTop + 62, FONT_HOUSEOFTERROR16, Color(220, 220, 220), DrawStringJustification::DS_ALIGN_CENTER);
            if (plantWin) {
                g->DrawImage(IMAGE_MP_PLANT_TROPHY, centerX - 30, rowMidY - 10, 60, 60);
            } else if (zombieWin) {
                g->DrawImage(IMAGE_MP_ZOMBIE_TROPHY, centerX - 30, rowMidY - 10, 60, 60);
            }
            if (versionMismatch) {
                TodDrawString(g, "[REPLAY_ERROR_VERSION]", centerX, rowTop + 124, FONT_HOUSEOFTERROR16, Color(255, 120, 120), DrawStringJustification::DS_ALIGN_CENTER);
            }

            const auto plantDeck = ParseDeck(item.plantDeck);
            const auto zombieDeck = ParseDeck(item.zombieDeck);
            int seedX = leftDeckX;
            for (SeedType st : plantDeck) {
                if (st != SEED_NONE) {
                    DrawSeedPacket(g, (float)seedX, (float)(rowTop + 48), st, SeedType::SEED_NONE, 0.0f, 255, false, false, false, true);
                    seedX += 50;
                }
            }
            seedX = rightDeckRightmostX;
            for (auto it = zombieDeck.rbegin(); it != zombieDeck.rend(); ++it) {
                SeedType st = *it;
                if (st != SEED_NONE) {
                    DrawSeedPacket(g, (float)seedX, (float)(rowTop + 48), st, SeedType::SEED_NONE, 0.0f, 255, false, false, true, true);
                    seedX -= 50;
                }
            }
            y += kReplayRowHeight;
        }
    }

    void MouseDown(int x, int y, int theClickCount) {
        (void)x;
        (void)theClickCount;
        const int index = y / kReplayRowHeight;
        if (index < 0 || index >= static_cast<int>(mReplays.size()) || mOwner == nullptr) {
            LOG_WARN("[REPLAY] invalid click y={}, idx={}, count={}", y, index, (int)mReplays.size());
            return;
        }
        const ReplayMetaInfo &item = mReplays[index];
        if (item.netplayVersion != NETPLAY_VERSION) {
            mOwner->mApp->LawnMessageBox(Dialogs::DIALOG_MESSAGE, "[REPLAY]", "[REPLAY_ERROR_VERSION_TEXT]", "[DIALOG_BUTTON_OK]", "", 3);
            return;
        }
        LOG_INFO("[REPLAY] click list index={} file={}", index, mReplays[index].fileName);
        mOwner->StartReplayByIndex(index);
    }

private:
    void Init() {
        _constructor();
        static bool uninitialized = true;
        if (uninitialized) {
            size_t kVTableBytes = sizeof(void *) * std::size(gReplayListContentWidgetVTable);
            std::memcpy(gReplayListContentWidgetVTable, vTable, sizeof(void *) * std::size(gReplayListContentWidgetVTable));
            homura::HookVirtualFunc(gReplayListContentWidgetVTable, 36, &ReplayListContentWidget::Draw, nullptr);
            homura::HookVirtualFunc(gReplayListContentWidgetVTable, 78, &ReplayListContentWidget::MouseDown, nullptr);
            uninitialized = false;
        }

        vTable = reinterpret_cast<int *>(gReplayListContentWidgetVTable);
        mReplays = replay::ListReplayFiles();
        mTotalItems = static_cast<int>(mReplays.size());
        LOG_INFO("[REPLAY] list loaded count={}", mTotalItems);
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
    mButtonListener = buttonListener;
    Resize(LawnApp::FULLSCREEN_RECT.mX, LawnApp::FULLSCREEN_RECT.mY, LawnApp::FULLSCREEN_RECT.mWidth, LawnApp::FULLSCREEN_RECT.mHeight);
    mClip = true;

    mScrollWidget = new ScrollWidget();
    mScrollWidget->Resize(0, 150, mWidth, mHeight - 150);
    mScrollWidget->SetScrollMode(ScrollWidget::ScrollMode::SCROLL_VERTICAL);
    mScrollWidget->EnableBounce(false);
    AddWidget(mScrollWidget);
    mScrollContent = new ReplayListContentWidget(this);
    mScrollContent->Resize(0, 0, mScrollWidget->mWidth, mScrollContent->mTotalItems * kReplayRowHeight);

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
    TodDrawString(g, "[REPLAY_MANAGE]", 640, 110, FONT_DWARVENTODCRAFT24, Color(255, 248, 195), DrawStringJustification::DS_ALIGN_CENTER);
}

void ReplayManageWidget::StartReplayByIndex(int index) {
    auto list = replay::ListReplayFiles();
    if (index < 0 || index >= static_cast<int>(list.size())) {
        LOG_ERROR("[REPLAY] start failed invalid index={} size={}", index, (int)list.size());
        return;
    }
    const ReplayMetaInfo &item = list[index];
    if (!replay::BeginPlaybackFromFile(item.filePath)) {
        LOG_ERROR("[REPLAY] begin playback failed file={}", item.filePath);
        return;
    }
    gIsServerModeNetplay = true;
    gServerModeTransport = ServerModeTransport::RELAY;
    gIsServerModeSpectator = false;
    gIsReplayMode = true;
    LOG_INFO("[REPLAY] start playback success file={}", item.filePath);

    if (auto *dialog = static_cast<WaitForSecondPlayerDialog *>(mApp->GetDialog(DIALOG_WAIT_FOR_SECOND_PLAYER))) {
        LOG_INFO("[REPLAY] closing replay manager via WaitForSecondPlayerDialog");
        dialog->CloseReplayManageWidget();
        dialog->LawnDialog::ButtonDepress(1000);
    }

    switch (item.vsBackground) {
        case 0:
            gVSBackground = BackgroundType::BACKGROUND_1_DAY;
            break;
        case 1:
            gVSBackground = BackgroundType::BACKGROUND_2_NIGHT;
            break;
        case 2:
            gVSBackground = BackgroundType::BACKGROUND_3_POOL;
            break;
        case 3:
            gVSBackground = BackgroundType::BACKGROUND_4_FOG;
            break;
        case 4:
            gVSBackground = BackgroundType::BACKGROUND_5_ROOF;
            break;
        case -1:
            gVSBackground = BackgroundType::BACKGROUND_1_DAY;
            Challenge::msVSShuffleMode = true;
            break;
        default:
            break;
    }

    mApp->KillChallengeScreen();
    mApp->PreNewGame(GAMEMODE_MP_VS, false);
}
