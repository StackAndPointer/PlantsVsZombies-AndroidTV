//
// Created by Admin on 2026/6/26.
//

#ifndef PVZ_SEXYAPPFRAMEWORK_BUFFER_H
#define PVZ_SEXYAPPFRAMEWORK_BUFFER_H

#include <string>

struct ByteVector {
    unsigned char *mBegin;       // +0x00 数据起始地址
    unsigned char *mEnd;         // +0x04 已使用数据的末尾
    unsigned char *mCapacityEnd; // +0x08 已分配内存的末尾
};

namespace Sexy {

class Buffer {
public:
    void *mVtable;
    ByteVector mData;
    int mDataBitSize;
    mutable int mReadBitPos;
    mutable int mWriteBitPos;
};
} // namespace Sexy
#endif // PVZ_SEXYAPPFRAMEWORK_BUFFER_H
