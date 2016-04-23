#pragma once

#include <dxgitype.h>
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
// Opaque black RGBA color.
constexpr float FLOAT4_BLACK[]  = {0.f, 0.f, 0.f, 0.f};
// Horizontal rendering resolution.
constexpr long  RES_X           = 1280;
// Vertical rendering resolution.
constexpr long  RES_Y           = 720;
// Vertical field of view (in radians).
constexpr float VERTICAL_FOV    = M_PI / 3.f;
// Maximal texture count.
constexpr uint  TEX_CNT         = 256;
// Maximal number of materials.
constexpr uint  MAT_CNT         = 32;
// Double/triple buffering.
constexpr uint  BUF_CNT         = 3;
// Maximal rendering queue depth (determines the frame latency).
constexpr uint  FRAME_CNT       = 2;
// Total number of render target views. Depends on the definition of 'FrameResource'.
constexpr uint  RTV_CNT         = BUF_CNT + 4 * FRAME_CNT;
// Vertical blank count after which the synchronization is performed.
constexpr uint  VSYNC_INTERVAL  = 0;
// Software rendering flag.
constexpr bool  USE_WARP_DEVICE = false;
// Normal vector texture format.
constexpr auto  FORMAT_NORMAL   = DXGI_FORMAT_R16G16_SNORM;
// UV coordinate texture format.
constexpr auto  FORMAT_UVCOORD  = DXGI_FORMAT_R16G16_UNORM;
// UV gradient texture format.
constexpr auto  FORMAT_UVGRAD   = DXGI_FORMAT_R16G16B16A16_SNORM;
// Material ID texture format.
constexpr auto  FORMAT_MAT_ID   = DXGI_FORMAT_R16_UINT;
// Primary render target view format.
constexpr auto  FORMAT_RTV      = DXGI_FORMAT_R8G8B8A8_UNORM;
// Primary depth stencil view format.
constexpr auto  FORMAT_DSV      = DXGI_FORMAT_D24_UNORM_S8_UINT;
// Upload buffer size (32 MiB).
constexpr uint  UPLOAD_BUF_SIZE = 32 * 1024 * 1024;
// Camera's speed (in meters/sec).
constexpr float CAM_SPEED       = 500.f;
// Camera's angular speed (in radians/sec).
constexpr float CAM_ANG_SPEED   = 2.0f;
// Default, single sample mode (no multi-sampling).
constexpr auto  SAMPLE_DEFAULT  = DXGI_SAMPLE_DESC{1, 0};
