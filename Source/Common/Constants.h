#pragma once

// Typedefs for dxgitype.h.
using BOOL = int;
using BYTE = unsigned char;
using UINT = unsigned int;

#include <dxgitype.h>

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
// Transparent black RGBA color.
constexpr float FLOAT4_BLACK[] = {0.f, 0.f, 0.f, 0.f};
// Horizontal rendering resolution.
constexpr auto RES_X           = 1280;
// Vertical rendering resolution.
constexpr auto RES_Y           = 720;
// Vertical field of view (in radians).
constexpr auto VERTICAL_FOV    = M_PI / 3.f;
// Maximal texture count.
constexpr auto TEX_CNT         = 256;
// Maximal number of materials.
constexpr auto MAT_CNT         = 32;
// Double/triple buffering.
constexpr auto BUF_CNT         = 3;
// Total number of render targets the G-buffer is composed of.
constexpr auto G_BUFFER_SIZE   = 4;
// Total number of render target views.
constexpr auto RTV_CNT         = BUF_CNT + G_BUFFER_SIZE;
// Maximal rendering queue depth (determines the frame latency).
constexpr auto FRAME_CNT       = 2;
// Vertical blank count after which the synchronization is performed.
constexpr auto VSYNC_INTERVAL  = 0;
// Software rendering flag.
constexpr bool USE_WARP_DEVICE = false;
// Normal texture format.
constexpr auto FORMAT_NORMAL   = DXGI_FORMAT_R16G16_SNORM;
// UV coordinate texture format.
constexpr auto FORMAT_UVCOORD  = DXGI_FORMAT_R16G16_UNORM;
// UV gradient texture format.
constexpr auto FORMAT_UVGRAD   = DXGI_FORMAT_R16G16B16A16_SNORM;
// Material ID texture format.
constexpr auto FORMAT_MAT_ID   = DXGI_FORMAT_R16_UINT;
// Swap chain buffer format.
constexpr auto FORMAT_SC       = DXGI_FORMAT_R8G8B8A8_UNORM;
// Primary render target view format.
constexpr auto FORMAT_RTV      = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
// Primary depth stencil view format.
constexpr auto FORMAT_DSV      = DXGI_FORMAT_D24_UNORM_S8_UINT;
// Upload buffer size (32 MiB).
constexpr auto UPLOAD_BUF_SIZE = 32 * 1024 * 1024;
// Camera's speed (in meters/sec).
constexpr auto CAM_SPEED       = 500.f;
// Camera's angular speed (in radians/sec).
constexpr auto CAM_ANG_SPEED   = 2.0f;
// Default sampling mode (no multi-sampling).
constexpr auto SINGLE_SAMPLE   = DXGI_SAMPLE_DESC{1, 0};
