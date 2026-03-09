#pragma once
#include <DirectXMath.h>

class Input;

class Camera
{
public:
    void SetPosition(float x, float y, float z);
    void Update(Input& input, float deltaTime);
    DirectX::XMMATRIX GetViewMatrix() const;
    DirectX::XMFLOAT3 GetPosition() const { return m_position; }

private:
    DirectX::XMFLOAT3 m_position = { 0.0f, 0.3f, -2.5f };
    float GetYaw() const { return m_yaw; }
    float GetPitch() const { return m_pitch; }
    float m_yaw = 0.0f;
    float m_pitch = 0.0f;
    float m_speed = 3.0f;
    float m_mouseSensitivity = 0.002f;
};