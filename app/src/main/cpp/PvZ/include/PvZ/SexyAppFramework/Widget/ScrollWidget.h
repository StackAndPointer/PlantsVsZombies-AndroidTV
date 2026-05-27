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

#ifndef PVZ_LAWN_WIDGET_SCROLL_WIDGET_H
#define PVZ_LAWN_WIDGET_SCROLL_WIDGET_H

#include "PvZ/SexyAppFramework/Misc/Point.h"
#include "PvZ/SexyAppFramework/Widget/Widget.h"
#include <list>

inline constexpr float SCROLL_TARGET_THRESHOLD_NORM = 0.01f;
inline constexpr float SCROLL_VELOCITY_THRESHOLD_NORM = 0.0001f;
inline constexpr float SCROLL_DEVIATION_DAMPING = 0.5f;
inline constexpr float SCROLL_SPRINGBACK_TENSION = 0.1f;
inline constexpr float SCROLL_VELOCITY_FILTER_WINDOW = 0.1f;
inline constexpr float SCROLL_VELOCITY_DAMPING = 0.975f;
inline constexpr float SCROLL_VELOCITY_DEVIATION_DAMPING = 0.85f;
inline constexpr float SCROLL_DRAG_THRESHOLD = 4.0f;
inline constexpr float SCROLL_PAGE_FLICK_THRESHOLD = 40.0f;
inline constexpr int SCROLL_TAP_DELAY_TICKS = 10;
inline constexpr int SCROLL_INDICATORS_FLASH_TICKS = 100;
inline constexpr float SCROLL_INDICATORS_FADE_IN_RATE = 0.05f;
inline constexpr float SCROLL_INDICATORS_FADE_OUT_RATE = 0.02f;

// class PageControl : public Sexy::Widget {
// protected:
//     Sexy::Image* mPartsImage;
//     int mNumberOfPages;
//     int mCurrentPage;

// public:
//     PageControl(Sexy::Image* partsImage);
//     ~PageControl();
//     void SetNumberOfPages(int count);
//     void SetCurrentPage(int page);
//     int GetCurrentPage();
//     void Draw(Sexy::Graphics* g); // Is a new function in WP, not an overridden virtual function.
// }

using CGPoint = Sexy::TPoint<float>;

class ScrollWidgetOverlay {
public:
    Sexy::Image *image;
    CGPoint offset;
};


struct _Touch {
public:
    Sexy::Point location;
    // Sexy::Point previousLocation;
    // int tapCount;
    double timestamp;
};


class ScrollWidget : public Sexy::Widget {
public:
    enum ScrollMode { Disabled, Horizontal, Vertical, Both };

    bool clientAllowsScroll;
    bool mClientDownDispatched;
    int mDeferredMouseDownMagicCode;
    Widget *mClient;
    Widget *mClientLastDown;
    Sexy::Image *mIndicatorsImage;
    Sexy::Image *mBackgroundImage;
    bool mFillBackground;
    std::vector<ScrollWidgetOverlay *> mOverlays;
    bool mDrawOverlays;
    ScrollMode mScrollMode;
    CGPoint mScrollTouchReference;
    Sexy::Insets mScrollInsets;
    CGPoint mScrollTarget;
    CGPoint mScrollOffset;
    CGPoint mScrollOffsetReference;
    CGPoint mScrollVelocity;
    bool mBounceEnabled;
    bool mIndicatorsEnabled;
    Sexy::Insets mIndicatorsInsets;
    int mIndicatorsFlashTimer;
    float mIndicatorsOpacity;
    int mCurrentPageHorizontal;
    int mCurrentPageVertical;
    bool mSeekScrollTarget;
    bool mScrollTracking;
    double mScrollLastTimestamp;
    float mSpringOverride;
    CGPoint mScrollMin;
    CGPoint mScrollMax;
    CGPoint mPageSize;
    ScrollMode mScrollPractical;
    int mPageCountHorizontal;
    int mPageCountVertical;

    ScrollWidget() {
        _constructor();
    }
    /*virtual */ ~ScrollWidget() {
        _destructor();
    }

    /*virtual */ void Draw(Sexy::Graphics *g);
    /*virtual */ void Update();
    void SetScrollMode(ScrollWidget::ScrollMode mode);
    void SetScrollInsets(Sexy::Insets &insets);
    void SetScrollOffset(CGPoint offset, bool animated);
    void ScrollToMin(bool animated);
    void ScrollToBottom(bool animated);
    void ScrollToPoint(CGPoint &point, bool animated);
    void ScrollRectIntoView(Sexy::Rect &rect, bool animated);
    void EnableBounce(bool enable);
    void EnablePaging(bool enable);
    void EnableIndicators(Sexy::Image *indicatorsImage);
    void SetIndicatorsInsets(Sexy::Insets &insets);
    void FlashIndicators();
    void SetPageHorizontal(int page, bool animated);
    void SetPageVertical(int page, bool animated);
    void SetPage(int hpage, int vpage, bool animated);
    int GetPageHorizontal();
    int GetPageVertical();
    void SetBackgroundImage(Sexy::Image *image);
    void EnableBackgroundFill(bool enable);
    void AddOverlayImage(Sexy::Image *image, CGPoint &offset);
    void EnableOverlays(bool enable);
    /*virtual */ void AddWidget(Sexy::Widget *theWidget);
    /*virtual */ void RemoveWidget(Sexy::Widget *theWidget);
    /*virtual */ void Resize(int x, int y, int width, int height);
    void ClientSizeChanged();
    /*virtual */ void MouseDown(int x, int y, int theMagicCode);
    /*virtual */ void MouseUp(int x, int y, int theMagicCode);
    /*virtual */ void MouseDrag(int x, int y);
    // /*virtual */void MouseWheel(int theDelta);

    void TouchBegan(_Touch touch, int theMagicCode);
    void TouchMoved(_Touch touch);
    void TouchEnded(_Touch touch, int theMagicCode);

    void TouchMotion(_Touch touch);
    void DoScrollUpdate();
    void DrawHorizontalStretchableImage(Sexy::Graphics *g, Sexy::Image *image, Sexy::Rect &destRect);
    void DrawVerticalStretchableImage(Sexy::Graphics *g, Sexy::Image *image, Sexy::Rect &destRect);
    // void DrawProxyWidget(Sexy::Graphics* g, ProxyWidget* proxyWidget);
    void SnapToPage();
    Sexy::Widget *GetClientWidgetAt(int x, int y);
    Sexy::Widget *GetClientWidgetAt(_Touch touch);
    CGPoint &GetScrollOffset();
    void SetScrollOffset(float x, float y);
    void CacheDerivedValues();

protected:
    void _constructor();
    void _destructor();
    void _destructor2();
};

#endif // PVZ_LAWN_WIDGET_SCROLL_WIDGET_H
