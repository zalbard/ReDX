#pragma once

#include <DirectXMathSSE4.h>
#include "Definitions.h"

class Sphere {
public:
    RULE_OF_ZERO(Sphere);
    Sphere() = default;
    // Ctor; takes the center and the radius as input.
    explicit Sphere(const DirectX::XMFLOAT3& center, const float radius);
    // Returns the center of the sphere. The W component is set to 1.
    DirectX::XMVECTOR center() const;
    // Returns the radius of the sphere in every component.
    DirectX::XMVECTOR radius() const;
private:
	DirectX::XMFLOAT4A m_data;
};

// Axis-aligned bounding box.
class AABox {
public:
    RULE_OF_ZERO(AABox);
    AABox() = default;
    // Ctor; takes the minimal and the maximal bounding points as input.
    explicit AABox(const DirectX::XMFLOAT3& pMin, const DirectX::XMFLOAT3& pMax);
	// Ctor; takes the minimal bounding point and the dimensions (in X, Y, Z) as input.
    explicit AABox(const DirectX::XMFLOAT3& pMin, const float (&dims)[3]);
private:
    DirectX::XMFLOAT3 m_pMin, m_pMax;
};

// Frustum represented by 5 plane equations with normals pointing inwards.
class Frustum {
public:
    // Returns 'true' if the sphere overlaps the frustum, 'false' otherwise.
    bool intersects(const Sphere& sphere) const;
private:
    DirectX::XMFLOAT4X4A m_tPlanes;  // Transposed equations of the left/right/top/bottom planes
    DirectX::XMFLOAT4A   m_farPlane; // Equation of the far plane
	/* Accessors */
    friend class PerspectiveCamera;
};
