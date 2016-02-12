#pragma once

#include <DirectXMath.h>

// Computes the square of the value
template <typename T>
static inline T sq(const T v) {
    return v * v;
}

// Computes the inverse square of the value
template <typename T>
static inline float invSq(const T v) {
    return 1.f / (v * v);
}

namespace DirectX {
    // Computes an infinite reversed projection matrix
    // Parameters: the width and the height of the viewport (in pixels),
    // and the vertical field of view (in radians)
    static inline XMMATRIX XM_CALLCONV
    InfRevProjMatLH(const long viewWidth, const long viewHeight, const float fovY) {
        const float cotHalfFovY    = cosf(0.5f * fovY) / sinf(0.5f * fovY);
        const float invAspectRatio = static_cast<float>(viewHeight) / static_cast<float>(viewWidth);
        const float m00 = invAspectRatio * cotHalfFovY;
        const float m11 = cotHalfFovY;
        // A few notes about the structure of the matrix are available at the link below:
        // http://timothylottes.blogspot.com/2014/07/infinite-projection-matrix-notes.html
        return XMMATRIX{
            m00, 0.f, 0.f, 0.f,
            0.f, m11, 0.f, 0.f,
            0.f, 0.f, 0.f, 1.f,
            0.f, 0.f, 1.f, 0.f
        };
    }

    // Computes a rotation matrix using forward (Z) and up (Y) vectors
    static inline XMMATRIX XM_CALLCONV
    RotationMatrixLH(FXMVECTOR forward, FXMVECTOR up) {
        assert(!XMVector3Equal(forward, XMVectorZero()));
        assert(!XMVector3IsInfinite(forward));
        assert(!XMVector3Equal(up, XMVectorZero()));
        assert(!XMVector3IsInfinite(up));
        // Compute the forward vector
        const XMVECTOR R2 = XMVector3Normalize(forward);
        // Compute the right vector
        const XMVECTOR R0 = XMVector3Normalize(XMVector3Cross(up, R2));
        // Compute the up vector
        const XMVECTOR R1 = XMVector3Cross(R2, R0);
        // Compose the matrix
        XMMATRIX M;
        M.r[0] = XMVectorSelect(g_XMIdentityR0.v, R0, g_XMSelect1110.v);
        M.r[1] = XMVectorSelect(g_XMIdentityR1.v, R1, g_XMSelect1110.v);
        M.r[2] = XMVectorSelect(g_XMIdentityR2.v, R2, g_XMSelect1110.v);
        M.r[3] = g_XMIdentityR3.v;
        return M;
    }
}
