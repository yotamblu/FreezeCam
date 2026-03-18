#pragma once

#define FREEZECAM_SHM_NAME    "FreezeCamProSharedMemory"
#define FREEZECAM_EVENT_NAME  "FreezeCamProFrameReady"
#define FREEZECAM_MUTEX_NAME  "FreezeCamProMutex"

#define FREEZECAM_MAX_WIDTH   1920
#define FREEZECAM_MAX_HEIGHT  1080
#define FREEZECAM_MAX_FRAME_SIZE (FREEZECAM_MAX_WIDTH * FREEZECAM_MAX_HEIGHT * 4)

#pragma pack(push, 1)
struct FreezeCamFrameHeader {
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    uint32_t format;       // 0 = RGBA, 1 = RGB24
    uint64_t frameNumber;
    uint32_t isActive;     // 1 = camera is active
};
#pragma pack(pop)

#define FREEZECAM_SHM_SIZE (sizeof(FreezeCamFrameHeader) + FREEZECAM_MAX_FRAME_SIZE)
