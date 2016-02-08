#include "Camera.h"
#include "Math.h"

using DirectX::XMVECTOR;
using DirectX::XMMATRIX;

PerspectiveCamera::PerspectiveCamera(const long width, const long height, const float fovY,
                                     const XMVECTOR& pos, const XMVECTOR& dir, const XMVECTOR& up)
    : position(pos)
    , orientQuat(DirectX::XMQuaternionRotationMatrix(DirectX::XMMatrixLookToLH({0.f}, dir, up)))
    , projMat{InfRevProjMatLH(static_cast<float>(width), static_cast<float>(height), fovY)} {}

XMMATRIX PerspectiveCamera::computeViewProjMatrix() const
{
    const XMVECTOR translation = DirectX::XMVectorNegate(position);
    const XMMATRIX viewMat     = DirectX::XMMatrixAffineTransformation({1.f, 1.f, 1.f}, position,
                                                                       orientQuat, translation);
    return viewMat * projMat;
}
