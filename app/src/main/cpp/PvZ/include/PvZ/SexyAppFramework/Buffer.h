//
// Created by Admin on 2026/6/26.
//

#ifndef PVZ_SEXYAPPFRAMEWORK_BUFFER_H
#define PVZ_SEXYAPPFRAMEWORK_BUFFER_H

#include <cstdint>
#include <Homura/TypeUtils.h>
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

    ~Buffer() {
        mData.Destruct();
    }
};


} // namespace Sexy
#endif // PVZ_SEXYAPPFRAMEWORK_BUFFER_H
