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

#include "PvZ/Lawn/Widget/VSResultsMenu.h"
#include "Homura/Logger.h"
#include "PvZ/GlobalVariable.h"
#include "PvZ/Lawn/Board/Challenge.h"
#include "PvZ/Lawn/LawnApp.h"
#include "PvZ/Lawn/Widget/GameButton.h"
#include "PvZ/NetPlay.h"
#include "PvZ/ReplaySystem.h"
#include "PvZ/SexyAppFramework/Graphics/Graphics.h"
#include "PvZ/TodLib/Common/TodCommon.h"
#include "PvZ/TodLib/Common/TodStringFile.h"

#include <chrono>
#include <ctime>
#include <unistd.h>

using namespace Sexy;

class ImageWidgetLike : public Sexy::Widget {
public:
    Sexy::Image *mImage;
};

static std::string BuildDeckText(const SeedType *seeds, int count) {
    std::string s;
    for (int i = 0; i < count; ++i) {
        if (i > 0) {
            s.push_back(',');
        }
        s += std::to_string(static_cast<int>(seeds[i]));
    }
    return s;
}

static const char *CampNameFromSide(int side) {
    if (side == 0) {
        return "Plant";
    }
    if (side == 1) {
        return "Zombie";
    }
    return "Unknown";
}


void VSResultsMenu::_constructor() {
    old_VSResultsMenu_Constructor(this);
    mIsReplaySession = gIsReplayMode;
    gVSResultRequestState = -1;
    gNetDelayNow = 0; // 清除旧的延时数据

    mBackButton = MakeButton(VSResultsMenu::VSResultsMenu_Back, this, this, "[BACK_TO_MODE_SELECT]");
    mBackButton->mDrawStoneButton = false;
    mBackButton->mButtonImage = addonImages.VS_Button;
    mBackButton->mOverImage = addonImages.VS_Button_selected;
    mBackButton->mDownImage = addonImages.VS_Button_selected;
    mBackButton->SetFont(FONT_DWARVENTODCRAFT24);
    (*mBackButton->mColors)[ButtonWidget::COLOR_LABEL] = Color(25, 197, 45);
    (*mBackButton->mColors)[ButtonWidget::COLOR_LABEL_HILITE] = Color(277, 225, 108);
    mBackButton->mLabelJustify = BUTTON_LABEL_WRAP_CENTER;
    mBackButton->Resize(660, BUTTON_Y, BUTTON_WIDTH, BUTTON_HEIGHT);
}

void VSResultsMenu::_destructor() {
    //    if (mBackButton) {
    //        RemoveWidget(mBackButton);
    //    }
    //    delete mBackButton;

    old_VSResultsMenu_Destructor(this);
}

void VSResultsMenu::AddedToManager(Sexy::WidgetManager *theWidgetManager) {
    old_VSResultsMenu_AddedToManager(this, theWidgetManager);

    AddWidget(mBackButton);
}

void VSResultsMenu::processClientEvent(const BaseEvent *event) {
    LOG_DEBUG("TYPE:{}", (int)event->type);
    switch (event->type) {
        case EVENT_CLIENT_VSRESULT_BUTTON_DEPRESS: {
            auto *event1 = static_cast<const U8_Event *>(event);
            int anId = event1->data;
            gVSResultRequestState = anId;
        } break;
        default:
            break;
    }
}


void VSResultsMenu::processServerEvent(const BaseEvent *event) {
    LOG_DEBUG("TYPE:{}", (int)event->type);
    switch (event->type) {
        case EVENT_SERVER_VSRESULT_BUTTON_DEPRESS: {
            auto *event1 = static_cast<const U8_Event *>(event);
            int anId = event1->data;
            mResultsButtonId = anId;
            OnExit();
        } break;
        default:
            break;
    }
}

void VSResultsMenu::InitFromBoard(Board *board) {
    mBoardMainCounter = board->mMainCounter;
    mBoardBackground = Challenge::msVSShuffleMode ? BackgroundType(-1) : board->mBackground;
    int aSeedNum = board->mSeedBank[0]->mNumPackets;
    for (int i = 1; i < aSeedNum; ++i) {
        mPlantSeeds[i - 1] = board->mSeedBank[0]->mSeedPackets[i].mPacketType;
        mZombieSeeds[i - 1] = board->mSeedBank[1]->mSeedPackets[i].mPacketType;
    }
    old_VSResultsMenu_InitFromBoard(this, board);
}

void VSResultsMenu::Update() {
    //    if (mIsReplaySession) {
    //        return;
    //    }
    old_VSResultsMenu_Update(this);
}

void VSResultsMenu::HideReplayButton(bool forceHide) {
    Sexy::Widget *saveBtn = FindWidget(VSResultsMenu_Save_Replay);
    if (saveBtn == nullptr) {
        return;
    }
    const bool connected = (gTcpConnected || gTcpClientSocket >= 0);
    if (forceHide || !connected || mIsReplaySession) {
        saveBtn->SetVisible(false);
        saveBtn->mDisabled = true;
    }
}

void VSResultsMenu::OnExit() {
    if (mResultsButtonId == VSResultsMenu_Quit_VS) {
        gLawnApp->ShowMainMenuScreen();
        gLawnApp->KillVSResultsScreen();
    } else if (mResultsButtonId == VSResultsMenu_Play_Again) {
        gLawnApp->PreNewGame(GameMode::GAMEMODE_MP_VS, false);
        gLawnApp->KillVSResultsScreen();
    } else if (mResultsButtonId == VSResultsMenu_Back) {
        gLawnApp->ShowChallengeScreen(ChallengePage::CHALLENGE_PAGE_VS);
        gLawnApp->KillVSResultsScreen();
    }
}

void VSResultsMenu::ButtonDepress(int theId) {
    if (mIsFading)
        return;

    if (theId == VSResultsMenu_Save_Replay) {
        if (mIsReplaySession) {
            LOG_INFO("[REPLAY] ignore save replay in replay session");
            return;
        }
        ReplayMetaInfo meta;
        const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::tm localTm{};
        localtime_r(&now, &localTm);
        char timeText[32]{};
        std::strftime(timeText, sizeof(timeText), "%Y-%m-%d %H:%M", &localTm);
        meta.hostName = (gServerHostName[0] != '\0') ? gServerHostName : gLawnApp->mPlayerInfo->mName;
        meta.guestName = (gSecondPlayerName[0] != '\0') ? gSecondPlayerName : "Guest";
        int winnerSide = -1;
        int hostCampSide = -1;
        int guestCampSide = -1;
        for (int slot = 0; slot < 2; ++slot) {
            const int sideInSlot = mSides[slot];
            if (sideInSlot < 0 || sideInSlot > 1) {
                continue;
            }
            const int this76Side = unk2[sideInSlot];
            if (int *playerRecord = GetPlayerRecord((unsigned int)this76Side)) {
                if (winnerSide == -1 && (playerRecord[0] == 0 || playerRecord[0] == 1)) {
                    winnerSide = playerRecord[0];
                }
            }
            if (this76Side == 0) {
                hostCampSide = sideInSlot;
            } else if (this76Side == 1) {
                guestCampSide = sideInSlot;
            }
        }
        meta.hostCamp = CampNameFromSide(hostCampSide);
        meta.guestCamp = CampNameFromSide(guestCampSide);
        if (winnerSide == hostCampSide) {
            meta.winnerName = meta.hostName;
        } else if (winnerSide == guestCampSide) {
            meta.winnerName = meta.guestName;
        } else {
            meta.winnerName = "Unknown";
        }
        meta.mapName = StrFormat("VSBG_%d", int(mBoardBackground));
        meta.vsBackground = int(mBoardBackground);
        meta.durationTicks = mBoardMainCounter > 0 ? mBoardMainCounter : replay::EstimateRecordedDurationTicks();
        meta.plantDeck = BuildDeckText(mPlantSeeds, 6);
        meta.zombieDeck = BuildDeckText(mZombieSeeds, 6);
        meta.createdAt = timeText;
        meta.fileName = StrFormat("replay_%lld_%s_vs_%s.rpl", static_cast<long long>(now), meta.hostName.c_str(), meta.guestName.c_str());
        for (char &c : meta.fileName) {
            if (c == ' ' || c == '/' || c == '\\' || c == ':') {
                c = '_';
            }
        }
        const bool saved = replay::SaveCurrentMatchReplay(meta);
        LOG_INFO("[REPLAY] save button clicked, saved={}, file={}", saved, meta.fileName);
        if (saved && gLawnApp != nullptr) {
            HideReplayButton(true);
            mDrawReplaySaved = true;
        }
        return;
    }

    if ((gIsServerModeSpectator || gIsReplayMode) && (gTcpConnected || gTcpServerSocket >= 0 || gTcpClientSocket >= 0) && theId == VSResultsMenu::VSResultsMenu_Quit_VS) {
        if (gTcpServerSocket >= 0) {
            shutdown(gTcpServerSocket, SHUT_RDWR);
            close(gTcpServerSocket);
            gTcpServerSocket = -1;
        }
        if (gTcpClientSocket >= 0) {
            shutdown(gTcpClientSocket, SHUT_RDWR);
            close(gTcpClientSocket);
            gTcpClientSocket = -1;
        }
        gTcpConnected = false;
        gTcpConnecting = false;
        gIsServerModeNetplay = false;
        gServerModeTransport = ServerModeTransport::NONE;
        gIsServerModeSpectator = false;
        gIsReplayMode = false;
        gSecondPlayerName[0] = '\0';
        gServerHostName[0] = '\0';
        netplay::ClearSendBuffer();

        mResultsButtonId = theId;
        OnExit();
        return;
    }

    if (theId == VSResultsMenu::VSResultsMenu_Quit_VS) {
        mResultsButtonId = theId;
        OnExit();
        return;
    }

    if (gTcpConnected) {
        // 客户端点击再来一局或返回模式选择
        U8_Event event = {{EventType::EVENT_CLIENT_VSRESULT_BUTTON_DEPRESS}, uint8_t(theId)};
        netplay::PutEvent(event);
        gVSResultRequestState = theId;
        return;
    }

    if (gTcpClientSocket >= 0) {
        U8_Event event = {{EventType::EVENT_SERVER_VSRESULT_BUTTON_DEPRESS}, uint8_t(theId)};
        netplay::PutEvent(event);
    }

    mResultsButtonId = theId;
    OnExit();
}

void VSResultsMenu::Draw(Graphics *g) {
    old_VSResultsMenu_Draw(this, g);

    // 观战者不绘制这些
    if (gIsServerModeSpectator || gIsReplayMode) {
        return;
    }

    if (gIsServerModeNetplay && gServerModeTransport == ServerModeTransport::RELAY && mCheckboxController != nullptr) {
        mCheckboxController->DrawCheckboxLabel(g);
    }

    if (mDrawReplaySaved) {
        TodDrawString(g, "[REPLAY_SAVED]", 400, -20, Sexy::FONT_HOUSEOFTERROR28, Color(0, 205, 0, 255), DrawStringJustification::DS_ALIGN_CENTER);
    } else {
        if (gTcpConnected) {
            switch (gVSResultRequestState) {
                case VSResultsMenu::VSResultsMenu_Play_Again:
                    TodDrawString(g, "[VS_RESULT_REMIND_HOST_PLAY_AGAIN]", 400, -20, Sexy::FONT_HOUSEOFTERROR28, Color(0, 205, 0, 255), DrawStringJustification::DS_ALIGN_CENTER);
                    break;
                case VSResultsMenu::VSResultsMenu_Back:
                    TodDrawString(g, "[VS_RESULT_REMIND_HOST_BACK_TO_MODE_SELECT]", 400, -20, Sexy::FONT_HOUSEOFTERROR28, Color(0, 205, 0, 255), DrawStringJustification::DS_ALIGN_CENTER);
                    break;
                default:
                    break;
            }
        }

        if (gTcpClientSocket >= 0) {
            switch (gVSResultRequestState) {
                case VSResultsMenu::VSResultsMenu_Play_Again:
                    TodDrawString(g, "[VS_RESULT_OPPONENT_REQUEST_PLAY_AGAIN]", 400, -20, Sexy::FONT_HOUSEOFTERROR28, Color(0, 205, 0, 255), DrawStringJustification::DS_ALIGN_CENTER);
                    break;
                case VSResultsMenu::VSResultsMenu_Back:
                    TodDrawString(g, "[VS_RESULT_OPPONENT_REQUEST_BACK_TO_MODE_SELECT]", 400, -20, Sexy::FONT_HOUSEOFTERROR28, Color(0, 205, 0, 255), DrawStringJustification::DS_ALIGN_CENTER);
                    break;
                default:
                    break;
            }
        }
    }
}

void VSResultsMenu::ShowReplayButton() {
    if (mIsReplaySession) {
        return;
    }

    mSaveReplayButton = MakeButton(VSResultsMenu_Save_Replay, this, this, "[SAVE_REPLAY]");
    mSaveReplayButton->mDrawStoneButton = false;
    mSaveReplayButton->mButtonImage = addonImages.VS_Button;
    mSaveReplayButton->mOverImage = addonImages.VS_Button_selected;
    mSaveReplayButton->mDownImage = addonImages.VS_Button_selected;


    if (mSaveReplayButton != nullptr) {
        //            mSaveReplayButton->mTextOffsetX = -2;
        //            mSaveReplayButton->mTextOffsetY = -4;
        //            mSaveReplayButton->mTextDownOffsetX = 1;
        //            mSaveReplayButton->mTextDownOffsetY = 1;
        mSaveReplayButton->SetFont(Sexy::FONT_DWARVENTODCRAFT24);
        (*mSaveReplayButton->mColors)[ButtonWidget::COLOR_LABEL] = Color(25, 197, 45);
        (*mSaveReplayButton->mColors)[ButtonWidget::COLOR_LABEL_HILITE] = Color(277, 225, 108);
        mSaveReplayButton->mLabelJustify = BUTTON_LABEL_WRAP_CENTER;
        mSaveReplayButton->GameButton::Resize(-60, BUTTON_Y, BUTTON_WIDTH, BUTTON_HEIGHT);
        AddWidget(mSaveReplayButton);
    }
}

void VSResultsMenu::DrawInfoBox(Sexy::Graphics *a2, int a3) {
    Sexy::Widget *slotWidget = FindWidget(a3 + 2);
    if (slotWidget == nullptr) {
        return;
    }

    auto *infoWidget = reinterpret_cast<ImageWidgetLike *>(slotWidget);
    a2->PushState();
    a2->Translate(slotWidget->mX, slotWidget->mY);

    int sideInSlot = mSides[a3];
    if (sideInSlot < 0 || sideInSlot > 1) {
        a2->PopState();
        return;
    }
    int this76Side = unk2[sideInSlot];
    int *playerRecord = GetPlayerRecord((unsigned int)this76Side);
    DefaultProfileMgr *profileMgr = gLawnApp->mProfileMgr;
    PlayerInfo *profileObj = gLawnApp->mPlayerInfo;
    if (!mIsReplaySession) {
        if (profileMgr != nullptr) {
            profileObj = profileMgr->GetProfile(profileObj->mName, this76Side);
        }
        if (playerRecord == nullptr || profileObj == nullptr) {
            a2->PopState();
            return;
        }
    } else if (playerRecord == nullptr) {
        a2->PopState();
        return;
    }
    bool isZombieSlot = (sideInSlot != 0);

    infoWidget->mImage = isZombieSlot ? Sexy::IMAGE_VS_INFO_BOX_ZOMBIES : Sexy::IMAGE_VS_INFO_BOX_PLANTS;
    a2->DrawImage(Sexy::IMAGE_NO_GAMERPIC, 31, 52);
    a2->DrawImage(isZombieSlot ? Sexy::IMAGE_VS_INFO_BOX_ZOMBIES_OVERLAY : Sexy::IMAGE_VS_INFO_BOX_PLANTS_OVERLAY, 0, 0);
    pvzstl::string playerFmt = TodStringTranslate("[PLAYER_FMT]");
    pvzstl::string playerLabel = StrFormat(playerFmt.c_str(), a3 + 1);
    const char *replayHostName = (gReplayHostName[0] != '\0') ? gReplayHostName : gServerHostName;
    const char *replayGuestName = (gReplayGuestName[0] != '\0') ? gReplayGuestName : gSecondPlayerName;
    if ((gTcpConnected || gTcpClientSocket >= 0 || mIsReplaySession) && ((mIsReplaySession && replayGuestName[0] != '\0') || (!mIsReplaySession && gSecondPlayerName[0] != '\0'))) {
        const char *hostName =
            mIsReplaySession ? ((replayHostName[0] != '\0') ? replayHostName : gLawnApp->mPlayerInfo->mName) : ((gServerHostName[0] != '\0') ? gServerHostName : gLawnApp->mPlayerInfo->mName);
        const char *guestName = mIsReplaySession ? replayGuestName : gSecondPlayerName;
        playerLabel = (this76Side == 0) ? hostName : guestName;
    }

    a2->SetColor(Sexy::Color::White);
    a2->SetFont(Sexy::FONT_DWARVENTODCRAFT18);
    a2->DrawString(playerLabel, 42, 44);
    if (!mIsReplaySession) {
        int winStreak = playerRecord[3];
        profileObj->mWinStreak = winStreak;
        if (winStreak > 1) {
            pvzstl::string streakFmt = TodStringTranslate("[WIN_STREAK_FMT]");
            a2->DrawString(StrFormat(streakFmt.c_str(), winStreak), 263, 78);
        }
    }
    float trophyX = unk3[0];
    float trophyY = unk3[1];
    if (mSmokeCounter > 49) {
        trophyY = (float)TodAnimateCurve(50, 60, mSmokeCounter, 82, 74, TodCurves::CURVE_EASE_IN_OUT);
    } else {
        trophyY = (float)TodAnimateCurve(0, 50, mSmokeCounter, -60 - slotWidget->mY - Sexy::IMAGE_MP_TROPHY_BASE->mHeight - Sexy::IMAGE_MP_PLANT_TROPHY->mHeight, 82, TodCurves::CURVE_LINEAR);
    }

    int winnerSide = playerRecord[0];
    if (winnerSide != -1) {
        if (TodParticleSystem *smoke = gLawnApp->ParticleTryToGet(mSmokeParticleID)) {
            smoke->Draw(a2);
        }

        Sexy::Image *topTrophy = (winnerSide != 0) ? Sexy::IMAGE_MP_ZOMBIE_TROPHY : Sexy::IMAGE_MP_PLANT_TROPHY;
        a2->DrawImage(Sexy::IMAGE_MP_TROPHY_BASE, (int)(trophyX - Sexy::IMAGE_MP_TROPHY_BASE->mWidth / 2.0f), (int)trophyY);
        a2->DrawImage(topTrophy, (int)(trophyX + 2.0f - topTrophy->mWidth / 2.0f), (int)(trophyY - 46.0f));

        if (TodParticleSystem *sparkle = gLawnApp->ParticleTryToGet(mSparkleParticleID)) {
            sparkle->SystemMove(trophyX, trophyY);
            sparkle->Draw(a2);
        }
    }
    if (!mIsReplaySession) {
        float plantTrophyX = (winnerSide == -1) ? 117.0f : 192.0f;
        int plantWins = playerRecord[1];
        if (plantWins > 0) {
            float step = 196.0f / (float)plantWins;
            if (step > 52.0f) {
                step = 52.0f;
            }
            for (int i = 0; i < plantWins; i++) {
                a2->DrawImage(Sexy::IMAGE_MP_PLANT_TROPHY, (int)plantTrophyX, 82, 40, 40);
                plantTrophyX += step;
            }
        }
        float zombieTrophyX = (winnerSide == -1) ? 117.0f : 192.0f;
        int zombieWins = playerRecord[2];
        if (zombieWins > 0) {
            float step = 196.0f / (float)zombieWins;
            if (step > 52.0f) {
                step = 52.0f;
            }
            for (int i = 0; i < zombieWins; i++) {
                a2->DrawImage(Sexy::IMAGE_MP_ZOMBIE_TROPHY, (int)zombieTrophyX, 124, 40, 40);
                zombieTrophyX += step;
            }
        }
    } else {
        if (winnerSide == 0) {
            a2->DrawImage(Sexy::IMAGE_MP_PLANT_TROPHY, 192, 82, 40, 40);
        } else if (winnerSide == 1) {
            a2->DrawImage(Sexy::IMAGE_MP_ZOMBIE_TROPHY, 192, 124, 40, 40);
        }
    }
    a2->PopState();
}
