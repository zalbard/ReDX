#include "Primitives.h"

using namespace DirectX;

Sphere::Sphere(const XMFLOAT3& center, const float radius)
    : m_data{center.x, center.y, center.z, radius} {}

XMVECTOR Sphere::center() const {
	return SSE4::XMVectorSetW(XMLoadFloat4A(&m_data), 1.f);
}

XMVECTOR Sphere::radius() const {
    return XMVectorSplatW(XMLoadFloat4A(&m_data));
}

AABox::AABox(const XMFLOAT3& pMin, const XMFLOAT3& pMax)
	: m_pMin{pMin}
	, m_pMax{pMax} {}

AABox::AABox(const XMFLOAT3& pMin, const float (&dims)[3])
	: m_pMin{pMin}
	, m_pMax{pMin.x + dims[0],
			 pMin.y + dims[1],
			 pMin.z + dims[2]} {}

bool Frustum::intersects(const Sphere sphere) const {
    const XMVECTOR sphereCenter    =  sphere.center();
    const XMVECTOR negSphereRadius = -sphere.radius();
	const XMMATRIX tPlanes  = XMLoadFloat4x4A(&m_tPlanes);
	const XMVECTOR farPlane = XMLoadFloat4A(&m_farPlane);
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
        const XMVECTOR distance = SSE4::XMVector4Dot(farPlane, sphereCenter);
        // Test the distance against the (negated) radius of the bounding sphere.
        if (XMVectorGetIntX(XMVectorLess(distance, negSphereRadius))) {
            return false;
        }
    }
    return true;
}
