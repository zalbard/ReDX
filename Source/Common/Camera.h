#pragma once

#include <DirectXMathSSE4.h>
#include "Definitions.h"

class Frustum;

class PerspectiveCamera {
public:
    RULE_OF_ZERO(PerspectiveCamera);
    // Ctor. Parameters: the width and the height of the sensor (in pixels),
    // the vertical field of view 'vFoV' (in radians),
    // the position 'pos', the viewing direction 'dir' and the 'up' vector.
    explicit PerspectiveCamera(const float width, const float height, const float vFoV,
                               DirectX::FXMVECTOR pos, DirectX::FXMVECTOR dir,
                               DirectX::FXMVECTOR up);
    // Returns the position of the camera.
    DirectX::XMVECTOR position() const;
    // Sets the position of the camera.
    void setPosition(DirectX::FXMVECTOR pos);
    // Returns the world-space up vector of the camera.
    DirectX::XMVECTOR upVector() const;
    // Sets the world-space up vector of the camera.
    void setUpVector(DirectX::FXMVECTOR up);
    // Returns the orientation as a rotation matrix.
    DirectX::XMMATRIX orientationMatrix() const;
    // Returns the orientation as a quaternion.
    DirectX::XMVECTOR orientationQuaternion() const;
    // Sets the orientation defined by a quaternion.
    void setOrientation(DirectX::FXMVECTOR orientQuat);
    // Returns the projection matrix.
    DirectX::XMMATRIX projectionMatrix() const;
    // Returns the normalized direction along the optical axis.
    DirectX::XMVECTOR computeForwardDir() const;
    // Returns the view matrix.
    DirectX::XMMATRIX computeViewMatrix() const;
    // Returns the view-projection matrix (and, optionally, the view matrix).
    DirectX::XMMATRIX computeViewProjMatrix(DirectX::XMMATRIX* viewMat = nullptr) const;
    // Returns a 3x3 transformation matrix which transforms
    // raster coordinates (x, y, 1) into the raster-to-camera direction in world space.
    DirectX::XMMATRIX computeRasterToViewDirMatrix() const;
    // Computes the viewing frustum bounded by the far/left/right/top/bottom planes.
    Frustum computeViewFrustum() const;
    // Moves the camera forward by 'dist' meters.
    void moveForward(const float dist);
    // Moves the camera back by 'dist' meters.
    void moveBack(const float dist);
    // Changes the yaw of the camera by 'angle' radians.
    void rotateLeft(const float angle);
    // Changes the yaw of the camera by -'angle' radians.
    void rotateRight(const float angle);
    // Changes the pitch of the camera by 'angle' radians.
    void rotateUpwards(const float angle);
    // Changes the pitch of the camera by -'angle' radians.
    void rotateDownwards(const float angle);
    // Rotates the camera by 'pitch' and 'yaw' radians, and
    // moves it along the forward direction by 'dist' meters.
    void rotateAndMoveForward(const float pitch, const float yaw, const float dist);
private:
    DirectX::XMFLOAT3A   m_position;    // Position
    DirectX::XMFLOAT3A   m_up;          // World-space up vector
    DirectX::XMFLOAT4A   m_orientQuat;  // Orientation (quaternion)
    DirectX::XMFLOAT4X4A m_projMat;     // Projection matrix
    DirectX::XMFLOAT2A   m_resolution;  // Viewport dimensions
};
