#pragma once

#include <DirectXMath.h>

class PerspectiveCamera {
public:
    // Ctor. Parameters: the width and the height of the sensor (in pixels),
    // the vertical field of view (in radians),
    // the position 'pos', the viewing direction 'dir' and the 'up' vector
    explicit PerspectiveCamera(const long width, const long height, const float fovY,
                               const DirectX::XMVECTOR& pos, const DirectX::XMVECTOR& dir,
                               const DirectX::XMVECTOR& up);
    // Returns the view-projection matrix
    DirectX::XMMATRIX computeViewProjMatrix() const;
private:
    DirectX::XMVECTOR position;     // World-space position
    DirectX::XMVECTOR orientQuat;   // Orientation defined as a quaternion
    DirectX::XMMATRIX projMat;      // Projection matrix
};
