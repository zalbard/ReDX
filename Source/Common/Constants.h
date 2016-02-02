#pragma once

#include <dxgiformat.h>
#include "Definitions.h"

// Horizontal window size (in pixels). Not the same as the rendering resolution!
constexpr long        WND_WIDTH       = 1280;
// Vertical window size (in pixels). Not the same as the rendering resolution!
constexpr long        WND_HEIGHT      = 720;
// Double buffering: present the front, render to the back
constexpr uint        BUFFER_COUNT    = 2;
// Software rendering flag
constexpr bool        USE_WARP_DEVICE = false;
// Render target view format specified for the swap chain
constexpr DXGI_FORMAT RTV_FORMAT      = DXGI_FORMAT_R8G8B8A8_UNORM;
// Render target view format specified for the pipeline state
constexpr DXGI_FORMAT RTV_FORMAT_SRGB = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
// Depth stencil view format
constexpr DXGI_FORMAT DSV_FORMAT      = DXGI_FORMAT_D24_UNORM_S8_UINT;
// Upload buffer size (64 MB)
constexpr uint        UPLOAD_BUF_SIZE = 64 * 1024 * 1024;
