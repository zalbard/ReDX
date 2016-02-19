#include "Camera.h"
#include "Math.h"

using DirectX::XMVECTOR;
using DirectX::XMMATRIX;

PerspectiveCamera::PerspectiveCamera(const long width, const long height, const float vFoV,
                                     const XMVECTOR& pos, const XMVECTOR& dir, const XMVECTOR& up)
    : position(pos)
    , orientQuat(DirectX::XMQuaternionRotationMatrix(DirectX::RotationMatrixLH(dir, up)))
    , projMat{DirectX::InfRevProjMatLH(width, height, vFoV)} {}

XMMATRIX PerspectiveCamera::computeViewProjMatrix() const {
    // Inverse the translation and the rotation for the view matrix
    const XMVECTOR translation = DirectX::XMVectorNegate(position);
    const XMVECTOR invOrient   = DirectX::XMQuaternionInverse(orientQuat);
    const XMMATRIX viewMat     = DirectX::XMMatrixAffineTransformation({1.f, 1.f, 1.f}, position,
                                                                       invOrient, translation);
    return viewMat * projMat;
}

void PerspectiveCamera::moveBack(const float dist) {
    moveForward(-dist);
}

void PerspectiveCamera::moveForward(const float dist) {
    const XMMATRIX orientMat = DirectX::XMMatrixRotationQuaternion(orientQuat);
    const XMVECTOR forward   = orientMat.r[2];
    position = DirectX::XMVectorAdd(position, DirectX::XMVectorScale(forward, dist));
}

void PerspectiveCamera::rotateLeft(const float angle) {
    rotateRight(-angle);
}

void PerspectiveCamera::rotateRight(const float angle) {
    const XMMATRIX orientMat = DirectX::XMMatrixRotationQuaternion(orientQuat);
    const XMVECTOR up        = orientMat.r[1];
    // XMQuaternionRotationNormal performs rotations clockwise
    const XMVECTOR rotQuat   = DirectX::XMQuaternionRotationNormal(up, angle);
    orientQuat = DirectX::XMQuaternionMultiply(orientQuat, rotQuat);
}

void PerspectiveCamera::rotateUpwards(const float angle) {
    rotateDownwards(-angle);
}

void PerspectiveCamera::rotateDownwards(const float angle) {
    const XMMATRIX orientMat = DirectX::XMMatrixRotationQuaternion(orientQuat);
    const XMVECTOR right     = orientMat.r[0];
    // XMQuaternionRotationNormal performs rotations clockwise
    const XMVECTOR rotQuat   = DirectX::XMQuaternionRotationNormal(right, angle);
    orientQuat = DirectX::XMQuaternionMultiply(orientQuat, rotQuat);
}

void PerspectiveCamera::rotateAndMoveForward(const float pitch, const float yaw, const float dist) {
    const XMMATRIX orientMat = DirectX::XMMatrixRotationQuaternion(orientQuat);
    const XMVECTOR right     = orientMat.r[0];
    const XMVECTOR up        = orientMat.r[1];
    const XMVECTOR forward   = orientMat.r[2];
    // Rotate downwards
    const XMVECTOR pitchQuat = DirectX::XMQuaternionRotationNormal(right, pitch);
    orientQuat = DirectX::XMQuaternionMultiply(orientQuat, pitchQuat);
    // Rotate right
    const XMVECTOR yawQuat   = DirectX::XMQuaternionRotationNormal(up, yaw);
    orientQuat = DirectX::XMQuaternionMultiply(orientQuat, yawQuat);
    // Translate along the forward direction
    position   = DirectX::XMVectorAdd(position, DirectX::XMVectorScale(forward, dist));
}
