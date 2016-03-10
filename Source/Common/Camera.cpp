#include "Camera.h"
#include "Math.h"

using namespace DirectX;

PerspectiveCamera::PerspectiveCamera(const long width, const long height, const float vFoV,
                                     FXMVECTOR pos, FXMVECTOR dir, FXMVECTOR up)
    : m_position(pos)
    , m_orientQuat(XMQuaternionRotationMatrix(RotationMatrixLH(dir, up)))
    , m_projMat{InfRevProjMatLH(width, height, vFoV)} {}

XMVECTOR PerspectiveCamera::position() const {
    return m_position;
}

XMVECTOR PerspectiveCamera::computeForwardDir() const {
    const XMMATRIX orientMat = XMMatrixRotationQuaternion(m_orientQuat);
    return orientMat.r[2];
}

XMMATRIX PerspectiveCamera::computeViewProjMatrix() const {
    return computeViewMatrix() * m_projMat;
}

XMMATRIX PerspectiveCamera::computeViewMatrix() const {
    // Inverse the translation and the rotation for the view matrix
    const XMVECTOR translation = -m_position;
    const XMVECTOR invOrient   = XMQuaternionInverse(m_orientQuat);
    const XMVECTOR scale       = {1.f, 1.f, 1.f};
    return XMMatrixAffineTransformation(scale, m_position, invOrient, translation);
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
    const XMMATRIX orientMat = XMMatrixRotationQuaternion(m_orientQuat);
    const XMVECTOR up        = XMVECTOR{0.f, 1.f, 0.f};
    // XMQuaternionRotationNormal performs rotations clockwise
    const XMVECTOR rotQuat   = XMQuaternionRotationNormal(up, angle);
    m_orientQuat = XMQuaternionMultiply(m_orientQuat, rotQuat);
}

void PerspectiveCamera::rotateUpwards(const float angle) {
    rotateDownwards(-angle);
}

void PerspectiveCamera::rotateDownwards(const float angle) {
    const XMMATRIX orientMat = XMMatrixRotationQuaternion(m_orientQuat);
    const XMVECTOR right     = orientMat.r[0];
    // XMQuaternionRotationNormal performs rotations clockwise
    const XMVECTOR rotQuat   = XMQuaternionRotationNormal(right, angle);
    m_orientQuat = XMQuaternionMultiply(m_orientQuat, rotQuat);
}

void PerspectiveCamera::rotateAndMoveForward(const float pitch, const float yaw, const float dist) {
    const XMMATRIX orientMat = XMMatrixRotationQuaternion(m_orientQuat);
    const XMVECTOR right     = orientMat.r[0];
    const XMVECTOR up        = XMVECTOR{0.f, 1.f, 0.f};
    const XMVECTOR forward   = orientMat.r[2];
    // Rotate downwards; XMQuaternionRotationNormal performs rotations clockwise
    const XMVECTOR pitchQuat = XMQuaternionRotationNormal(right, pitch);
    m_orientQuat = XMQuaternionMultiply(m_orientQuat, pitchQuat);
    // Rotate right; XMQuaternionRotationNormal performs rotations clockwise
    const XMVECTOR yawQuat   = XMQuaternionRotationNormal(up, yaw);
    m_orientQuat = XMQuaternionMultiply(m_orientQuat, yawQuat);
    // Translate along the forward direction
    m_position  += forward * dist;
}
