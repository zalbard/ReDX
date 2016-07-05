#include "Math.h"
#include "Primitives.h"

using namespace DirectX;

AABox::AABox(const XMFLOAT3& pMin, const XMFLOAT3& pMax)
    : m_pMin{pMin.x, pMin.y, pMin.z}
    , m_pMax{pMax.x, pMax.y, pMax.z} {}

AABox::AABox(FXMVECTOR pMin, FXMVECTOR pMax) {
    XMStoreFloat3A(&m_pMin, pMin);
    XMStoreFloat3A(&m_pMax, pMax);
}

AABox::AABox(const XMFLOAT3& pMin, const float (&dims)[3])
    : m_pMin{pMin.x, pMin.y, pMin.z}
    , m_pMax{pMin.x + dims[0],
             pMin.y + dims[1],
             pMin.z + dims[2]} {}

AABox::AABox(const size_t count, const XMFLOAT3* points) {
    // Start with an empty box.
    const AABox aaBox = AABox::empty();
    XMVECTOR    pMin  = aaBox.minPoint();
    XMVECTOR    pMax  = aaBox.maxPoint();
    // Gradually extend it until it contains all points.
    for (size_t i = 0; i < count; ++i) {
        const XMVECTOR p = XMLoadFloat3(&points[i]);
        pMin = XMVectorMin(p, pMin);
        pMax = XMVectorMax(p, pMax);
    }
    // Store the computed points.
    XMStoreFloat3A(&m_pMin, pMin);
    XMStoreFloat3A(&m_pMax, pMax);
}

AABox::AABox(const size_t count, const uint32_t* indices, const XMFLOAT3* points) {
    // Start with an empty box.
    const AABox aaBox = AABox::empty();
    XMVECTOR    pMin  = aaBox.minPoint();
    XMVECTOR    pMax  = aaBox.maxPoint();
    // Gradually extend it until it contains all points.
    for (size_t t = 0; t < count; t += 3) {
        for (size_t v = 0; v < 3; ++v) {
            const size_t   i = indices[t + v];
            const XMVECTOR p = XMLoadFloat3(&points[i]);
            pMin = XMVectorMin(p, pMin);
            pMax = XMVectorMax(p, pMax);
        }
    }
    // Store the computed points.
    XMStoreFloat3A(&m_pMin, pMin);
    XMStoreFloat3A(&m_pMax, pMax);
}

void AABox::extend(const XMFLOAT3& point) {
    extend(XMLoadFloat3(&point));
}

void AABox::extend(FXMVECTOR point) {
    const XMVECTOR pMin = XMVectorMin(point, minPoint());
    const XMVECTOR pMax = XMVectorMax(point, maxPoint());
    XMStoreFloat3A(&m_pMin, pMin);
    XMStoreFloat3A(&m_pMax, pMax);
}

AABox AABox::empty() {
    const XMFLOAT3 pMin = { FLT_MAX,  FLT_MAX,  FLT_MAX};
    const XMFLOAT3 pMax = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
    return AABox{pMin, pMax};
}

AABox AABox::computeOverlap(const AABox& aaBox1, const AABox& aaBox2) {
    const XMVECTOR pMin = XMVectorMax(aaBox1.minPoint(), aaBox2.minPoint());
    const XMVECTOR pMax = XMVectorMin(aaBox1.maxPoint(), aaBox2.maxPoint());
    return AABox{pMin, pMax};
}

bool AABox::disjoint(const AABox& aaBox1, const AABox& aaBox2) {
    const AABox overlap = computeOverlap(aaBox1, aaBox2);
    return !XMVector4LessOrEqual(overlap.minPoint(), overlap.maxPoint());
}

DirectX::XMVECTOR AABox::minPoint() const {
    return XMLoadFloat3A(&m_pMin);
}

DirectX::XMVECTOR AABox::maxPoint() const {
    return XMLoadFloat3A(&m_pMax);
}

XMVECTOR AABox::getPoint(const size_t index) const {
    assert(index <= 1);
    return XMLoadFloat3A(&m_pMax + index);
}

XMVECTOR AABox::center() const {
    return 0.5f * (minPoint() + maxPoint());
}

Sphere::Sphere(const XMFLOAT3& center, const float radius)
    : m_data{center.x, center.y, center.z, radius} {}

Sphere::Sphere(FXMVECTOR center, FXMVECTOR radius) {
    const XMVECTOR data = SSE4::XMVectorSetW(center, XMVectorGetX(radius));
    XMStoreFloat4A(&m_data, data);
}

Sphere Sphere::inscribed(const AABox& aaBox) {
    // Compute the center and the radius.
    const XMVECTOR pMin     = aaBox.minPoint();
    const XMVECTOR pMax     = aaBox.maxPoint();
    const XMVECTOR center   = 0.5f * (pMin + pMax);
    const XMVECTOR diagonal = pMax - pMin;
    const XMVECTOR radius   = XMVector4Min(0.5f * diagonal);
    // Create the sphere.
    return Sphere{center, radius};
}

Sphere Sphere::encompassing(const AABox& aaBox) {
    // Compute the center and the radius.
    const XMVECTOR pMin     = aaBox.minPoint();
    const XMVECTOR pMax     = aaBox.maxPoint();
    const XMVECTOR center   = 0.5f * (pMin + pMax);
    const XMVECTOR diagonal = pMax - pMin;
    const XMVECTOR radius   = SSE4::XMVector3Length(0.5f * diagonal);
    // Create the sphere.
    return Sphere{center, radius};
}

DirectX::XMVECTOR Sphere::center() const {
    return SSE4::XMVectorSetW(XMLoadFloat4A(&m_data), 0.f);
}

XMVECTOR Sphere::centerW1() const {
    return SSE4::XMVectorSetW(XMLoadFloat4A(&m_data), 1.f);
}

XMVECTOR Sphere::radius() const {
    return XMVectorSplatW(XMLoadFloat4A(&m_data));
}

bool Frustum::intersects(const AABox& aaBox, float* minDist) const {
    // Test whether the bounding boxes are disjoint.
    const XMVECTOR pMin = aaBox.minPoint();
    const XMVECTOR pMax = aaBox.maxPoint();
    const XMVECTOR oMin = XMVectorMax(pMin, m_bBox.minPoint());
    const XMVECTOR oMax = XMVectorMin(pMax, m_bBox.maxPoint());
    // Validate the axis-aligned box representing the overlap.
    if (!XMVector4LessOrEqual(oMin, oMax)) {
        return false;
    }

    // Test the corners of the box against the left/right/top/bottom frustum planes.
    const XMMATRIX tPlanes = XMLoadFloat4x4A(&m_tPlanes);
    // Get the signs of the X, Y, Z components of all 4 plane normals.
    const XMVECTOR normalComponentSigns[3] = {
        XMLoadFloat4A(&m_tPlanesSgn[0]),
        XMLoadFloat4A(&m_tPlanesSgn[1]),
        XMLoadFloat4A(&m_tPlanesSgn[2])
    };
    // Find 4 points with largest signed distances along plane normals, transposed.
    XMVECTOR tLargestSignDistPoints[3];
    // Iterate over X, Y, Z components.
    XMVECTOR pMinSplatC, pMaxSplatC;
    // Splat the X component of 'pMin' and 'pMax'.
    pMinSplatC = XMVectorSplatX(pMin);
    pMaxSplatC = XMVectorSplatX(pMax);
    // Select the X component of 'pMax' if the sign is positive, of 'pMin' otherwise.
    tLargestSignDistPoints[0] = XMVectorSelect(pMinSplatC, pMaxSplatC, normalComponentSigns[0]);
    // Splat the Y component of 'pMin' and 'pMax'.
    pMinSplatC = XMVectorSplatY(pMin);
    pMaxSplatC = XMVectorSplatY(pMax);
    // Select the Y component of 'pMax' if the sign is positive, of 'pMin' otherwise.
    tLargestSignDistPoints[1] = XMVectorSelect(pMinSplatC, pMaxSplatC, normalComponentSigns[1]);
    // Splat the Z component of 'pMin' and 'pMax'.
    pMinSplatC = XMVectorSplatZ(pMin);
    pMaxSplatC = XMVectorSplatZ(pMax);
    // Select the Z component of 'pMax' if the sign is positive, of 'pMin' otherwise.
    tLargestSignDistPoints[2] = XMVectorSelect(pMinSplatC, pMaxSplatC, normalComponentSigns[2]);
    // Determine whether the 4 points with largest signed distances along plane normals
    // lie inside the positive half-spaces of their respective planes.
    // Compute the signed distances to the left/right/top/bottom frustum planes.
    const XMVECTOR upperPart = tPlanes.r[0] * tLargestSignDistPoints[0]
                             + tPlanes.r[1] * tLargestSignDistPoints[1];
    const XMVECTOR lowerPart = tPlanes.r[2] * tLargestSignDistPoints[2]
                             + tPlanes.r[3];
    const XMVECTOR distances = upperPart + lowerPart;
    // Test the distances for being negative.
    const XMVECTOR outsideTests = XMVectorLess(distances, g_XMZero);
    // Check if at least one of the 'outside' tests passed.
    if (XMVector4NotEqualInt(outsideTests, XMVectorFalseInt())) {
        return false;
    }

    // Test whether the box is in front of the camera.
    // Our projection matrix is reversed, so we use the far plane.
    const XMVECTOR farPlane = XMLoadFloat4A(&m_farPlane);
    // First, we have to find a point with the largest signed distance to the plane.
    const XMVECTOR normalComponentSign  = XMLoadFloat4A(&m_farPlaneSgn);
    XMVECTOR       largestSignDistPoint = XMVectorSelect(pMin, pMax, normalComponentSign);
    largestSignDistPoint = SSE4::XMVectorSetW(largestSignDistPoint, 1.f);
    // Compute the signed distance to the far plane.
    const XMVECTOR distance = SSE4::XMVector4Dot(farPlane, largestSignDistPoint);
    // Test the distance for being negative.
    if (XMVectorGetIntX(XMVectorLess(distance, g_XMZero))) {
        return false;
    } else {
        // Find a point with the smallest signed distance to the plane.
        XMVECTOR smallestSignDistPoint = XMVectorSelect(pMax, pMin, normalComponentSign);
        smallestSignDistPoint = SSE4::XMVectorSetW(smallestSignDistPoint, 1.f);
        // Compute the signed distance to the far plane.
        const XMVECTOR signDist = SSE4::XMVector4Dot(farPlane, smallestSignDistPoint);
        *minDist = XMVectorGetX(signDist);
        return true;
    }
}

bool Frustum::intersects(const Sphere& sphere, float* minDist) const {
    const XMVECTOR sphereCenter    =  sphere.centerW1();
    const XMVECTOR negSphereRadius = -sphere.radius();

    // Test against the left/right/top/bottom planes.
    const XMMATRIX tPlanes = XMLoadFloat4x4A(&m_tPlanes);
    // Compute the signed distances to the left/right/top/bottom frustum planes.
    const XMVECTOR upperPart = tPlanes.r[0] * XMVectorSplatX(sphereCenter)
                             + tPlanes.r[1] * XMVectorSplatY(sphereCenter);
    const XMVECTOR lowerPart = tPlanes.r[2] * XMVectorSplatZ(sphereCenter)
                             + tPlanes.r[3];
    const XMVECTOR distances = upperPart + lowerPart;
    // Test the distances against the (negated) radius of the sphere.
    const XMVECTOR outsideTests = XMVectorLess(distances, negSphereRadius);
    // Check if at least one of the 'outside' tests passed.
    if (XMVector4NotEqualInt(outsideTests, XMVectorFalseInt())) {
        return false;
    }

    // Test whether the sphere is in front of the camera.
    // Our projection matrix is reversed, so we use the far plane.
    const XMVECTOR farPlane = XMLoadFloat4A(&m_farPlane);
    const XMVECTOR distance = SSE4::XMVector4Dot(farPlane, sphereCenter);
    // Test the distance against the (negated) radius of the sphere.
    if (XMVectorGetIntX(XMVectorLess(distance, negSphereRadius))) {
        return false;
    } else {
        // Compute the signed distance to the front of the sphere (as seen by the camera).
        const XMVECTOR signDist = distance + negSphereRadius;
        *minDist = XMVectorGetX(signDist); 
        return true;
    }
}
