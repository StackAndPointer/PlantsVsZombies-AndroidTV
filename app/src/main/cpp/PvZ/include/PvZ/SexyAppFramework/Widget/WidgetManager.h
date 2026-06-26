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

#ifndef PVZ_SEXYAPPFRAMEWORK_WIDGET_WIDGET_MANAGER_H
#define PVZ_SEXYAPPFRAMEWORK_WIDGET_WIDGET_MANAGER_H

#include "../Misc/Common.h"
#include "../Misc/KeyCodes.h"
#include "WidgetContainer.h"

namespace Sexy {

class Widget;

struct DeferredOverlayVector {
    void *mStart;        // +0x00
    void *mFinish;       // +0x04
    void *mEndOfStorage; // +0x08
};

struct KeyDownMapLayout {
    unsigned int mComparatorOrPadding; // +0x00
    RbTreeHeader32 mHeader;            // +0x04
    unsigned int mNodeCount;           // +0x14
};

class WidgetManager : public WidgetContainer {
public:
    Graphics *mCurG;     // +0x00
    SexyAppBase *mApp;   // +0x04
    MemoryImage *mImage; // +0x08
    MemoryImage *mTransientImage;
    bool mLastHadTransients;
    Widget *mPopupCommandWidget;
    DeferredOverlayVector mDeferredOverlayWidgets;
    int mMinDeferredOverlayPriority;
    bool mHasFocus;
    Widget *mFocusWidget; // 40
    Widget *mLastDownWidget;
    Widget *mOverWidget;
    Widget *mBaseModalWidget;
    FlagsMod mLostFocusFlagsMod;
    FlagsMod mBelowModalFlagsMod;
    FlagsMod mDefaultBelowModalFlagsMod;
    int mPreModalInfoList[2]; // std::list<PreModalInfo> mPreModalInfoList;
    Rect mMouseDestRect;
    Rect mMouseSourceRect;
    bool mMouseIn;
    int mLastMouseX;
    int mLastMouseY;
    int mDownButtons;
    int mActualDownButtons;
    int mLastInputUpdateCnt;
    KeyDownMapLayout mKeyDown;
    int mLastDownButtonId;
    int mWidgetFlags;
    char unkMem6[60]; // 推测和AxisMove有关

    // 大小89个整数
    void SetFocus(Widget *aWidget) {
        reinterpret_cast<void (*)(WidgetManager *, Widget *)>(Sexy_WidgetManager_SetFocusAddr)(this, aWidget);
    }
    Widget *GetWidgetAt(int x, int y, int *theWidgetX, int *theWidgetY) {
        return reinterpret_cast<Widget *(*)(WidgetManager *, int, int, int *, int *)>(Sexy_WidgetManager_GetWidgetAtAddr)(this, x, y, theWidgetX, theWidgetY);
    }
    int GetWidgetFlags() {
        return reinterpret_cast<int (*)(WidgetManager *)>(Sexy_WidgetManager_GetWidgetFlagsAddr)(this);
    }
    void RehupMouse() {
        reinterpret_cast<void (*)(WidgetManager *)>(Sexy_WidgetManager_RehupMouseAddr)(this);
    }

    void MouseDown(int x, int y, int theClickCount);
    void MouseDrag(int x, int y);
    void MouseUp(int x, int y, int theClickCount);
};

} // namespace Sexy

inline void (*old_Sexy_WidgetManager_MouseDown)(Sexy::WidgetManager *, int x, int y, int theClickCount);
inline void (*old_Sexy_WidgetManager_MouseDrag)(Sexy::WidgetManager *, int x, int y);
inline void (*old_Sexy_WidgetManager_MouseUp)(Sexy::WidgetManager *, int x, int y, int theClickCount);


#endif // PVZ_SEXYAPPFRAMEWORK_WIDGET_WIDGET_MANAGER_H
