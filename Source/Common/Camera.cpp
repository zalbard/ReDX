#include "Camera.h"
#include "Math.h"

using namespace DirectX;

PerspectiveCamera::PerspectiveCamera(const long width, const long height, const float vFoV,
                                     FXMVECTOR pos, FXMVECTOR dir, FXMVECTOR up)
    : m_position(pos)
    , m_worldUp(up)
    , m_orientQuat(XMQuaternionRotationMatrix(RotationMatrixLH(dir, up)))
    , m_projMat{InfRevProjMatLH(width, height, vFoV)} {
        // We are using an infinite reversed projection matrix, so
        // the sensor lies on the far plane, at the distance of 1.
        const float sensorHeight = 2.f * tanf(0.5f * vFoV);
        const float aspectRatio  = static_cast<float>(width) / static_cast<float>(height);
        const float sensorWidth  = sensorHeight * aspectRatio;
        m_sensorDims = {sensorWidth, sensorHeight};
}

XMVECTOR PerspectiveCamera::position() const {
    return m_position;
}

XMMATRIX PerspectiveCamera::projectionMatrix() const {
    return m_projMat;
}

XMVECTOR PerspectiveCamera::orientation() const {
    return m_orientQuat;
}

XMFLOAT2 PerspectiveCamera::sensorDimensions() const {
    return m_sensorDims;
}

XMVECTOR PerspectiveCamera::computeForwardDir() const {
    const XMMATRIX orientMat = XMMatrixRotationQuaternion(m_orientQuat);
    return orientMat.r[2];
}

XMMATRIX PerspectiveCamera::computeViewProjMatrix(XMMATRIX* viewMat) const {
    const XMMATRIX v = computeViewMatrix();
    if (viewMat) {
        *viewMat = v;
    }
    return v * m_projMat;
}

XMMATRIX PerspectiveCamera::computeViewMatrix() const {
    // Invert the rotation and translation for the view matrix.
    constexpr XMVECTOR scale       = {1.f, 1.f, 1.f};
    const     XMVECTOR invOrient   = XMQuaternionInverse(m_orientQuat);
    const     XMVECTOR translation = -m_position;
    return XMMatrixAffineTransformation(scale, m_position, invOrient, translation);
}

// See "Fast Extraction of Viewing Frustum Planes from the WorldView-Projection Matrix"
// by Gil Gribb and Klaus Hartmann.
Frustum PerspectiveCamera::computeViewFrustum() const {
    Frustum  frustum;
    XMMATRIX frustumPlanes;
    const XMMATRIX tViewProj = XMMatrixTranspose(computeViewProjMatrix());
    // Left plane.
    frustumPlanes.r[0] = tViewProj.r[3] + tViewProj.r[0];
    // Right plane.
    frustumPlanes.r[1] = tViewProj.r[3] - tViewProj.r[0];
    // Top plane.
    frustumPlanes.r[2] = tViewProj.r[3] - tViewProj.r[1];
    // Bottom plane.
    frustumPlanes.r[3] = tViewProj.r[3] + tViewProj.r[1];
    // Far plane.
    frustum.farPlane   = tViewProj.r[3] - tViewProj.r[2];
    // Compute the inverse magnitudes.
    frustum.tPlanes    = XMMatrixTranspose(frustumPlanes);
    const XMVECTOR magsSq  = sq(frustum.tPlanes.r[0])
                           + sq(frustum.tPlanes.r[1])
                           + sq(frustum.tPlanes.r[2]);
    const XMVECTOR invMags = XMVectorReciprocalSqrt(magsSq);
    // Normalize the plane equations.
    frustumPlanes.r[0] *= XMVectorSplatX(invMags);
    frustumPlanes.r[1] *= XMVectorSplatY(invMags);
    frustumPlanes.r[2] *= XMVectorSplatZ(invMags);
    frustumPlanes.r[3] *= XMVectorSplatW(invMags);
    frustum.farPlane    = XMPlaneNormalize(frustum.farPlane);
    // Transpose the normalized plane equations.
    frustum.tPlanes = XMMatrixTranspose(frustumPlanes);
    return frustum;
}

void PerspectiveCamera::moveBack(const float dist) {
    moveForward(-dist);
}

void PerspectiveCamera::moveForward(const float dist) {
    const XMMATRIX orientMat = XMMatrixRotationQuaternion(m_orientQuat);
    const XMVECTOR forward   = orientMat.r[2];
    m_position += forward * dist;
}

void PerspectiveCamera::rotateLeft(const float angle) {
    rotateRight(-angle);
}

void PerspectiveCamera::rotateRight(const float angle) {
    // XMQuaternionRotationNormal performs rotations clockwise.
    const XMVECTOR rotQuat = XMQuaternionRotationNormal(m_worldUp, angle);
    m_orientQuat = XMQuaternionMultiply(m_orientQuat, rotQuat);
}

void PerspectiveCamera::rotateUpwards(const float angle) {
    rotateDownwards(-angle);
}

void PerspectiveCamera::rotateDownwards(const float angle) {
    const XMMATRIX orientMat = XMMatrixRotationQuaternion(m_orientQuat);
    const XMVECTOR right     = orientMat.r[0];
    // XMQuaternionRotationNormal performs rotations clockwise.
    const XMVECTOR rotQuat   = XMQuaternionRotationNormal(right, angle);
    m_orientQuat = XMQuaternionMultiply(m_orientQuat, rotQuat);
}

void PerspectiveCamera::rotateAndMoveForward(const float pitch, const float yaw, const float dist) {
    const XMMATRIX orientMat = XMMatrixRotationQuaternion(m_orientQuat);
    const XMVECTOR right     = orientMat.r[0];
    const XMVECTOR forward   = orientMat.r[2];
    // XMQuaternionRotationNormal performs rotations clockwise.
    const XMVECTOR pitchQuat = XMQuaternionRotationNormal(right, pitch);
    const XMVECTOR yawQuat   = XMQuaternionRotationNormal(m_worldUp, yaw);
    m_orientQuat = XMQuaternionMultiply(XMQuaternionMultiply(m_orientQuat, pitchQuat), yawQuat);
    // Translate along the forward direction.
    m_position  += forward * dist;
}
