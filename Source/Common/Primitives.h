#pragma once

#include <DirectXMathSSE4.h>
#include "Definitions.h"

// Axis-aligned bounding box.
class AABox {
public:
    RULE_OF_ZERO(AABox);
    AABox() = default;
    // Ctor; takes the minimal and the maximal bounding points as input.
    explicit AABox(const DirectX::XMFLOAT3& pMin, const DirectX::XMFLOAT3& pMax);
	// Ctor; takes the minimal bounding point and the dimensions (in X, Y, Z) as input.
    explicit AABox(const DirectX::XMFLOAT3& pMin, const float (&dims)[3]);
    // Constructs the bounding box containing 'count' points.
    explicit AABox(const size_t count, const DirectX::XMFLOAT3* points);
    // Constructs the bounding box containing 'count' indexed points.
    explicit AABox(const size_t count, const uint32_t* indices, const DirectX::XMFLOAT3* points);
    // Returns an empty bounding box.
    static AABox empty();
    // Extends the bounding box if the point is outside.
    void extend(const DirectX::XMFLOAT3& point);
    void extend(DirectX::FXMVECTOR point);
    // Returns the minimal bounding point if passed 0, and the maximal one if passed 1.
    // The W component is set to 1.
    DirectX::XMVECTOR boundingPoint(const size_t index) const;
    // Returns the center of the bounding box.
    DirectX::XMVECTOR center() const;
private:
    DirectX::XMFLOAT3 m_pMin, m_pMax;
};

class Sphere {
public:
    RULE_OF_ZERO(Sphere);
    Sphere() = default;
    // Ctor; takes the center and the radius as input.
    explicit Sphere(const DirectX::XMFLOAT3& center, const float radius);
    explicit Sphere(DirectX::FXMVECTOR center, DirectX::FXMVECTOR radius);
    // Constructs a sphere which fits inside the given bounding box.
    static Sphere inscribed(const AABox& aaBox);
    // Constructs a sphere which contains the given bounding box.
    static Sphere encompassing(const AABox& aaBox);
    // Returns the center of the sphere. The W component is set to 1.
    DirectX::XMVECTOR center() const;
    // Returns the radius of the sphere in every component.
    DirectX::XMVECTOR radius() const;
private:
	DirectX::XMFLOAT4A m_data;
};

// Frustum represented by 5 plane equations with normals pointing inwards.
class Frustum {
public:
    // Returns 'true' if the axis-aligned bounding box overlaps the frustum, 'false' otherwise.
    bool intersects(const AABox& aaBox) const;
    // Returns 'true' if the sphere overlaps the frustum, 'false' otherwise.
    bool intersects(const Sphere& sphere) const;
private:
    DirectX::XMFLOAT4X4A m_tPlanes;          // Transposed left/right/top/bottom plane equations
    DirectX::XMFLOAT4A   m_farPlane;         // Equation of the far plane
    DirectX::XMFLOAT3A   m_sensorCorners[4]; // Points at the corners of the sensor
	/* Accessors */
    friend class PerspectiveCamera;
};
