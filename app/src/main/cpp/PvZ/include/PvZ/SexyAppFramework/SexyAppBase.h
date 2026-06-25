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

#ifndef PVZ_SEXYAPPFRAMEWORK_SEXY_APP_BASE_H
#define PVZ_SEXYAPPFRAMEWORK_SEXY_APP_BASE_H

#include "PvZ/MagicNumbers.h"
#include "PvZ/STL/string.h"
#include "PvZ/SexyAppFramework/Widget/WidgetManager.h"
#include "PvZ/Symbols.h"

#include "Graphics/Color.h"
#include "Graphics/MemoryImage.h"
#include "Misc/Common.h"
#include "Misc/Rect.h"
#include "Sound/AudiereSoundManager.h"
#include "Widget/ButtonListener.h"

void InitHookFunction();

// ARM32 libstdc++ std::_Rb_tree footprint used by the game.
// mHeaderColor is embedded in the header/end sentinel; mRoot is the pointer
// passed to _M_erase(), and mLeftmost is begin().
struct SexyOpaqueTree32 {
    unsigned int mCompareOrPadding; // +0x00
    unsigned int mHeaderColor;      // +0x04, &mHeaderColor is end()
    unsigned int mRoot;             // +0x08
    unsigned int mLeftmost;         // +0x0C, begin()
    unsigned int mRightmost;        // +0x10
    unsigned int mNodeCount;        // +0x14
};

struct SexyOpaqueList32 {
    unsigned int next;
    unsigned int prev; // 0x08
};

struct SexyOpaqueThread32 {
    unsigned int words[2]; // observed 0x08
};

struct SexyOpaqueCritSect32 {
    unsigned int impl; // observed 0x04
};

struct SexyOpaqueRatio32 {
    int numerator;
    int denominator; // 0x08
};

struct SexyOpaqueRect32 {
    int x;
    int y;
    int width;
    int height; // 0x10
};

// ARM32 std::vector<std::string> storage.
struct SexyStringVector32 {
    unsigned int mBegin;
    unsigned int mEnd;
    unsigned int mCapacityEnd;
};

// Exact 0x34-byte footprint observed around AddParameterEntries().
// Only mSelectedIndex's constructor value (-1) is known; the remaining
// semantics are intentionally left generic.
struct SexyParameterStorage32 {
    int mField00;
    int mField04;
    int mField08;
    int mField0C;
    int mSelectedIndex;
    unsigned char mData[0x20];
};


namespace Sexy {
class Gamepad;
class Dialog;


class SexyAppBase {

public:
    void **vTable;    // 0x000, dword 0
    int *mVtables[2]; // 0x004, dword 1~2
    int mRandSeed;    // 0x00C, dword 3

    homura::Storage<pvzstl::string> mCompanyName;           // 0x010
    homura::Storage<pvzstl::string> mFullCompanyName;       // 0x014
    homura::Storage<pvzstl::string> mProdName;              // 0x018
    homura::Storage<pvzstl::string> mTitle;                 // 0x01C
    homura::Storage<pvzstl::string> mRegKey;                // 0x020
    homura::Storage<pvzstl::string> mChangeDirTo;           // 0x024
    homura::Storage<pvzstl::string> mResourceManifestPath;  // 0x028
    homura::Storage<pvzstl::string> mDefaultPropertiesPath; // 0x02C

    int mRelaxUpdateBacklogCount; // 0x030, dword 12
    int mPreferredX;              // 0x034, dword 13
    int mPreferredY;              // 0x038, dword 14
    int mPreferredX2;             // 0x03C, tentative
    int mPreferredY2;             // 0x040, tentative
    int unk_044;                  // 0x044
    int unk_048;                  // 0x048

    int mWidth;              // 0x04C, dword 19
    int mHeight;             // 0x050, dword 20
    int mFullscreenBits;     // 0x054, dword 21
    double mMusicVolume;     // 0x058
    double mSfxVolume;       // 0x060
    double mDemoMusicVolume; // 0x068
    double mDemoSfxVolume;   // 0x070

    bool mNoSoundNeeded;         // 0x078
    bool mWantFMod;              // 0x079
    bool mCmdLineParsed;         // 0x07A
    bool mSkipSignatureChecks;   // 0x07B
    bool mStandardWordWrap;      // 0x07C
    bool mAllowExtendedChars;    // 0x07D
    bool mOnlyAllowOneCopyToRun; // 0x07E
    bool unk_07F;                // 0x07F

    void *mMutex;                            // 0x080
    SexyOpaqueCritSect32 mCritSect;          // 0x084
    SexyOpaqueCritSect32 mSecondaryCritSect; // 0x088, Android addition
    int mNotifyGameMessage;                  // 0x08C, tentative name
    bool mBetaValidate;                      // 0x090
    unsigned char mAdd8BitMaxTable[512];     // 0x091, values 0..255 then 0xFF
    unsigned char pad_291[3];                // 0x291

    WidgetManager *mWidgetManager; // 0x294, dword 165

    SexyOpaqueTree32 mDialogMap;       // 0x298
    SexyOpaqueList32 mDialogList;      // 0x2B0
    unsigned int mPrimaryThreadId;     // 0x2B8
    SexyOpaqueThread32 mPrimaryThread; // 0x2BC, Android addition

    bool unkAppState_2C4;          // 0x2C4, likely mSEHOccured from PC member order
    bool mShutDown;                // 0x2C5, confirmed by external xref
    bool unkAppStateFlags_2C6[10]; // 0x2C6~0x2CF; first byte is likely mExitToTop
    unsigned int mTimeLoaded;      // 0x2D0
    void *mPlatformWindow1;        // 0x2D4
    void *mPlatformWindow2;        // 0x2D8
    bool mIsScreenSaver;           // 0x2DC = false
    bool mAllowMonitorPowersave;   // 0x2DD = true
    bool mNoDefer;                 // 0x2DE = false
    bool mFullScreenPageFlip;      // 0x2DF = true

    bool mTabletPC; // 0x2E0 = false，高置信
    bool pad_2E1[3];

    void *mPlatformDriverOrQueue; // 0x2E4 = nullptr，准确类型未知

    void *mFileDriver;       // 0x2E8
    void *mAppDriver;        // 0x2EC
    void *mDDInterface;      // 0x2F0, high-confidence slot
    void *mAudioDriver;      // 0x2F4
    void *mResStreamsDriver; // 0x2F8
    void *mHttpDriver;       // 0x2FC
    bool mAlphaDisabled;     // 0x300
    bool pad_301[3];
    void *mMusicInterface;  // 0x304
    bool mReadFromRegistry; // 0x308
    bool pad_309[3];
    homura::Storage<pvzstl::string> mRegisterLink;   // 0x30C
    homura::Storage<pvzstl::string> mProductVersion; // 0x310
    void *mCursorImages[13];                         // 0x314, dword 197~209
    int unkCursorPlatform[26];                       // 0x348~0x3AF

    bool mLawnMouseMode;     // 0x3B0
    bool mIsOpeningURL;      // 0x3B1
    bool mShutdownOnURLOpen; // 0x3B2, tentative
    bool pad_3B3;

    homura::Storage<pvzstl::string> mOpeningURL; // 0x3B4
    unsigned int mOpeningURLTime;                // 0x3B8
    unsigned int mLastTimerTime;                 // 0x3BC
    unsigned int mLastBigDelayTime;              // 0x3C0
    unsigned int pad_3C4;
    double mUnmutedMusicVolume; // 0x3C8
    double mUnmutedSfxVolume;   // 0x3D0
    int mMuteCount;             // 0x3D8
    int mAutoMuteCount;         // 0x3DC
    bool mDemoMute;             // 0x3E0
    bool mMuteOnLostFocus;      // 0x3E1
    bool pad_3E2[2];

    SexyOpaqueTree32 mMemoryImageSet;          // 0x3E4, std::set<MemoryImage*>
    SexyOpaqueCritSect32 mMemoryImageCritSect; // 0x3FC, Android addition
    SexyOpaqueTree32 mPIEffectSet;             // 0x400, std::set<PIEffect*>
    SexyOpaqueTree32 mPopAnimSet;              // 0x418, std::set<PopAnim*>
    SexyOpaqueTree32 mSharedImageMap;          // 0x430, map<pair<string,string>, SharedImage>
    bool mCleanupSharedImages;                 // 0x448
    bool pad_449[3];
    SexyOpaqueTree32 mImageGroupMap;    // 0x44C, map<string, map<string, Image*>>
    SexyOpaqueTree32 mImageIdStringMap; // 0x464, map<int, string>

    int mNonDrawCount;      // 0x47C
    int mFrameTime;         // 0x480
    bool mIsDrawing;        // 0x484
    bool mLastDrawWasEmpty; // 0x485
    bool mHasPendingDraw;   // 0x486
    bool pad_487;
    double mPendingUpdatesAcc;       // 0x488
    double mUpdateFTimeAcc;          // 0x490
    unsigned int mLastTimeCheck;     // 0x498
    unsigned int mLastTime;          // 0x49C
    unsigned int mLastUserInputTick; // 0x4A0
    int mSleepCount;                 // 0x4A4
    int mDrawCount;                  // 0x4A8
    int mUpdateCount;                // 0x4AC
    int mUpdateAppState;             // 0x4B0
    int mUpdateAppDepth;             // 0x4B4
    int unk_4B8;                     // 0x4B8
    int mUpdateFrameTime;            // 0x4BC, initialized to 10; name tentative
    double mUpdateMultiplier;        // 0x4C0
    bool mPaused;                    // 0x4C8
    bool pad_4C9[3];
    int mFastForwardToUpdateNum; // 0x4CC
    bool mFastForwardToMarker;   // 0x4D0
    bool mFastForwardStep;       // 0x4D1
    bool pad_4D2[2];
    unsigned int mLastDrawTick; // 0x4D4
    unsigned int mNextDrawTick; // 0x4D8
    int mStepMode;              // 0x4DC
    int mCursorNum;             // 0x4E0
    int unkCursorState_4E4;     // 0x4E4

    AudiereSoundManager *mSoundManager; // 0x4E8, dword 314

    void *mHandCursor;                // 0x4EC
    void *mDraggingCursor;            // 0x4F0
    int unkCursorPlatform_4F4[7];     // 0x4F4~0x50F
    int mMouseX;                      // 0x510, remapped current mouse X
    int mMouseY;                      // 0x514, remapped current mouse Y
    SexyOpaqueList32 mSafeDeleteList; // 0x518, list<WidgetSafeDeleteInfo>

    bool mMouseIn;       // 0x520
    bool mRunning;       // 0x521
    bool mActive;        // 0x522
    bool mMinimized;     // 0x523
    bool mPhysMinimized; // 0x524
    bool mIsDisabled;    // 0x525
    bool mHasFocus;      // 0x526
    bool unk_527;

    int mDrawTime;              // 0x528
    unsigned int mFPSStartTick; // 0x52C
    int mFPSFlipCount;          // 0x530
    int mFPSDirtyCount;         // 0x534
    int mFPSTime;               // 0x538
    int mFPSCount;              // 0x53C
    bool mShowFPS;              // 0x540
    bool pad_541[3];
    int mShowFPSMode;   // 0x544
    int mScreenBltTime; // 0x548
    int unkPerf_54C[8]; // 0x54C ~ 0x56B

    bool mAutoStartLoadingThread; // 0x56C
    bool mLoadingThreadStarted;   // 0x56D
    bool mLoadingThreadCompleted; // 0x56E
    bool mLoaded;                 // 0x56F

    bool mYieldMainThread;   // 0x570
    bool mLoadingFailed;     // 0x571
    bool unkLoadingFlag_572; // 0x572
    bool unkLoadingFlag_573; // 0x573

    SexyOpaqueThread32 mLoadingThread; // 0x574

    bool mCursorThreadRunning;     // 0x57C
    bool mSysCursor;               // 0x57D
    bool mCustomCursorsEnabled;    // 0x57E
    bool mCustomCursorDirty;       // 0x57F
    bool mLastShutdownWasGraceful; // 0x580
    bool mIsWideWindow;            // 0x581
    bool unkLoadingFlag_582;       // 0x582
    bool unkLoadingFlag_583;       // 0x583
    bool unkLoadingFlag_584;       // 0x584
    bool unkLoadingFlag_585;       // 0x585
    bool unkLoadingFlag_586;       // 0x586
    bool unkLoadingFlag_587;       // 0x587

    int mNumLoadingThreadTasks;       // 0x588, dword 354
    int mCompletedLoadingThreadTasks; // 0x58C, dword 355

    bool mRecordingDemoBuffer; // 0x590
    bool mPlayingDemoBuffer;   // 0x591
    bool mManualShutdown;      // 0x592
    bool pad_593;
    homura::Storage<pvzstl::string> mDemoPrefix;   // 0x594
    homura::Storage<pvzstl::string> mDemoFileName; // 0x598
    char mDemoBuffer[0x1C];                        // 0x59C
    int mDemoLength;                               // 0x5B8
    int mLastDemoMouseX;                           // 0x5BC
    int mLastDemoMouseY;                           // 0x5C0
    int mLastDemoUpdateCnt;                        // 0x5C4
    bool mDemoNeedsCommand;                        // 0x5C8
    bool mDemoIsShortCmd;                          // 0x5C9
    bool pad_5CA[2];
    int mDemoCmdNum;           // 0x5CC
    int mDemoCmdOrder;         // 0x5D0
    int mDemoCmdBitPos;        // 0x5D4
    bool mDemoLoadingComplete; // 0x5D8
    bool pad_5D9[3];
    int mCurHandleNum;                // 0x5DC
    SexyOpaqueList32 mDemoMarkerList; // 0x5E0

    bool mDebugKeysEnabled;     // 0x5E8
    bool mEnableMaximizeButton; // 0x5E9
    bool mCtrlDown;             // 0x5EA
    bool mAltDown;              // 0x5EB
    bool unk_5EC[4];            // 0x5EC

    int mSyncRefreshRate;                   // 0x5F0
    bool mVSyncUpdates;                     // 0x5F4
    bool mSoftVSyncWait;                    // 0x5F5, default true
    bool mVSyncBroken;                      // 0x5F6
    bool unkVSync_5F7;                      // 0x5F7
    int mVSyncBrokenCount;                  // 0x5F8
    unsigned int mVSyncBrokenTestStartTick; // 0x5FC
    unsigned int mVSyncBrokenTestUpdates;   // 0x600
    bool mWaitForVSync;                     // 0x604
    bool mUserChanged3DSetting;             // 0x605
    bool mAutoEnable3D;                     // 0x606
    bool mTest3D;                           // 0x607
    bool mIs3DAccelerated;                  // 0x608
    bool unk3D_609;                         // 0x609
    bool pad_60A[2];

    unsigned int mMinVidMemory3D;         // 0x60C, unkMem5[0] = 6
    unsigned int mRecommendedVidMemory3D; // 0x610, unkMem5[1] = 14

    bool mWidescreenAware;          // 0x614, unkMem5[2].byte0
    bool unkDisplayFlag_615;        // 0x615, initialized true
    bool unkDisplayFlag_616;        // 0x616
    bool unkDisplayFlag_617;        // 0x617
    SexyOpaqueRect32 mScreenBounds; // 0x618, unkMem5[3]~[6]

    bool mEnableWindowAspect; // 0x628, unkMem5[7].byte0
    bool pad_629[3];
    SexyOpaqueRatio32 mWindowAspect;       // 0x62C, 4:3
    SexyOpaqueRatio32 mPresentationAspect; // 0x634, 4:3, Android addition
    SexyOpaqueRatio32 mWideScreenAspect;   // 0x63C, 16:10, Android addition
    int unk_644;                           // 0x644
    int mScreenWidth;                      // 0x648 = 800
    int mScreenHeight;                     // 0x64C = 600
    bool unkDisplayFlags_650[4];           // 0x650

    SexyOpaqueTree32 mStringProperties;       // 0x654, map<string, SexyString>
    SexyOpaqueTree32 mBoolProperties;         // 0x66C, map<string, bool>
    SexyOpaqueTree32 mIntProperties;          // 0x684, map<string, int>
    SexyOpaqueTree32 mDoubleProperties;       // 0x69C, map<string, double>
    SexyOpaqueTree32 mStringVectorProperties; // 0x6B4, map<string, vector<string>>
    int *mResourceManager;                    // 0x6CC, unkMem5[48]

    char mPopLoc[0x30];                            // 0x6D0, unkMem5[49]~[60]
    int *mAuthManager;                             // 0x700, unkMem5[61]
    int unkAuth_704[3];                            // 0x704, unkMem5[62]~[64]
    int *mInputManager;                            // 0x710, unkMem5[65]
    int *mInputConnectManager;                     // 0x714, unkMem5[66]
    homura::Storage<pvzstl::string> unkString_718; // 0x718
    SexyStringVector32 mUnknownStringVector;       // 0x71C, vector<string>
    void *mUnknownHeap_728;                        // 0x728, separately deleted
    int unk_72C;                                   // 0x72C
    int unk_730;                                   // 0x730
    float unkFloat_734;                            // 0x734 = 100.0f
    float unkFloat_738;                            // 0x738 = 100.0f
    int unkInt_73C;                                // 0x73C = 100
    int mMaxFPS;                                   // 0x740, SEXY_MAX_FPS
    int unk_744[2];                                // 0x744
    bool unkFlag_74C;                              // 0x74C = true
    bool pad_74D[3];
    homura::Storage<pvzstl::string> unkString_750; // 0x750
    SexyParameterStorage32 mParameterStorage;      // 0x754
    void *mParameterEntriesHeap;                   // 0x788, deleted in destructor
    int unk_78C;                                   // 0x78C
    int unk_790;                                   // 0x790
    bool unkFlags_794[4];                          // 0x794
    int unk_798;                                   // 0x798
    bool mThreadedPreload;                         // 0x79C, default true
    bool unkPreload_79D;                           // 0x79D
    bool mSafeReload;                              // 0x79E, SEXY_SAFE_RELOAD
    bool unkPreload_79F;                           // 0x79F

    int unkTailToPartSize[SexyAppBasePartSize]; // 0x7A0~0x837, dword 488~525
    Sexy::Gamepad *mGamepads[2];                // 0x828～0x82F

    int unkTailAfterGamepads[2]; // 0x830～0x837
    bool unkBool1;               // 0x838
    bool mGamePad1IsOn;          // 0x839
    bool pad_83A[2];
    int unkMem7[22];    // 0x83C~0x893
    bool unkBool2;      // 0x894
    bool mGamePad2IsOn; // 0x895
    bool pad_896[2];
    int unkMem8[2]; // 0x898~0x89F
    // 115： 552 , 111： 553

    Dialog *GetDialog(Dialogs theDialogId) { // vTable + 4 * 103
        return reinterpret_cast<Dialog *(*)(SexyAppBase *, Dialogs)>(Sexy_SexyAppBase_GetDialogAddr)(this, theDialogId);
    }
    Image *GetImage(const pvzstl::string &theFileName, bool commitBits = true) {
        return reinterpret_cast<Image *(*)(SexyAppBase *, const pvzstl::string &, bool)>(Sexy_SexyAppBase_GetImageAddr)(this, theFileName, commitBits);
    }
    bool RegistryReadString(const pvzstl::string &theValueName, pvzstl::string *theString) {
        return reinterpret_cast<bool (*)(SexyAppBase *, const pvzstl::string &, pvzstl::string *)>(Sexy_SexyAppBase_RegistryReadStringAddr)(this, theValueName, theString);
    }
    Image *CopyImage(Image *theImage) {
        return reinterpret_cast<Image *(*)(SexyAppBase *, Image *)>(Sexy_SexyAppBase_CopyImageAddr)(this, theImage);
    }
    Image *CopyImage(Image *theImage, const Rect &theRect) {
        return reinterpret_cast<Image *(*)(SexyAppBase *, Image *, const Rect &)>(Sexy_SexyAppBase_CopyImage2Addr)(this, theImage, theRect);
    }
    void AddDialog(Dialogs id, Dialog *theDialog) { // vTable + 4 * 104
        reinterpret_cast<void (*)(SexyAppBase *, Dialogs, Dialog *)>(Sexy_SexyAppBase_AddDialog2Addr)(this, id, theDialog);
    }
    void AddDialog(Dialog *theDialog) {
        reinterpret_cast<void (*)(SexyAppBase *, Dialog *)>(Sexy_SexyAppBase_AddDialogAddr)(this, theDialog);
    }
    int GetDialogCount() { // vTable + 4 * 109
        return reinterpret_cast<int (*)(SexyAppBase *)>(Sexy_SexyAppBase_GetDialogCountAddr)(this);
    }
    void DoParseCmdLine() { // vTable + 4 * 62
        reinterpret_cast<void (*)(SexyAppBase *)>(Sexy_SexyAppBase_DoParseCmdLineAddr)(this);
    }
    int GetInteger(const pvzstl::string &theName, int defValue) {
        return reinterpret_cast<bool (*)(SexyAppBase *, const pvzstl::string &, int)>(Sexy_SexyAppBase_GetIntegerAddr)(this, theName, defValue);
    }
    int SetCursor(int theCursorNum) {
        return reinterpret_cast<int (*)(SexyAppBase *, int)>(Sexy_SexyAppBase_SetCursorAddr)(this, theCursorNum);
    }
    void LoadResourceManifest() {
        reinterpret_cast<void (*)(SexyAppBase *)>(Sexy_SexyAppBase_LoadResourceManifestAddr)(this);
    }
    void StartLoadingThread() {
        reinterpret_cast<void (*)(SexyAppBase *)>(Sexy_SexyAppBase_StartLoadingThreadAddr)(this);
    }
    double GetLoadingThreadProgress() {
        return reinterpret_cast<double (*)(SexyAppBase *)>(Sexy_SexyAppBase_GetLoadingThreadProgressAddr)(this);
    }

    bool UpdateApp();
    bool EraseFile(const pvzstl::string &theFileName);

protected:
    SexyAppBase() = default;
    ~SexyAppBase() = default;

    friend void ::InitHookFunction();

    void _constructor();
};

} // namespace Sexy

inline void (*old_Sexy_SexyAppBase_SexyAppBase)(Sexy::SexyAppBase *appBase);

inline bool (*old_Sexy_SexyAppBase_UpdateApp)(Sexy::SexyAppBase *appBase);

#endif // PVZ_SEXYAPPFRAMEWORK_SEXY_APP_BASE_H
