#include "Camera.h"
#include "Math.h"

using namespace DirectX;

PerspectiveCamera::PerspectiveCamera(const long width, const long height, const float vFoV,
                                     FXMVECTOR pos, FXMVECTOR dir, FXMVECTOR up)
    : m_resolution{width, height} {
        setPosition(pos);
        setUpVector(up);
        setOrientation(XMQuaternionRotationMatrix(RotationMatrixLH(dir, up)));
        // Compute the infinite reversed projection matrix.
        XMStoreFloat4x4A(&m_projMat, InfRevProjMatLH(width, height, vFoV));
        // The sensor lies on the far plane, at the distance of 1.
        const float sensorHeight = 2.f * tanf(0.5f * vFoV);
        const float aspectRatio  = static_cast<float>(width) / static_cast<float>(height);
        const float sensorWidth  = sensorHeight * aspectRatio;
        m_sensorDims = {sensorWidth, sensorHeight};
}

XMVECTOR PerspectiveCamera::position() const {
    return XMLoadFloat3A(&m_position);
}

void PerspectiveCamera::setPosition(FXMVECTOR pos) {
    XMStoreFloat3A(&m_position, pos);
}

XMVECTOR PerspectiveCamera::upVector() const {
    return XMLoadFloat3A(&m_up);
}

void PerspectiveCamera::setUpVector(FXMVECTOR up) {
    XMStoreFloat3A(&m_up, up);
}

XMMATRIX PerspectiveCamera::orientationMatrix() const {
    return XMMatrixRotationQuaternion(orientationQuaternion());
}

XMVECTOR PerspectiveCamera::orientationQuaternion() const {
    return XMLoadFloat4A(&m_orientQuat);
}

void PerspectiveCamera::setOrientation(DirectX::FXMVECTOR orientQuat) {
    XMStoreFloat4A(&m_orientQuat, orientQuat);
}

XMMATRIX PerspectiveCamera::projectionMatrix() const {
    return XMLoadFloat4x4A(&m_projMat);
}

XMVECTOR PerspectiveCamera::computeForwardDir() const {
    const XMMATRIX orientMat = orientationMatrix();
    return orientMat.r[2];
}

XMMATRIX PerspectiveCamera::computeViewMatrix() const {
    const XMVECTOR scale       = g_XMOne;
    const XMVECTOR origin      = position();
    const XMVECTOR translation = -origin;
    const XMVECTOR invOrient   = XMQuaternionInverse(orientationQuaternion());
    return XMMatrixAffineTransformation(scale, origin, invOrient, translation);
}

XMMATRIX PerspectiveCamera::computeViewProjMatrix(XMMATRIX* viewMat) const {
    const XMMATRIX viewMatrix = computeViewMatrix();
    if (viewMat) {
        *viewMat = viewMatrix;
    }
    return viewMatrix * projectionMatrix();
}

XMMATRIX PerspectiveCamera::computeRasterToViewDirMatrix() const {
    // Compose the view space version first.
    // Dir = -(X, Y, Z), s.t. X = (x / resX - 0.5) * width, Y = (0.5 - y / resY) * height, Z = 1.
    // -X = x * (-width / resX) + 0.5 * width  = x * m00 + m20.
    // -Y = y * (height / resY) - 0.5 * height = y * m11 + m21.
    const float m00 = m_sensorDims.x / -m_resolution.x;
    const float m20 = m_sensorDims.x * 0.5f;
    const float m11 = m_sensorDims.y / m_resolution.y;
    const float m21 = m_sensorDims.y * -0.5f;
    // Compose the matrix.
    const XMMATRIX viewSpaceRasterTransform = {m00, 0.f,  0.f, 0.f,
                                               0.f, m11,  0.f, 0.f,
                                               m20, m21, -1.f, 0.f,
                                               0.f, 0.f,  0.f, 1.f};
    // Concatenate the matrix with the transformation from view to world space.
    return viewSpaceRasterTransform * orientationMatrix();
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
    const XMMATRIX orientMat   = orientationMatrix();
    const XMVECTOR forward     = orientMat.r[2];
    const XMVECTOR newPosition = position() + forward * dist;
    setPosition(newPosition);
}

void PerspectiveCamera::rotateLeft(const float angle) {
    rotateRight(-angle);
}

void PerspectiveCamera::rotateRight(const float angle) {
    const XMVECTOR rotQuat   = XMQuaternionRotationNormal(upVector(), angle);
    const XMVECTOR newOrient = XMQuaternionMultiply(orientationQuaternion(), rotQuat);
    setOrientation(newOrient);
}

void PerspectiveCamera::rotateUpwards(const float angle) {
    rotateDownwards(-angle);
}

void PerspectiveCamera::rotateDownwards(const float angle) {
    const XMMATRIX orientMat = orientationMatrix();
    const XMVECTOR right     = orientMat.r[0];
    const XMVECTOR rotQuat   = XMQuaternionRotationNormal(right, angle);
    const XMVECTOR newOrient = XMQuaternionMultiply(orientationQuaternion(), rotQuat);
    setOrientation(newOrient);
}

void PerspectiveCamera::rotateAndMoveForward(const float pitch, const float yaw, const float dist) {
    const XMMATRIX orientMat = orientationMatrix();
    const XMVECTOR right     = orientMat.r[0];
    const XMVECTOR forward   = orientMat.r[2];
    const XMVECTOR pitchQuat = XMQuaternionRotationNormal(right, pitch);
    const XMVECTOR yawQuat   = XMQuaternionRotationNormal(upVector(), yaw);
    const XMVECTOR newOrient = XMQuaternionMultiply(XMQuaternionMultiply(orientationQuaternion(),
                                                                         pitchQuat), yawQuat);
    setOrientation(newOrient);
    const XMVECTOR newPosition = position() + forward * dist;
    setPosition(newPosition);
}
