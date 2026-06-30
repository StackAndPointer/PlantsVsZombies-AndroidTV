//
// Created by Admin on 2026/6/30.
//

#ifndef PVZ_SEXYAPPFRAMEWORK_EVENT_H
#define PVZ_SEXYAPPFRAMEWORK_EVENT_H

namespace Sexy {

struct Touch {
    Point location;
    // Point previousLocation;
    // int tapCount;
    double timestamp;
};

enum SexyEventType {
    EVENT_TYPE_KEYDOWN = 1,
    EVENT_TYPE_KEYUP = 2,
    EVENT_TYPE_MOUSEDOWN = 3,
    EVENT_TYPE_MOUSEUP = 4,
    EVENT_TYPE_MOUSEWHEELFORWARD = 5,
    EVENT_TYPE_MOUSEWHEELBACK = 6,
    EVENT_TYPE_MOUSEMOVE = 7,
    EVENT_TYPE_DEVICECONNECTING = 11,
    EVENT_TYPE_DEVICEDISCONNECTED = 12,
    EVENT_TYPE_DEVICECONNECTED = 13,
    EVENT_TYPE_TOUCH = 16,
    EVENT_TYPE_AXISMOVED = 19,
    EVENT_TYPE_MOUSEEXIT = 21,
    EVENT_TYPE_MOUSEWHEEL2D = 22
};

enum SexyEventFlags {
    EVENT_FLAG_ABSOLUTE_POSITION = 0x001,
    EVENT_FLAG_RELATIVE_POSITION = 0x002,
    EVENT_FLAG_MOUSE_BUTTON = 0x004,
    EVENT_FLAG_KEY_CODE = 0x008,
    EVENT_FLAG_CHARACTER = 0x010,
    EVENT_FLAG_SCALED_POSITION = 0x020,
    EVENT_FLAG_TIMESTAMP = 0x040,
    EVENT_FLAG_KEY_REPEAT = 0x080,
    EVENT_FLAG_UNICODE = 0x200
};
struct Event {
    SexyEventType mType; // 0x00
    unsigned int mFlags;
    int mDriverId;
    int mDeviceId;
    unsigned int mTimestamp;
    int mData[8];
};
} // namespace Sexy


#endif // PVZ_SEXYAPPFRAMEWORK_EVENT_H
