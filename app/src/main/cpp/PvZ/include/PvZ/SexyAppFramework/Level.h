//
// Created by Admin on 2026/6/29.
//

#ifndef PVZ_SEXYAPPFRAMEWORK_LEVEL_H
#define PVZ_SEXYAPPFRAMEWORK_LEVEL_H

#include "Homura/TypeUtils.h"
#include "PvZ/STL/map.h"
#include "PvZ/STL/string.h"

struct LevelPlantInfo {
    int mX;       // +0x00
    int mY;       // +0x04
    int mPlantId; // +0x08
};

static_assert(sizeof(LevelPlantInfo) == 0x0C);

template <typename T>
struct Vector32 {
    T *mBegin;       // +0x00
    T *mEnd;         // +0x04
    T *mCapacityEnd; // +0x08
};

static_assert(sizeof(Vector32<int>) == 0x0C);

namespace Sexy {
struct _Wave;

class Level {
public:
    int unk_00; // +0x00 = -1
    int unk_04; // +0x04 = -1
    int unk_08; // +0x08 = -1

    Vector32<void> unkVector_0C; // +0x0C，元素类型未知

    int unk_18; // +0x18 = 0

    Vector32<LevelPlantInfo> mPlants; // +0x1C

    Vector32<int> mForbiddenSeeds; // +0x28
    // 同时被 isCardNotAllowedToPick 和 isSeedForbidden 使用

    Vector32<void> unkVector_34; // +0x34，元素类型未知

    int mSeedBankLimit; // +0x40

    Vector32<int> mSeedConfig; // +0x44，

    int unk_50;         // +0x50 = 0
    int mFlagWaveCount; // +0x54 = 0

    bool unk_58;         // +0x58 = true
    bool pad_59[3];      // +0x59
    unsigned int unk_5C; // +0x5C，构造函数未初始化

    Vector32<void> unkVector_60; // +0x60，元素类型未知

    pvzstl::map<int, Sexy::_Wave> mWaves; // +0x6C

    homura::Storage<pvzstl::string> mStringStorage; // +0x84，旧版32位 std::string
};
} // namespace Sexy

#endif // PVZ_SEXYAPPFRAMEWORK_LEVEL_H
