#include "Camera.h"
#include "Math.h"

using namespace DirectX;

PerspectiveCamera::PerspectiveCamera(const long width, const long height, const float vFoV,
                                     FXMVECTOR pos, FXMVECTOR dir, FXMVECTOR up)
    : m_position(pos)
    , m_worldUp(up)
    , m_orientQuat(XMQuaternionRotationMatrix(RotationMatrixLH(dir, up)))
    , m_projMat{InfRevProjMatLH(width, height, vFoV)} {}

XMVECTOR PerspectiveCamera::position() const {
    return m_position;
}

XMMATRIX PerspectiveCamera::projectionMatrix() const {
    return m_projMat;
}

XMVECTOR PerspectiveCamera::computeForwardDir() const {
    const XMMATRIX orientMat = XMMatrixRotationQuaternion(m_orientQuat);
    return orientMat.r[2];
}

XMMATRIX PerspectiveCamera::computeViewProjMatrix(XMMATRIX* const viewMat) const {
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
