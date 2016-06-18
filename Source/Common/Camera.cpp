#include "Camera.h"
#include "Math.h"

using namespace DirectX;

PerspectiveCamera::PerspectiveCamera(const float width, const float height, const float vFoV,
                                     FXMVECTOR pos, FXMVECTOR dir, FXMVECTOR up)
    : m_resolution{width, height} {
    setPosition(pos);
    setUpVector(up);
    setOrientation(XMQuaternionRotationMatrix(RotationMatrixLH(dir, up)));
    // Compute the infinite reversed projection matrix.
    XMStoreFloat4x4A(&m_projMat, InfRevProjMatLH(width, height, vFoV));
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
    // Dir = -(X, Y, Z), s.t. Z = 1,
    //  X = (2x / resX - 1) * tan(vFoV / 2) * ar = (2x / resX - 1) / p00,
    //  Y = (1 - 2y / resY) * tan(vFoV / 2) = (1 - 2y / resY) / p11.
    // -X = x * (-2 / p00 / resX) + ( 1 / p00) = x * m00 + m20
    // -Y = y * ( 2 / p11 / resY) + (-1 / p11) = y * m11 + m21
    // Additional derivation details are available in the chapter 17.1 of
    // "Introduction to 3D Game Programming with DirectX 12" by Frank Luna.
    const float p00 = m_projMat.m[0][0];
    const float p11 = m_projMat.m[1][1];
    const float m20 =  1.f / p00;
    const float m21 = -1.f / p11;
    const float m00 = -2.f * m20 / m_resolution.x;
    const float m11 = -2.f * m21 / m_resolution.y;
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
    XMVECTOR farPlane  = tViewProj.r[3] - tViewProj.r[2];
    // Compute the inverse magnitudes.
    XMMATRIX tPlanes   = XMMatrixTranspose(frustumPlanes);
    const XMVECTOR magsSq  = sq(tPlanes.r[0]) + sq(tPlanes.r[1]) + sq(tPlanes.r[2]);
    const XMVECTOR invMags = XMVectorReciprocalSqrt(magsSq);
    // Normalize the plane equations.
    frustumPlanes.r[0] *= XMVectorSplatX(invMags);
    frustumPlanes.r[1] *= XMVectorSplatY(invMags);
    frustumPlanes.r[2] *= XMVectorSplatZ(invMags);
    frustumPlanes.r[3] *= XMVectorSplatW(invMags);
    farPlane = XMPlaneNormalize(farPlane);
    // Transpose the normalized plane equations.
    tPlanes  = XMMatrixTranspose(frustumPlanes);
	// Store the results.
    Frustum frustum;
	XMStoreFloat4x4A(&frustum.m_tPlanes, tPlanes);
	XMStoreFloat4A(&frustum.m_farPlane, farPlane);
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
