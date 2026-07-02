//
// Created by Admin on 2026/6/26.
//

#ifndef PVZ_SEXYAPPFRAMEWORK_BUFFER_H
#define PVZ_SEXYAPPFRAMEWORK_BUFFER_H

#include <cstdint>
#include <Homura/TypeUtils.h>
#include <PvZ/Symbols.h>
#include <string>
#include <vector>
namespace Sexy {

class Buffer {
public:
    void **mVtable;
    homura::Storage<std::vector<unsigned char>> mData;
    int mDataBitSize;
    mutable int mReadBitPos;
    mutable int mWriteBitPos;
    Buffer() {
        mVtable = reinterpret_cast<void **>(reinterpret_cast<uintptr_t>(vTableForSexyBufferAddr) + 8);
        mData.Construct();
        mDataBitSize = 0;
        mReadBitPos = 0;
        mWriteBitPos = 0;
    }
    ~Buffer() {
        mData.Destruct();
    }
};


} // namespace Sexy
#endif // PVZ_SEXYAPPFRAMEWORK_BUFFER_H
