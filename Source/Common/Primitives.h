#pragma once

#include <DirectXMathSSE4.h>
#include "Definitions.h"

class Sphere {
public:
    RULE_OF_ZERO(Sphere);
    Sphere() = default;
    // Ctor; takes the center and the radius as input.
    explicit Sphere(const DirectX::XMFLOAT3& center, const float radius);
    // Returns the center of the sphere in the XYZ part; W = radius.
    DirectX::XMVECTOR center() const;
    // Returns the radius of the sphere in every component.
    DirectX::XMVECTOR radius() const;
private:
    DirectX::XMVECTOR m_data;
};

class Frustum {
public:
    // Returns 'true' if the sphere overlaps the frustum, 'false' otherwise.
    bool intersects(const Sphere sphere) const;
private:
    friend class PerspectiveCamera;
    DirectX::XMMATRIX tPlanes;  // Transposed equations of the left/right/top/bottom planes
    DirectX::XMVECTOR farPlane; // Equation of the far plane
};
