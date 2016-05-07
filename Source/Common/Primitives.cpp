#include "Primitives.h"

using namespace DirectX;

Sphere::Sphere(const XMFLOAT3& center, const float radius)
    : m_data{center.x, center.y, center.z, radius} {}

XMVECTOR Sphere::center() const {
    return m_data;
}

XMVECTOR Sphere::radius() const {
    return XMVectorSplatW(m_data);
}

bool Frustum::intersects(const Sphere sphere) const {
    const XMVECTOR sphereCenter    =  sphere.center();
    const XMVECTOR negSphereRadius = -sphere.radius();
    // Compute the distances to the left/right/top/bottom frustum planes.
    const XMVECTOR upperPart = tPlanes.r[0] * XMVectorSplatX(sphereCenter)
                             + tPlanes.r[1] * XMVectorSplatY(sphereCenter);
    const XMVECTOR lowerPart = tPlanes.r[2] * XMVectorSplatZ(sphereCenter)
                             + tPlanes.r[3];
    const XMVECTOR distances = upperPart + lowerPart;
    // Test the distances against the (negated) radius of the bounding sphere.
    const XMVECTOR outsideTests = XMVectorLess(distances, negSphereRadius);
    // Check if at least one of the 'outside' tests passed.
    if (XMVector4NotEqualInt(outsideTests, XMVectorFalseInt())) {
        return false;
    } else {
        // Test whether the object is in front of the camera.
        // Our projection matrix is reversed, so we use the far plane.
        const XMVECTOR distance = SSE4::XMVector4Dot(farPlane, XMVectorSetW(sphereCenter, 1.f));
        // Test the distance against the (negated) radius of the bounding sphere.
        if (XMVectorGetIntX(XMVectorLess(distance, negSphereRadius))) {
            return false;
        }
    }
    return true;
}
