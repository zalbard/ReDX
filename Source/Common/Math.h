#pragma once

#include <DirectXMathSSE4.h>

// Computes the square of the value.
template <typename T>
static inline auto sq(const T v)
-> T {
    return v * v;
}

// Computes the reciprocal of the value.
template <typename T>
static inline auto rcp(const T v)
-> float {
    return 1.f / v;
}

namespace DirectX {
    // Constructs an infinite reversed projection matrix.
    // The distance to the near plane is infinite, the distance to the far plane is 1.
    // Parameters: the width and the height of the viewport (in pixels),
    // and the vertical field of view 'vFoV' (in radians).
    static inline auto InfRevProjMatLH(const long width, const long height, const float vFoV)
    -> XMMATRIX {
        const float cotHalfFovY    = cosf(0.5f * vFoV) / sinf(0.5f * vFoV);
        const float invAspectRatio = static_cast<float>(height) / static_cast<float>(width);
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

    // Constructs a rotation matrix using forward (Z) and up (Y) vectors.
    static inline auto RotationMatrixLH(FXMVECTOR forward, FXMVECTOR up)
    -> XMMATRIX {
        assert(!XMVector3Equal(forward, XMVectorZero()));
        assert(!XMVector3IsInfinite(forward));
        assert(!XMVector3Equal(up, XMVectorZero()));
        assert(!XMVector3IsInfinite(up));
        // Compute the forward vector.
        const XMVECTOR R2 = SSE4::XMVector3Normalize(forward);
        // Compute the right vector.
        const XMVECTOR R0 = SSE4::XMVector3Normalize(XMVector3Cross(up, R2));
        // Compute the up vector.
        const XMVECTOR R1 = XMVector3Cross(R2, R0);
        // Compose the matrix.
        XMMATRIX M;
        M.r[0] = XMVectorSelect(g_XMIdentityR0.v, R0, g_XMSelect1110.v);
        M.r[1] = XMVectorSelect(g_XMIdentityR1.v, R1, g_XMSelect1110.v);
        M.r[2] = XMVectorSelect(g_XMIdentityR2.v, R2, g_XMSelect1110.v);
        M.r[3] = g_XMIdentityR3.v;
        return M;
    }
}
