//
// Created by Admin on 2026/6/24.
//

#ifndef PLANTSVSZOMBIES_ANDROIDTV_STARTBUTTON_H
#define PLANTSVSZOMBIES_ANDROIDTV_STARTBUTTON_H

#include "ButtonWidget.h"

class TitleScreen;


class StartButton : public Sexy::ButtonWidget {
public:
    Sexy::Color mColor;                     // 816
    Sexy::Color mOverColor;                 // 832
    TitleScreen *mParent;                   // 212 * 4
    Sexy::Font *mFont;                      // 852
    homura::Storage<pvzstl::string> mLabel; // 856
    int mGamepadIndex;                      // 215
    int mTextDrawMode;                      // 216
    float mTextAlpha;                       // 217
    int mUnderlineOffset;


}; // 大小 218 个 int


#endif // PLANTSVSZOMBIES_ANDROIDTV_STARTBUTTON_H
