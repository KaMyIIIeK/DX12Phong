#ifndef GAME_OBJECT_H_
#define GAME_OBJECT_H_

#include <d3d12.h>
#include <d3dx12.h>
#include <SimpleMath.h>

using namespace DirectX;
using namespace DirectX::SimpleMath;

class GameObject
{
public:
    explicit GameObject(UINT cbIndex)
        : m_cb_index_(cbIndex)
    {
    }

    virtual ~GameObject() = default;

    virtual void Update(float dt)
    {
        m_rotation_.y += m_rotation_speed_y_ * dt;
        RebuildWorld();
    }

    virtual void Draw(
        ID3D12GraphicsCommandList* cmdList,
        D3D12_GPU_DESCRIPTOR_HANDLE cbvHeapStart,
        UINT cbvDescriptorSize) const = 0;

    void SetPosition(const Vector3& position)
    {
        m_position_ = position;
        RebuildWorld();
    }

    void SetRotation(const Vector3& rotation)
    {
        m_rotation_ = rotation;
        RebuildWorld();
    }

    void SetScale(const Vector3& scale)
    {
        m_scale_ = scale;
        RebuildWorld();
    }

    void SetColor(const Vector4& color)
    {
        m_color_ = color;
    }

    void SetRotationSpeedY(float speed)
    {
        m_rotation_speed_y_ = speed;
    }

    const Matrix& World() const
    {
        return m_world_;
    }

    const Vector4& Color() const
    {
        return m_color_;
    }

    UINT CBIndex() const
    {
        return m_cb_index_;
    }

protected:
    void RebuildWorld()
    {
        m_world_ =
            Matrix::CreateScale(m_scale_) *
            Matrix::CreateFromYawPitchRoll(m_rotation_.y, m_rotation_.x, m_rotation_.z) *
            Matrix::CreateTranslation(m_position_);
    }

protected:
    UINT m_cb_index_ = 0;
    Vector3 m_position_ = Vector3::Zero;
    Vector3 m_rotation_ = Vector3::Zero;
    Vector3 m_scale_ = Vector3(1.0f, 1.0f, 1.0f);
    Vector4 m_color_ = Vector4(1.0f, 1.0f, 1.0f, 1.0f);
    float m_rotation_speed_y_ = 0.0f;
    Matrix m_world_ = Matrix::Identity;
};

class MeshObject final : public GameObject
{
public:
    MeshObject(
        UINT cbIndex,
        const D3D12_VERTEX_BUFFER_VIEW& vbv,
        const D3D12_INDEX_BUFFER_VIEW& ibv,
        UINT indexCount)
        : GameObject(cbIndex),
        m_vbv_(vbv),
        m_ibv_(ibv),
        m_index_count_(indexCount)
    {
    }

    void Draw(
        ID3D12GraphicsCommandList* cmdList,
        D3D12_GPU_DESCRIPTOR_HANDLE cbvHeapStart,
        UINT cbvDescriptorSize) const override
    {
        cmdList->IASetVertexBuffers(0, 1, &m_vbv_);
        cmdList->IASetIndexBuffer(&m_ibv_);

        CD3DX12_GPU_DESCRIPTOR_HANDLE cbvHandle(cbvHeapStart);
        cbvHandle.Offset(static_cast<INT>(m_cb_index_), cbvDescriptorSize);

        cmdList->SetGraphicsRootDescriptorTable(0, cbvHandle);
        cmdList->DrawIndexedInstanced(m_index_count_, 1, 0, 0, 0);
    }

private:
    D3D12_VERTEX_BUFFER_VIEW m_vbv_{};
    D3D12_INDEX_BUFFER_VIEW m_ibv_{};
    UINT m_index_count_ = 0;
};

#endif // GAME_OBJECT_H_