#pragma once

#include <cmath>
#include <DirectXMathSSE4.h>
#include "Constants.h"
#include "Definitions.h"

// Computes the square of the value.
template <typename T>
constexpr auto sq(const T v)
-> T {
    return v * v;
}

// Computes the FP32 reciprocal of the value.
template <typename T>
constexpr auto rcp(const T v)
-> float {
    return 1.f / v;
}

// Returns -1 for negative values, and 1 otherwise.
// Correctly handles negative zero.
template <typename T>
constexpr auto sign(const T v)
-> T {
    return std::signbit(v) ? T{-1} : T{1};
}

// Returns 'true' if 'v' is a power of 2.
template <typename T>
constexpr auto isPow2(const T v)
-> bool {
    return 0 == (v & (v - 1));
}

// Aligns the integer number to the next multiple of alignment.
template <size_t alignment>
static inline auto align(const size_t number)
-> size_t {
    // Make sure that the alignment is non-zero, and is a power of 2.
    static_assert((alignment != 0) && isPow2(alignment), "Invalid alignment.");
    return (number + (alignment - 1)) & ~(alignment - 1);
}

// Aligns the memory address to the next multiple of alignment.
template <size_t alignment>
static inline auto align(void* address)
-> byte_t* {
    const size_t number = reinterpret_cast<size_t>(address);
    return reinterpret_cast<byte_t*>(align<alignment>(number));
}

namespace DirectX {
    // Returns the value of the largest component of 'v' in all 4 components of the result.
    static inline auto XMVector4Max(FXMVECTOR v)
    -> XMVECTOR {
        XMVECTOR hMax = XMVectorMax(v, XMVectorSwizzle(v, 1, 2, 3, 0));
        hMax = XMVectorMax(hMax, XMVectorSwizzle(hMax, 2, 3, 0, 1));
        return hMax;
    }

    // Returns the index of the largest component of 'v'.
    static inline auto XMVector3MaxComponent(FXMVECTOR v)
    -> size_t {
        XMFLOAT3A f;
        XMStoreFloat3A(&f, v);
        size_t id;
        if (f.y > f.x) {
            id = f.z > f.y ? 2 : 1;
        } else {
            id = f.z > f.x ? 2 : 0;
        }
        return id;
    }

    // Returns the index of the largest component of 'v'.
    static inline auto XMVector4MaxComponent(FXMVECTOR v)
    -> size_t {
        XMFLOAT4A f;
        XMStoreFloat4A(&f, v);
        size_t id;
        if (f.y > f.x) {
            id = f.z > f.y ? 2 : 1;
        } else {
            id = f.z > f.x ? 2 : 0;
        }
        id = f.w > (&f.x)[id] ? 3 : id;
        return id;
    }

    // Constructs an infinite reversed projection matrix.
    // The distance to the near plane is infinite, the distance to the far plane is 1.
    // Parameters: the width and the height of the viewport (in pixels),
    // and the vertical field of view 'vFoV' (in radians).
    static inline auto InfRevProjMatLH(const float width, const float height, const float vFoV)
    -> XMMATRIX {
        const float cotHalfFovY = cosf(0.5f * vFoV) / sinf(0.5f * vFoV);
        const float m00 = cotHalfFovY * (height / width);
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
        assert(!XMVector3Equal(forward, g_XMZero));
        assert(!XMVector3IsInfinite(forward));
        assert(!XMVector3Equal(up, g_XMZero));
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

    // Computes the tangent frame of the triangle aligned with the U and V axes.
    // Input: triangle vertex positions and UV coordinates.
    // Degenerate inputs will produce incorrect results! No checks are performed.
    // Output: 3x3 matrix with rows containing the tangent, the bitangent and the normal.
    // The tangent frame is also not necessarily orthogonal. The normal may appear flipped.
    // See "Computing Tangent Space Basis Vectors for an Arbitrary Mesh" by Eric Lengyel.
    static inline auto ComputeTangentFrame(FXMVECTOR pts[3], HXMVECTOR uvs[3])
    -> XMMATRIX {
        const XMVECTOR e1  = pts[1] - pts[0];
        const XMVECTOR e2  = pts[2] - pts[0];
        const XMVECTOR st1 = uvs[1] - uvs[0];
        const XMVECTOR st2 = uvs[2] - uvs[0];
        const float s1 = XMVectorGetX(st1);
        const float t1 = XMVectorGetY(st1);
        const float s2 = XMVectorGetX(st2);
        const float t2 = XMVectorGetY(st2);
        const XMVECTOR vt      = { t2, -t1};
        const XMVECTOR vs      = {-s2,  s1};
        const XMMATRIX stMat   = {vt, vs, g_XMZero, g_XMZero};
        const XMMATRIX edgeMat = {e1, e2, g_XMZero, g_XMZero};
        const XMMATRIX tanMat  = stMat * edgeMat;
        // We are only interested in the sign of the common factor
        // as we will perform normalization anyway.
        const float factor = sign(s1 * t2 - s2 * t1);
        XMVECTOR tangent   = factor * tanMat.r[0];
        XMVECTOR bitangent = factor * tanMat.r[1];
        XMVECTOR normal    = XMVector3Cross(tangent, bitangent);
        // Validate the resulting normal.
        assert(XMVector4NotEqual(normal, g_XMZero));
        // Normalize and return the tangent frame.
        tangent   = SSE4::XMVector3Normalize(tangent);
        bitangent = SSE4::XMVector3Normalize(bitangent);
        normal    = SSE4::XMVector3Normalize(normal);
        return {tangent, bitangent, normal, g_XMIdentityR3};
    }

    // Input/output: 3x3 matrix with rows containing the tangent, the bitangent and the normal.
    static inline auto OrthogonalizeTangentFrame(FXMMATRIX frame)
    -> XMMATRIX {
        XMVECTOR tangent   = frame.r[0];
        XMVECTOR bitangent = frame.r[1];
        // Check whether the input frame is already orthogonal.
        constexpr float    eps   = 0.0001f;
        constexpr XMVECTOR vEps  = {eps, eps, eps, eps};
        const     XMVECTOR vTrue = XMVectorTrueInt();
        const     XMVECTOR cosA  = SSE4::XMVector3Dot(tangent, bitangent);
        if (XMVectorGetX(XMVectorNearEqual(cosA, g_XMZero, vEps))) {
            return frame;
        }
        // Compute the median between the tangent and the bitangent.
        const XMVECTOR median   = SSE4::XMVector3Normalize(tangent + bitangent);
        // Complete the reference frame formed by the normal and the median.
        // Median is X, normal is Y, covector is Z.
        const XMVECTOR normal   = frame.r[2];
        const XMVECTOR covector = XMVector3Cross(median, normal);
        // The new tangent/bitangent should have the same median,
        // but both should form an angle of pi/4 with the median.
        const float cosPi4 = cos(M_PI_4);
        const float sinPi4 = sin(M_PI_4);
        // Use the spherical coordinates to generate the new tangent.
        tangent   = cosPi4 * median + sinPi4 * covector;
        bitangent = cosPi4 * median - sinPi4 * covector;
        // Validation to make sure we do not mix up the tangent and the bitangent.
        assert(XMVectorGetX(SSE4::XMVector3Dot(frame.r[0],   tangent)) > cosPi4 &&
               XMVectorGetX(SSE4::XMVector3Dot(frame.r[1], bitangent)) > cosPi4 &&
               XMVector4EqualInt(vTrue, XMVectorNearEqual(XMVector3Cross(tangent, bitangent),
                                                          normal, vEps)));
        return {tangent, bitangent, normal, g_XMIdentityR3};
    }
}
