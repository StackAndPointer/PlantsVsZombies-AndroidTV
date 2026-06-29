//
// Created by Admin on 2026/6/29.
//

#ifndef PVZ_SEXYAPPFRAMEWORK_THREAD_H
#define PVZ_SEXYAPPFRAMEWORK_THREAD_H


namespace Sexy {

class Thread {
public:
    unsigned int mThread; // +0x00，目标程序中的 pthread_t
    bool mValid;          // +0x04
    bool mPadding[3];
};
} // namespace Sexy

#endif // PVZ_SEXYAPPFRAMEWORK_THREAD_H
