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

#ifndef PVZ_LAWN_WIDGET_VS_RESULTS_MENU_H
#define PVZ_LAWN_WIDGET_VS_RESULTS_MENU_H

#include "PvZ/Lawn/Board/Board.h"
#include "PvZ/Lawn/Board/SeedBank.h"
#include "PvZ/Lawn/Common/ConstEnums.h"
#include "PvZ/Lawn/Common/LawnCommon.h"
#include "PvZ/Lawn/LawnApp.h"
#include "PvZ/Lawn/Widget/GameButton.h"
#include "PvZ/NetPlay.h"
#include "PvZ/SexyAppFramework/Widget/Checkbox.h"
#include "PvZ/SexyAppFramework/Widget/CheckboxListener.h"
#include "PvZ/SexyAppFramework/Widget/MenuWidget.h"
#include "PvZ/SexyAppFramework/Widget/WidgetManager.h"
#include "PvZ/TodLib/Common/TodStringFile.h"

#include <cstddef>

inline constexpr int BUTTON_LABEL_WRAP_CENTER = 2;
inline constexpr int BUTTON_Y = 472;
inline constexpr int BUTTON_WIDTH = 205;
inline constexpr int BUTTON_HEIGHT = 96;

class VSResultsMenu : public Sexy::MenuWidget {
public:
    enum {
        VSResultsMenu_Play_Again = 0,
        VSResultsMenu_Quit_VS = 1,
        VSResultsMenu_Info_Box_P1 = 2,
        VSResultsMenu_Info_Box_P2 = 3,
        VSResultsMenu_Plant_Side = 4,
        VSResultsMenu_Plant_Side_Front = 5,
        VSResultsMenu_Zombie_Side = 6,
        VSResultsMenu_Zombie_Side_Front = 7,
        VSResultsMenu_Win_Image = 8,
        VSResultsMenu_Back = 9,
        VSResultsMenu_Save_Replay = 1200,
    };

    enum WinSide {
        INVALID_WIN_SIDE = -1,
        WIN_SIDE_PLANT = 0,
        WIN_SIDE_ZOMBIE = 1,
    };

    // [记录槽] -> [胜负方, 植物胜场, 僵尸胜场, 连胜数, playerIndex2]
    static int (&msPlayerRecords)[2][5];

    int unk[3];                          // 70 ~ 72
    int mSparkleCounter;                 // 73
    int mSmokeCounter;                   // 74
    int mResultsButtonId;                // 75
    int mPlayerIndices[2];               // 76 ~ 77, P1 / P2 的 playerIndex2
    WinSide mSides[2];                   // 78 ~ 79
    float mTrophyPosX;                   // 80, 奖杯中心 X
    float mTrophyPosY;                   // 81, 奖杯中心 Y
    ParticleSystemID mSparkleParticleID; // 82
    ParticleSystemID mSmokeParticleID;   // 83
    int mUpdateCounter;                  // 84
    int mBoardMainCounter = 0;
    BackgroundType mBoardBackground = BackgroundType::BACKGROUND_1_DAY;
    SeedType mPlantSeeds[6] = {SeedType::SEED_NONE, SeedType::SEED_NONE, SeedType::SEED_NONE, SeedType::SEED_NONE, SeedType::SEED_NONE, SeedType::SEED_NONE};
    SeedType mZombieSeeds[6] = {SeedType::SEED_NONE, SeedType::SEED_NONE, SeedType::SEED_NONE, SeedType::SEED_NONE, SeedType::SEED_NONE, SeedType::SEED_NONE};
    bool mIsReplaySession = false;
    bool mIsOnlineSession = false;
    int mVSResultsCounter = 0;
    class VSResultsCheckboxController *mCheckboxController = nullptr;
    GameButton *mBackButton = nullptr;
    GameButton *mSaveReplayButton = nullptr;

    int *GetPlayerRecord(unsigned int playerIndex) {
        return reinterpret_cast<int *(*)(VSResultsMenu *, unsigned int)>(VSResultsMenu_GetPlayerRecordAddr)(this, playerIndex);
    }

    VSResultsMenu() {
        _constructor();
    }
    ~VSResultsMenu() = delete;

    void AddedToManager(Sexy::WidgetManager *theWidgetManager);
    void RemovedFromManager(Sexy::WidgetManager *theWidgetManager);
    void ClearPlayerRecords();
    void Update();
    void OnExit();
    void ButtonDepress(int theId);
    void Draw(Sexy::Graphics *g);
    void DrawInfoBox(Sexy::Graphics *a2, int a3);
    void HideReplayButton(bool forceHide);
    void HandleOpponentDisconnected();
    void InitFromBoard(Board *theBoard);
    void ShowReplayButton();
    void KillReplayButton();

    void processClientEvent(const BaseEvent *event);
    void processServerEvent(const BaseEvent *event);

protected:
    friend void InitHookFunction();

    void _constructor();
    void _destructor();
};

class VSResultsCheckboxController final : public Sexy::CheckboxListener {
public:
    enum {
        VSResultsMenu_Send_Player_Name = 100,
    };

    Sexy::Checkbox *mSendPlayerNameCheckbox;

    VSResultsCheckboxController()
        : Sexy::CheckboxListener()
        , mSendPlayerNameCheckbox(nullptr)
        , mParentMenu(nullptr) {}

    void CheckboxChecked(int theId, bool checked) override {
        if (theId == VSResultsMenu_Send_Player_Name) {
            gLawnApp->mPlayerInfo->mVSResultsSendPlayerName = checked;
            gLawnApp->mPlayerInfo->SaveDetails();
        }
    }

    void InitCheckboxWidget(VSResultsMenu *parentMenu) {
        mParentMenu = parentMenu;
        if (mParentMenu == nullptr || mParentMenu->mWidgetManager == nullptr) {
            return;
        }
        if (mSendPlayerNameCheckbox != nullptr) {
            return;
        }
        mSendPlayerNameCheckbox = MakeNewCheckbox(VSResultsMenu_Send_Player_Name, this, mParentMenu, false);
        mSendPlayerNameCheckbox->Resize(-60, 580, 175, 50);
        mSendPlayerNameCheckbox->SetChecked(gLawnApp->mPlayerInfo->mVSResultsSendPlayerName, false);
        mParentMenu->AddWidget(mSendPlayerNameCheckbox);
    }

    void DrawCheckboxLabel(Sexy::Graphics *g) const {
        if (mSendPlayerNameCheckbox == nullptr || g == nullptr || !mSendPlayerNameCheckbox->mVisible) {
            return;
        }
        const Sexy::Color color = (mParentMenu != nullptr && mParentMenu->mFocusedChildWidget == mSendPlayerNameCheckbox) ? Sexy::Color(0, 255, 0) : Sexy::Color(107, 110, 145);
        g->SetFont(Sexy::FONT_DWARVENTODCRAFT18);
        g->SetColor(color);
        g->DrawString(TodStringTranslate("[SEND_PLAYER_NAME]"), mSendPlayerNameCheckbox->mX + 40, mSendPlayerNameCheckbox->mY + 26);
    }

    void HideCheckboxWidget() {
        if (mSendPlayerNameCheckbox == nullptr) {
            return;
        }
        mSendPlayerNameCheckbox->SetVisible(false);
        mSendPlayerNameCheckbox->mDisabled = true;
    }

    void SetCheckboxVisible(bool visible) {
        if (mSendPlayerNameCheckbox == nullptr) {
            return;
        }
        mSendPlayerNameCheckbox->SetVisible(visible);
        mSendPlayerNameCheckbox->mDisabled = !visible;
    }

    void DestroyCheckboxWidget() {
        if (mSendPlayerNameCheckbox == nullptr) {
            return;
        }
        if (mParentMenu != nullptr && mParentMenu->mWidgetManager != nullptr) {
            mParentMenu->RemoveWidget(mSendPlayerNameCheckbox);
        }
        gLawnApp->SafeDeleteWidget(mSendPlayerNameCheckbox);
        mSendPlayerNameCheckbox = nullptr;
        mParentMenu = nullptr;
    }

private:
    VSResultsMenu *mParentMenu;
};

inline int gVSResultRequestState = -1;

inline void (*old_VSResultsMenu_Update)(VSResultsMenu *a);

inline void (*old_VSResultsMenu_Draw)(VSResultsMenu *, Sexy::Graphics *);

inline void (*old_VSResultsMenu_DrawInfoBox)(VSResultsMenu *a, Sexy::Graphics *a2, int a3);

inline void (*old_VSResultsMenu_Constructor)(VSResultsMenu *);

inline void (*old_VSResultsMenu_AddedToManager)(VSResultsMenu *, Sexy::WidgetManager *);

inline void (*old_VSResultsMenu_RemovedFromManager)(VSResultsMenu *, Sexy::WidgetManager *);

inline void (*old_VSResultsMenu_Destructor)(VSResultsMenu *);

inline void (*old_VSResultsMenu_ClearPlayerRecords)(VSResultsMenu *);

#endif // PVZ_LAWN_WIDGET_VS_RESULTS_MENU_H
