#pragma once

#include <dxgiformat.h>
#include "Definitions.h"

// Horizontal window size (in pixels). Not the same as the rendering resolution!
constexpr long        WND_WIDTH       = 1280;
// Vertical window size (in pixels). Not the same as the rendering resolution!
constexpr long        WND_HEIGHT      = 720;
// Vertical field of view (in radians)
constexpr float       VERTICAL_FOV    = 3.141592654f / 3.f;
// Double/triple buffering
constexpr uint        BUF_CNT         = 3;
// Maximal rendering queue depth (determines the frame latency)
constexpr uint        FRAME_CNT       = 2;
// Vertical blank count after which the synchronization is performed 
constexpr uint        VSYNC_INTERVAL  = 0;
// Software rendering flag
constexpr bool        USE_WARP_DEVICE = false;
// Render target view format
constexpr DXGI_FORMAT RTV_FORMAT      = DXGI_FORMAT_R8G8B8A8_UNORM;
// Depth stencil view format
constexpr DXGI_FORMAT DSV_FORMAT      = DXGI_FORMAT_D24_UNORM_S8_UINT;
// Upload buffer size (8 MB)
constexpr uint        UPLOAD_BUF_SIZE = 8 * 1024 * 1024;
// Camera's speed (in meters/sec)
constexpr float       CAM_SPEED       = 500.f;
// Camera's angular speed (in radians/sec)
constexpr float       CAM_ANG_SPEED   = 2.0f;
