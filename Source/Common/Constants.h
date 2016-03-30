#pragma once

#include <dxgiformat.h>
#include "Definitions.h"

// Mathematical constants.
constexpr float M_E             = 2.71828175f;   // e
constexpr float M_LOG2E         = 1.44269502f;   // log2(e)
constexpr float M_LOG10E        = 0.434294492f;  // log10(e)
constexpr float M_LN2           = 0.693147182f;  // ln(2)
constexpr float M_LN10          = 2.30258512f;   // ln(10)
constexpr float M_PI            = 3.14159274f;   // pi
constexpr float M_PI_2          = 1.57079637f;   // pi/2
constexpr float M_PI_4          = 0.785398185f;  // pi/4
constexpr float M_1_PI          = 0.318309873f;  // 1/pi
constexpr float M_2_PI          = 0.636619747f;  // 2/pi
constexpr float M_2_SQRTPI      = 1.12837923f;   // 2/sqrt(pi)
constexpr float M_SQRT2         = 1.41421354f;   // sqrt(2)
constexpr float M_SQRT1_2       = 0.707106769f;  // 1/sqrt(2)
// Horizontal window size (in pixels). Not the same as the rendering resolution!
constexpr long  WND_WIDTH       = 1280;
// Vertical window size (in pixels). Not the same as the rendering resolution!
constexpr long  WND_HEIGHT      = 720;
// Vertical field of view (in radians).
constexpr float VERTICAL_FOV    = M_PI / 3.f;
// Double/triple buffering.
constexpr uint  BUF_CNT         = 3;
// Maximal rendering queue depth (determines the frame latency).
constexpr uint  FRAME_CNT       = 2;
// Vertical blank count after which the synchronization is performed. 
constexpr uint  VSYNC_INTERVAL  = 0;
// Maximal texture count.
constexpr uint  TEX_CNT         = 256;
// Software rendering flag.
constexpr bool  USE_WARP_DEVICE = false;
// Render target view format.
constexpr auto  RTV_FORMAT      = DXGI_FORMAT_R8G8B8A8_UNORM;
// Depth stencil view format.
constexpr auto  DSV_FORMAT      = DXGI_FORMAT_D24_UNORM_S8_UINT;
// Upload buffer size (16 MiB).
constexpr uint  UPLOAD_BUF_SIZE = 16 * 1024 * 1024;
// Camera's speed (in meters/sec).
constexpr float CAM_SPEED       = 500.f;
// Camera's angular speed (in radians/sec).
constexpr float CAM_ANG_SPEED   = 2.0f;
