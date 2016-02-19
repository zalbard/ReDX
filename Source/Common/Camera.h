#pragma once

#include <DirectXMath.h>

class PerspectiveCamera {
public:
    // Ctor. Parameters: the width and the height of the sensor (in pixels),
    // the vertical field of view 'vFoV' (in radians),
    // the position 'pos', the viewing direction 'dir' and the 'up' vector
    explicit PerspectiveCamera(const long width, const long height, const float vFoV,
                               const DirectX::XMVECTOR& pos, const DirectX::XMVECTOR& dir,
                               const DirectX::XMVECTOR& up);
    // Returns the view-projection matrix
    DirectX::XMMATRIX computeViewProjMatrix() const;
    // Moves the camera forward by 'dist' meters
    void moveForward(const float dist);
    // Moves the camera back by 'dist' meters
    void moveBack(const float dist);
    // Changes the yaw of the camera by 'angle' radians
    void rotateLeft(const float angle);
    // Changes the yaw of the camera by -'angle' radians
    void rotateRight(const float angle);
    // Changes the pitch of the camera by 'angle' radians
    void rotateUpwards(const float angle);
    // Changes the pitch of the camera by -'angle' radians
    void rotateDownwards(const float angle);
    // Rotates the camera by 'pitch' and 'yaw' radians, and
    // moves it along the forward direction by 'dist' meters
    void rotateAndMoveForward(const float pitch, const float yaw, const float dist);
private:
    DirectX::XMVECTOR position;     // World-space position
    DirectX::XMVECTOR orientQuat;   // Orientation defined as a quaternion
    DirectX::XMMATRIX projMat;      // Projection matrix
};
