#pragma once

#include <DirectXMathSSE4.h>
#include "Definitions.h"

// Axis-aligned box.
class AABox {
public:
    RULE_OF_ZERO(AABox);
    AABox() = default;
    // Ctor; takes the minimal and the maximal points as input.
    explicit AABox(const DirectX::XMFLOAT3& pMin, const DirectX::XMFLOAT3& pMax);
    explicit AABox(DirectX::FXMVECTOR pMin, DirectX::FXMVECTOR pMax);
	// Ctor; takes the minimal point and the X, Y, Z dimensions as input.
    explicit AABox(const DirectX::XMFLOAT3& pMin, const float (&dims)[3]);
    // Constructs the bounding box for 'count' points.
    explicit AABox(const size_t count, const DirectX::XMFLOAT3* points);
    // Constructs the bounding box for 'count' indexed points.
    explicit AABox(const size_t count, const uint32_t* indices, const DirectX::XMFLOAT3* points);
    // Extends the box in order to contain the point (if the point is outside).
    void extend(const DirectX::XMFLOAT3& point);
    void extend(DirectX::FXMVECTOR point);
    // Returns an empty box.
    static AABox empty();
    // Computes the overlap between two boxes.
    // If there is no overlap, then the resulting box is invalid.
    static AABox computeOverlap(const AABox& aaBox1, const AABox& aaBox2);
    // Returns 'true' if the two boxes do NOT overlap.
    static bool disjoint(const AABox& aaBox1, const AABox& aaBox2);
    // Returns the minimal point of the box. The W component is set to 0.
    DirectX::XMVECTOR minPoint() const;
    // Returns the maximal point of the box. The W component is set to 0.
    DirectX::XMVECTOR maxPoint() const;
    // Returns the minimal point if passed 0, and the maximal point if passed 1.
    // The W component is set to 0.
    DirectX::XMVECTOR getPoint(const size_t index) const;
    // Returns the center of the box. The W component is set to 0.
    DirectX::XMVECTOR center() const;
private:
    DirectX::XMFLOAT3A m_pMin, m_pMax;
};

class Sphere {
public:
    RULE_OF_ZERO(Sphere);
    Sphere() = default;
    // Ctor; takes the center and the radius as input.
    explicit Sphere(const DirectX::XMFLOAT3& center, const float radius);
    explicit Sphere(DirectX::FXMVECTOR center, DirectX::FXMVECTOR radius);
    // Constructs a sphere which tightly fits inside the given box.
    static Sphere inscribed(const AABox& aaBox);
    // Constructs a bounding sphere for the given box.
    static Sphere encompassing(const AABox& aaBox);
    // Returns the center of the sphere. The W component is set to 0.
    DirectX::XMVECTOR center() const;
    // Returns the center of the sphere. The W component is set to 1.
    DirectX::XMVECTOR centerW1() const;
    // Returns the radius of the sphere in every component.
    DirectX::XMVECTOR radius() const;
private:
	DirectX::XMFLOAT4A m_data;
};

// Frustum represented by 5 plane equations with normals pointing inwards.
class Frustum {
public:
    // Returns 'true' if the axis-aligned box overlaps the frustum, 'false' otherwise.
    // In case there is an overlap, it also computes the smallest signed distance 'minDist'.
    bool intersects(const AABox& aaBox, float* minDist) const;
    // Returns 'true' if the sphere overlaps the frustum, 'false' otherwise.
    // In case there is an overlap, it also computes the smallest signed distance 'minDist'.
    bool intersects(const Sphere& sphere, float* minDist) const;
private:
    AABox                m_bBox;        // Bounding box of the frustum
    DirectX::XMFLOAT4X4A m_tPlanes;     // Transposed equations of the left/right/top/bottom planes
    DirectX::XMFLOAT4A   m_farPlane;    // Equation of the far plane
	/* Accessors */
    friend class PerspectiveCamera;
};
