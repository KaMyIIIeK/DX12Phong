#include "Camera.h"
#include "Input.h"
#include <cmath>

using namespace DirectX;

void Camera::SetPosition(float x, float y, float z)
{
    m_position = { x, y, z };
}

void Camera::Update(Input& input, float deltaTime)
{
    if (input.IsMouseCaptured())
    {
        m_yaw -= input.GetMouseDeltaX() * m_mouseSensitivity;
        m_pitch -= input.GetMouseDeltaY() * m_mouseSensitivity;
    }

    // Полная матрица вращения камеры: сначала pitch, потом yaw
    XMMATRIX rotation = XMMatrixRotationRollPitchYaw(m_pitch, m_yaw, 0.0f);

    XMVECTOR forwardVec = XMVector3TransformNormal(
        XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), rotation);

    XMVECTOR rightVec = XMVector3TransformNormal(
        XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), rotation);

    XMFLOAT3 forward;
    XMFLOAT3 right;
    XMStoreFloat3(&forward, forwardVec);
    XMStoreFloat3(&right, rightVec);

    float speed = m_speed * deltaTime;

    if (input.IsKeyDown('W'))
    {
        m_position.x += forward.x * speed;
        m_position.y += forward.y * speed;
        m_position.z += forward.z * speed;
    }
    if (input.IsKeyDown('S'))
    {
        m_position.x -= forward.x * speed;
        m_position.y -= forward.y * speed;
        m_position.z -= forward.z * speed;
    }
    if (input.IsKeyDown('A'))
    {
        m_position.x -= right.x * speed;
        m_position.y -= right.y * speed;
        m_position.z -= right.z * speed;
    }
    if (input.IsKeyDown('D'))
    {
        m_position.x += right.x * speed;
        m_position.y += right.y * speed;
        m_position.z += right.z * speed;
    }

    input.ResetMouseDelta();
}

XMMATRIX Camera::GetViewMatrix() const
{
    XMMATRIX rotation = XMMatrixRotationRollPitchYaw(m_pitch, m_yaw, 0.0f);

    XMVECTOR eye = XMLoadFloat3(&m_position);

    XMVECTOR forward = XMVector3TransformNormal(
        XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f), rotation);

    XMVECTOR up = XMVector3TransformNormal(
        XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), rotation);

    XMVECTOR at = XMVectorAdd(eye, forward);

    return XMMatrixLookAtLH(eye, at, up);
}