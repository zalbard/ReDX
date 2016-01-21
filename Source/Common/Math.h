#pragma once

#include <DirectXMath.h>

// Computes an Infinite Reversed Projection Matrix
// Parameters: the width and the height of the viewport, the horizontal FoV
DirectX::XMMATRIX InfRevProjMatLH(const float viewWidth, const float viewHeight, const float fovY) {
    const float cotHalfFovY    = cosf(0.5f * fovY) / sinf(0.5f * fovY);
    const float invAspectRatio = viewHeight / viewWidth;
    const float m00 = invAspectRatio * cotHalfFovY;
    const float m11 = cotHalfFovY;
    // A few notes about the structure of the matrix are available at the link below:
    // http://timothylottes.blogspot.com/2014/07/infinite-projection-matrix-notes.html
    return DirectX::XMMATRIX{
        m00, 0.f, 0.f, 0.f,
        0.f, m11, 0.f, 0.f,
        0.f, 0.f, 0.f, 1.f,
        0.f, 0.f, 1.f, 0.f
    };
}
