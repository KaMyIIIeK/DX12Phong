#include "DX12App.h"

#include <d3dcompiler.h>
#include <iostream>
#include <string>
#include <stdexcept>
#include <vector>
#include <cstdint>
#include <limits>
#include <algorithm>
#include <cmath>

#include <DirectXColors.h>
#include <SimpleMath.h>

#include "d3dUtil.h"
#include "vertex.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

void DX12App::EnableDebug()
{
#if defined(DEBUG) || defined(_DEBUG)
    ComPtr<ID3D12Debug> debugController;
    ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
    debugController->EnableDebugLayer();

    ComPtr<ID3D12Debug1> debugController1;
    if (SUCCEEDED(debugController.As(&debugController1)))
    {
        debugController1->SetEnableGPUBasedValidation(true);
    }
#endif
}

void DX12App::InitializeDevice()
{
    m_back_buffer_format_ = DXGI_FORMAT_R8G8B8A8_UNORM;
    m_depth_stencil_format_ = DXGI_FORMAT_D24_UNORM_S8_UINT;

    EnableDebug();
    ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&m_dxgi_factory_)));

    ThrowIfFailed(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device_)));
    ThrowIfFailed(m_device_->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence_)));

    m_RTV_descriptor_size_ = m_device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    m_DSV_descriptor_size_ = m_device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    m_CbvSrvUav_descriptor_size_ = m_device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    msQualityLevels_.Format = m_back_buffer_format_;
    msQualityLevels_.SampleCount = 4;
    msQualityLevels_.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
    msQualityLevels_.NumQualityLevels = 0;

    ThrowIfFailed(
        m_device_->CheckFeatureSupport(
            D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
            &msQualityLevels_,
            sizeof(msQualityLevels_)));
}

void DX12App::InitializeCommandObjects()
{
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

    ThrowIfFailed(m_device_->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_command_queue_)));
    ThrowIfFailed(m_device_->CreateCommandAllocator(queueDesc.Type, IID_PPV_ARGS(&m_direct_cmd_list_alloc_)));

    ThrowIfFailed(
        m_device_->CreateCommandList(
            0,
            queueDesc.Type,
            m_direct_cmd_list_alloc_.Get(),
            nullptr,
            IID_PPV_ARGS(&m_command_list_)));

    ThrowIfFailed(m_command_list_->Close());
}

void DX12App::CreateSwapChain(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC swDesc = {};
    swDesc.BufferDesc.Width = m_client_width_;
    swDesc.BufferDesc.Height = m_client_height_;
    swDesc.BufferDesc.RefreshRate.Numerator = 60;
    swDesc.BufferDesc.RefreshRate.Denominator = 1;
    swDesc.BufferDesc.Format = m_back_buffer_format_;
    swDesc.SampleDesc.Count = 1;
    swDesc.SampleDesc.Quality = 0;
    swDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swDesc.BufferCount = 2;
    swDesc.OutputWindow = hWnd;
    swDesc.Windowed = true;
    swDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    ThrowIfFailed(m_dxgi_factory_->CreateSwapChain(m_command_queue_.Get(), &swDesc, &m_swap_chain_));
}

void DX12App::CreateRTVAndDSVDescriptorHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC RTVHeapDesc = {};
    RTVHeapDesc.NumDescriptors = 2;
    RTVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    RTVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    RTVHeapDesc.NodeMask = 0;

    ThrowIfFailed(m_device_->CreateDescriptorHeap(&RTVHeapDesc, IID_PPV_ARGS(&m_RTV_heap_)));

    D3D12_DESCRIPTOR_HEAP_DESC DSVHeapDesc = {};
    DSVHeapDesc.NumDescriptors = 1;
    DSVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    DSVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    DSVHeapDesc.NodeMask = 0;

    ThrowIfFailed(m_device_->CreateDescriptorHeap(&DSVHeapDesc, IID_PPV_ARGS(&m_DSV_heap_)));
}

void DX12App::CreateCBVDescriptorHeap()
{
    D3D12_DESCRIPTOR_HEAP_DESC CBVHeapDesc = {};
    CBVHeapDesc.NumDescriptors = kMaxObjects_;
    CBVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    CBVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    CBVHeapDesc.NodeMask = 0;

    ThrowIfFailed(m_device_->CreateDescriptorHeap(&CBVHeapDesc, IID_PPV_ARGS(&m_CBV_heap_)));
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12App::GetBackBuffer() const
{
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(
        m_RTV_heap_->GetCPUDescriptorHandleForHeapStart(),
        m_current_back_buffer_,
        m_RTV_descriptor_size_);
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12App::GetDSV() const
{
    return m_DSV_heap_->GetCPUDescriptorHandleForHeapStart();
}

ID3D12Resource* DX12App::CurrentBackBuffer() const
{
    return m_swap_chain_buffer_[m_current_back_buffer_].Get();
}

void DX12App::CreateRTV()
{
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_RTV_heap_->GetCPUDescriptorHandleForHeapStart());

    for (UINT i = 0; i < 2; i++)
    {
        ThrowIfFailed(m_swap_chain_->GetBuffer(i, IID_PPV_ARGS(&m_swap_chain_buffer_[i])));
        m_device_->CreateRenderTargetView(m_swap_chain_buffer_[i].Get(), nullptr, rtvHandle);
        rtvHandle.Offset(1, m_RTV_descriptor_size_);
    }
}

void DX12App::CreateDSV()
{
    D3D12_RESOURCE_DESC dsDesc = {};
    dsDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    dsDesc.Width = m_client_width_;
    dsDesc.Height = m_client_height_;
    dsDesc.DepthOrArraySize = 1;
    dsDesc.MipLevels = 1;
    dsDesc.Format = m_depth_stencil_format_;
    dsDesc.SampleDesc.Count = 1;
    dsDesc.SampleDesc.Quality = 0;
    dsDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    dsDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE clrValue = {};
    clrValue.Format = m_depth_stencil_format_;
    clrValue.DepthStencil.Depth = 1.0f;
    clrValue.DepthStencil.Stencil = 0;

    CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
    ThrowIfFailed(
        m_device_->CreateCommittedResource(
            &heapProperties,
            D3D12_HEAP_FLAG_NONE,
            &dsDesc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &clrValue,
            IID_PPV_ARGS(&m_DSV_buffer)));

    m_device_->CreateDepthStencilView(m_DSV_buffer.Get(), nullptr, GetDSV());
}

void DX12App::SetViewport()
{
    vp_.TopLeftX = 0.0f;
    vp_.TopLeftY = 0.0f;
    vp_.Width = static_cast<float>(m_client_width_);
    vp_.Height = static_cast<float>(m_client_height_);
    vp_.MinDepth = 0.0f;
    vp_.MaxDepth = 1.0f;
}

void DX12App::SetScissor()
{
    m_scissor_rect_ = { 0, 0, m_client_width_, m_client_height_ };
}

void DX12App::CalculateGameStats(GameTimer& gt, HWND hWnd)
{
    static int frameCnt = 0;
    static float timeElapsed = 0.0f;

    frameCnt++;

    if ((gt.TotalTime() - timeElapsed) >= 1.0f)
    {
        float fps = static_cast<float>(frameCnt);
        float mspf = 1000.0f / fps;

        std::wstring windowText =
            L"WINDOW fps: " + std::to_wstring(fps) + L" mspf: " + std::to_wstring(mspf);

        SetWindowText(hWnd, windowText.c_str());

        frameCnt = 0;
        timeElapsed += 1.0f;
    }
}

void DX12App::FlushCommandQueue()
{
    m_current_fence_++;
    ThrowIfFailed(m_command_queue_->Signal(m_fence_.Get(), m_current_fence_));

    if (m_fence_->GetCompletedValue() < m_current_fence_)
    {
        HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
        ThrowIfFailed(m_fence_->SetEventOnCompletion(m_current_fence_, eventHandle));
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }
}

bool DX12App::TryLoadObjFromKnownPaths()
{
    const std::vector<std::string> paths =
    {
        "sponza.obj",
        ".\\sponza.obj",
        "..\\sponza.obj",
        "..\\..\\sponza.obj",
        "..\\..\\..\\sponza.obj",
        "..\\..\\..\\Lab4\\sponza.obj",
        "..\\..\\Lab4\\sponza.obj"
    };

    for (const auto& path : paths)
    {
        if (objParser.Load(path))
        {
            m_loaded_path_ = path;
            return true;
        }
    }

    return false;
}

void DX12App::BuildFallbackCube()
{
    m_cpu_vertices_ =
    {
        Vertex{ Vector3(-0.5f, -0.5f, -0.5f), Vector3(0.f,  0.f, -1.f), Vector2(0.f, 1.f) },
        Vertex{ Vector3(-0.5f,  0.5f, -0.5f), Vector3(0.f,  0.f, -1.f), Vector2(0.f, 0.f) },
        Vertex{ Vector3(0.5f,  0.5f, -0.5f), Vector3(0.f,  0.f, -1.f), Vector2(1.f, 0.f) },
        Vertex{ Vector3(0.5f, -0.5f, -0.5f), Vector3(0.f,  0.f, -1.f), Vector2(1.f, 1.f) },

        Vertex{ Vector3(-0.5f, -0.5f,  0.5f), Vector3(0.f,  0.f,  1.f), Vector2(1.f, 1.f) },
        Vertex{ Vector3(0.5f, -0.5f,  0.5f), Vector3(0.f,  0.f,  1.f), Vector2(0.f, 1.f) },
        Vertex{ Vector3(0.5f,  0.5f,  0.5f), Vector3(0.f,  0.f,  1.f), Vector2(0.f, 0.f) },
        Vertex{ Vector3(-0.5f,  0.5f,  0.5f), Vector3(0.f,  0.f,  1.f), Vector2(1.f, 0.f) },

        Vertex{ Vector3(-0.5f, -0.5f,  0.5f), Vector3(-1.f,  0.f,  0.f), Vector2(0.f, 1.f) },
        Vertex{ Vector3(-0.5f,  0.5f,  0.5f), Vector3(-1.f,  0.f,  0.f), Vector2(0.f, 0.f) },
        Vertex{ Vector3(-0.5f,  0.5f, -0.5f), Vector3(-1.f,  0.f,  0.f), Vector2(1.f, 0.f) },
        Vertex{ Vector3(-0.5f, -0.5f, -0.5f), Vector3(-1.f,  0.f,  0.f), Vector2(1.f, 1.f) },

        Vertex{ Vector3(0.5f, -0.5f, -0.5f), Vector3(1.f,  0.f,  0.f), Vector2(0.f, 1.f) },
        Vertex{ Vector3(0.5f,  0.5f, -0.5f), Vector3(1.f,  0.f,  0.f), Vector2(0.f, 0.f) },
        Vertex{ Vector3(0.5f,  0.5f,  0.5f), Vector3(1.f,  0.f,  0.f), Vector2(1.f, 0.f) },
        Vertex{ Vector3(0.5f, -0.5f,  0.5f), Vector3(1.f,  0.f,  0.f), Vector2(1.f, 1.f) },

        Vertex{ Vector3(-0.5f,  0.5f, -0.5f), Vector3(0.f,  1.f,  0.f), Vector2(0.f, 1.f) },
        Vertex{ Vector3(-0.5f,  0.5f,  0.5f), Vector3(0.f,  1.f,  0.f), Vector2(0.f, 0.f) },
        Vertex{ Vector3(0.5f,  0.5f,  0.5f), Vector3(0.f,  1.f,  0.f), Vector2(1.f, 0.f) },
        Vertex{ Vector3(0.5f,  0.5f, -0.5f), Vector3(0.f,  1.f,  0.f), Vector2(1.f, 1.f) },

        Vertex{ Vector3(-0.5f, -0.5f,  0.5f), Vector3(0.f, -1.f,  0.f), Vector2(1.f, 0.f) },
        Vertex{ Vector3(-0.5f, -0.5f, -0.5f), Vector3(0.f, -1.f,  0.f), Vector2(1.f, 1.f) },
        Vertex{ Vector3(0.5f, -0.5f, -0.5f), Vector3(0.f, -1.f,  0.f), Vector2(0.f, 1.f) },
        Vertex{ Vector3(0.5f, -0.5f,  0.5f), Vector3(0.f, -1.f,  0.f), Vector2(0.f, 0.f) }
    };

    m_cpu_indices_ =
    {
        0,2,1,  0,3,2,
        4,6,5,  4,7,6,
        8,10,9, 8,11,10,
        12,14,13, 12,15,14,
        16,18,17, 16,19,18,
        20,22,21, 20,23,22
    };

    m_obj_loaded_ = false;
    m_loaded_path_.clear();
}

void DX12App::ParseFile()
{
    m_cpu_vertices_.clear();
    m_cpu_indices_.clear();

    if (!TryLoadObjFromKnownPaths())
    {
        BuildFallbackCube();
        return;
    }

    m_cpu_vertices_ = objParser.GetVertices();
    m_cpu_indices_ = objParser.GetIndices();

    if (m_cpu_vertices_.empty() || m_cpu_indices_.empty())
    {
        BuildFallbackCube();
        return;
    }

    Vector3 minP(
        (std::numeric_limits<float>::max)(),
        (std::numeric_limits<float>::max)(),
        (std::numeric_limits<float>::max)());

    Vector3 maxP(
        -(std::numeric_limits<float>::max)(),
        -(std::numeric_limits<float>::max)(),
        -(std::numeric_limits<float>::max)());

    for (auto& v : m_cpu_vertices_)
    {
        minP.x = (std::min)(minP.x, v.pos.x);
        minP.y = (std::min)(minP.y, v.pos.y);
        minP.z = (std::min)(minP.z, v.pos.z);

        maxP.x = (std::max)(maxP.x, v.pos.x);
        maxP.y = (std::max)(maxP.y, v.pos.y);
        maxP.z = (std::max)(maxP.z, v.pos.z);

        if (v.normal.LengthSquared() < 0.000001f)
            v.normal = Vector3(0.f, 1.f, 0.f);
        else
            v.normal.Normalize();
    }

    const Vector3 center = 0.5f * (minP + maxP);
    const Vector3 extent = maxP - minP;
    float maxExtent = (std::max)(extent.x, (std::max)(extent.y, extent.z));
    if (maxExtent < 0.0001f)
        maxExtent = 1.0f;

    const float scale = 2.0f / maxExtent;

    for (auto& v : m_cpu_vertices_)
    {
        v.pos = (v.pos - center) * scale;
    }

    m_obj_loaded_ = true;
}

void DX12App::CreateVertexBuffer()
{
    if (m_cpu_vertices_.empty())
        ParseFile();

    const UINT vertexBufferByteSize =
        static_cast<UINT>(sizeof(Vertex) * m_cpu_vertices_.size());

    ThrowIfFailed(m_direct_cmd_list_alloc_->Reset());
    ThrowIfFailed(m_command_list_->Reset(m_direct_cmd_list_alloc_.Get(), nullptr));

    VertexBufferGPU_ = d3dUtil::CreateDefaultBuffer(
        m_device_.Get(),
        m_command_list_.Get(),
        m_cpu_vertices_.data(),
        vertexBufferByteSize,
        VertexBufferUploader_);

    VertexBuffers[0].BufferLocation = VertexBufferGPU_->GetGPUVirtualAddress();
    VertexBuffers[0].StrideInBytes = sizeof(Vertex);
    VertexBuffers[0].SizeInBytes = vertexBufferByteSize;
}

void DX12App::CreateIndexBuffer()
{
    if (m_cpu_indices_.empty())
        ParseFile();

    m_index_count_ = static_cast<UINT>(m_cpu_indices_.size());
    const UINT indexBufferByteSize =
        static_cast<UINT>(sizeof(UINT) * m_cpu_indices_.size());

    IndexBufferGPU_ = d3dUtil::CreateDefaultBuffer(
        m_device_.Get(),
        m_command_list_.Get(),
        m_cpu_indices_.data(),
        indexBufferByteSize,
        IndexBufferUploader_);

    ibv.BufferLocation = IndexBufferGPU_->GetGPUVirtualAddress();
    ibv.Format = DXGI_FORMAT_R32_UINT;
    ibv.SizeInBytes = indexBufferByteSize;

    ThrowIfFailed(m_command_list_->Close());

    ID3D12CommandList* cmdLists[] = { m_command_list_.Get() };
    m_command_queue_->ExecuteCommandLists(1, cmdLists);
    FlushCommandQueue();

    BuildSceneObjects();
}

void DX12App::BuildSceneObjects()
{
    m_objects_.clear();

    auto obj1 = std::make_unique<MeshObject>(0, VertexBuffers[0], ibv, m_index_count_);
    obj1->SetPosition(Vector3(-1.8f, 0.0f, 0.0f));
    obj1->SetScale(Vector3(0.9f, 0.9f, 0.9f));
    obj1->SetColor(Vector4(1.0f, 0.25f, 0.25f, 1.0f));
    obj1->SetRotationSpeedY(0.8f);
    m_objects_.push_back(std::move(obj1));

    auto obj2 = std::make_unique<MeshObject>(1, VertexBuffers[0], ibv, m_index_count_);
    obj2->SetPosition(Vector3(0.0f, 0.0f, 0.0f));
    obj2->SetScale(Vector3(1.0f, 1.0f, 1.0f));
    obj2->SetColor(Vector4(0.25f, 1.0f, 0.25f, 1.0f));
    obj2->SetRotationSpeedY(-1.1f);
    m_objects_.push_back(std::move(obj2));

    auto obj3 = std::make_unique<MeshObject>(2, VertexBuffers[0], ibv, m_index_count_);
    obj3->SetPosition(Vector3(1.8f, 0.0f, 0.0f));
    obj3->SetScale(Vector3(0.9f, 0.9f, 0.9f));
    obj3->SetColor(Vector4(0.25f, 0.45f, 1.0f, 1.0f));
    obj3->SetRotationSpeedY(1.4f);
    m_objects_.push_back(std::move(obj3));
}

void DX12App::OnMouseDown(HWND hWnd)
{
    m_left_mouse_down_ = true;
    SetCapture(hWnd);
}

void DX12App::OnMouseUp()
{
    m_left_mouse_down_ = false;
    ReleaseCapture();
}

void DX12App::OnMouseMove(WPARAM btnState, int dx, int dy)
{
    if ((btnState & MK_LBUTTON) || m_left_mouse_down_)
    {
        const float angleSpeed = 0.01f;

        mTheta_ += dx * angleSpeed;
        mPhi_ -= dy * angleSpeed;

        if (mPhi_ < 0.1f)
            mPhi_ = 0.1f;
        if (mPhi_ > XM_PI - 0.1f)
            mPhi_ = XM_PI - 0.1f;
    }
}

void DX12App::Update(const GameTimer& gt)
{
    const float x = mRadius_ * sinf(mPhi_) * cosf(mTheta_);
    const float z = mRadius_ * sinf(mPhi_) * sinf(mTheta_);
    const float y = mRadius_ * cosf(mPhi_);

    const Vector3 eyePos(x, y, z);
    const Vector3 target(0.0f, 0.0f, 0.0f);
    const Vector3 up(0.0f, 1.0f, 0.0f);

    mView_ = Matrix::CreateLookAt(eyePos, target, up);

    const float t = gt.TotalTime();
    Vector3 lightDir(std::cosf(t * 0.8f), -0.65f, std::sinf(t * 0.8f));
    lightDir.Normalize();

    Matrix viewProj = (mView_ * mProj_).Transpose();

    PassConstants passData;
    passData.ViewProj = viewProj;
    passData.LightDir = Vector4(lightDir.x, lightDir.y, lightDir.z, 0.0f);
    passData.EyePosW = Vector4(eyePos.x, eyePos.y, eyePos.z, 1.0f);
    passData.AmbientStrength = 0.22f;
    passData.SpecularStrength = 0.70f;
    passData.SpecularPower = 48.0f;

    m_passCB_->CopyData(0, passData);

    for (size_t i = 0; i < m_objects_.size(); ++i)
    {
        m_objects_[i]->Update(gt.DeltaTime());

        ObjectConstants objData;
        objData.World = m_objects_[i]->World().Transpose();
        objData.Color = m_objects_[i]->Color();

        m_objectCB_->CopyData(static_cast<int>(i), objData);
    }
}

void DX12App::InitUploadBuffer()
{
    m_objectCB_ = std::make_unique<UploadBuffer<ObjectConstants>>(
        m_device_.Get(),
        (std::max)(1u, static_cast<UINT>(m_objects_.size())),
        true);

    m_passCB_ = std::make_unique<UploadBuffer<PassConstants>>(
        m_device_.Get(),
        1,
        true);
}

void DX12App::CreateConstantBufferView()
{
    const UINT objCBByteSize = d3dUtil::CalcConstantBufferSize(sizeof(ObjectConstants));
    const UINT passCBByteSize = d3dUtil::CalcConstantBufferSize(sizeof(PassConstants));

    CD3DX12_CPU_DESCRIPTOR_HANDLE cbvHandle(m_CBV_heap_->GetCPUDescriptorHandleForHeapStart());

    D3D12_GPU_VIRTUAL_ADDRESS objAddress = m_objectCB_->Resource()->GetGPUVirtualAddress();
    for (UINT i = 0; i < static_cast<UINT>(m_objects_.size()); ++i)
    {
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
        cbvDesc.BufferLocation = objAddress + i * objCBByteSize;
        cbvDesc.SizeInBytes = objCBByteSize;
        m_device_->CreateConstantBufferView(&cbvDesc, cbvHandle);
        cbvHandle.Offset(1, m_CbvSrvUav_descriptor_size_);
    }

    CD3DX12_CPU_DESCRIPTOR_HANDLE passHandle(
        m_CBV_heap_->GetCPUDescriptorHandleForHeapStart(),
        m_pass_cbv_index_,
        m_CbvSrvUav_descriptor_size_);

    D3D12_CONSTANT_BUFFER_VIEW_DESC passDesc = {};
    passDesc.BufferLocation = m_passCB_->Resource()->GetGPUVirtualAddress();
    passDesc.SizeInBytes = passCBByteSize;
    m_device_->CreateConstantBufferView(&passDesc, passHandle);
}

void DX12App::CreateRootSignature()
{
    CD3DX12_DESCRIPTOR_RANGE objCbvTable;
    objCbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

    CD3DX12_ROOT_PARAMETER slotRootParameter[2] = {};
    slotRootParameter[0].InitAsDescriptorTable(1, &objCbvTable);
    slotRootParameter[1].InitAsConstantBufferView(1);

    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(
        2,
        slotRootParameter,
        0,
        nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;

    HRESULT hr = D3D12SerializeRootSignature(
        &rootSigDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        serializedRootSig.GetAddressOf(),
        errorBlob.GetAddressOf());

    if (errorBlob)
        OutputDebugStringA(static_cast<char*>(errorBlob->GetBufferPointer()));

    ThrowIfFailed(hr);

    ThrowIfFailed(
        m_device_->CreateRootSignature(
            0,
            serializedRootSig->GetBufferPointer(),
            serializedRootSig->GetBufferSize(),
            IID_PPV_ARGS(&m_root_signature_)));
}

void DX12App::CompileShaders()
{
    const char* shaderCode = R"(
cbuffer cbPerObject : register(b0)
{
    float4x4 gWorld;
    float4 gColor;
};

cbuffer cbPass : register(b1)
{
    float4x4 gViewProj;
    float4 gLightDir;
    float4 gEyePosW;

    float gAmbientStrength;
    float gSpecularStrength;
    float gSpecularPower;
    float gPadding;
};

struct VSInput
{
    float3 PosL    : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC    : TEXCOORD;
};

struct PSInput
{
    float4 PosH    : SV_POSITION;
    float3 PosW    : POSITION0;
    float3 NormalW : NORMAL;
};

PSInput VS(VSInput vin)
{
    PSInput vout;

    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosW = posW.xyz;
    vout.PosH = mul(posW, gViewProj);
    vout.NormalW = normalize(mul(float4(vin.NormalL, 0.0f), gWorld).xyz);

    return vout;
}

float4 PS(PSInput pin) : SV_Target
{
    float3 N = normalize(pin.NormalW);
    float3 L = normalize(-gLightDir.xyz);
    float3 V = normalize(gEyePosW.xyz - pin.PosW);
    float3 R = reflect(-L, N);

    float ambient = gAmbientStrength;
    float diffuse = max(dot(N, L), 0.0f);

    float specular = 0.0f;
    if (diffuse > 0.0f)
    {
        specular = pow(max(dot(R, V), 0.0f), gSpecularPower) * gSpecularStrength;
    }

    float3 finalColor = gColor.rgb * (ambient + diffuse) + specular.xxx;
    finalColor = saturate(finalColor);

    return float4(finalColor, gColor.a);
}
)";

    UINT flags = 0;
#if defined(DEBUG) || defined(_DEBUG)
    flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ComPtr<ID3DBlob> errors = nullptr;

    HRESULT hr = D3DCompile(
        shaderCode,
        strlen(shaderCode),
        nullptr,
        nullptr,
        nullptr,
        "VS",
        "vs_5_1",
        flags,
        0,
        &mvsByteCode_,
        &errors);

    if (errors)
        OutputDebugStringA(static_cast<char*>(errors->GetBufferPointer()));
    ThrowIfFailed(hr);

    errors.Reset();

    hr = D3DCompile(
        shaderCode,
        strlen(shaderCode),
        nullptr,
        nullptr,
        nullptr,
        "PS",
        "ps_5_1",
        flags,
        0,
        &mpsByteCode_,
        &errors);

    if (errors)
        OutputDebugStringA(static_cast<char*>(errors->GetBufferPointer()));
    ThrowIfFailed(hr);
}

void DX12App::BuildLayout()
{
    m_input_layout_ =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
}

void DX12App::CreatePSO()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { m_input_layout_.data(), static_cast<UINT>(m_input_layout_.size()) };
    psoDesc.pRootSignature = m_root_signature_.Get();
    psoDesc.VS =
    {
        reinterpret_cast<BYTE*>(mvsByteCode_->GetBufferPointer()),
        mvsByteCode_->GetBufferSize()
    };
    psoDesc.PS =
    {
        reinterpret_cast<BYTE*>(mpsByteCode_->GetBufferPointer()),
        mpsByteCode_->GetBufferSize()
    };

    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState.FrontCounterClockwise = true;

    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = m_back_buffer_format_;
    psoDesc.DSVFormat = m_depth_stencil_format_;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleDesc.Quality = 0;

    ThrowIfFailed(m_device_->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&PSO_)));
}

void DX12App::SetTopology()
{
    // Оставлено для совместимости с winmain.cpp
}

void DX12App::InitProjectionMatrix()
{
    const float aspectRatio =
        static_cast<float>(m_client_width_) / static_cast<float>(m_client_height_);

    mProj_ = Matrix::CreatePerspectiveFieldOfView(
        XMConvertToRadians(60.0f),
        aspectRatio,
        0.1f,
        100.0f);
}

void DX12App::Draw(const GameTimer&)
{
    ThrowIfFailed(m_direct_cmd_list_alloc_->Reset());
    ThrowIfFailed(m_command_list_->Reset(m_direct_cmd_list_alloc_.Get(), PSO_.Get()));

    m_command_list_->RSSetViewports(1, &vp_);
    m_command_list_->RSSetScissorRects(1, &m_scissor_rect_);

    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_PRESENT,
        D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_command_list_->ResourceBarrier(1, &barrier);

    m_command_list_->ClearRenderTargetView(GetBackBuffer(), Colors::LightSteelBlue, 0, nullptr);
    m_command_list_->ClearDepthStencilView(
        GetDSV(),
        D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
        1.0f,
        0,
        0,
        nullptr);

    D3D12_CPU_DESCRIPTOR_HANDLE bb = GetBackBuffer();
    D3D12_CPU_DESCRIPTOR_HANDLE dsv = GetDSV();
    m_command_list_->OMSetRenderTargets(1, &bb, true, &dsv);

    ID3D12DescriptorHeap* descriptorHeaps[] = { m_CBV_heap_.Get() };
    m_command_list_->SetDescriptorHeaps(1, descriptorHeaps);
    m_command_list_->SetGraphicsRootSignature(m_root_signature_.Get());
    m_command_list_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    m_command_list_->SetGraphicsRootConstantBufferView(
        1,
        m_passCB_->Resource()->GetGPUVirtualAddress());

    const D3D12_GPU_DESCRIPTOR_HANDLE cbvHeapStart =
        m_CBV_heap_->GetGPUDescriptorHandleForHeapStart();

    for (const auto& obj : m_objects_)
    {
        obj->Draw(m_command_list_.Get(), cbvHeapStart, m_CbvSrvUav_descriptor_size_);
    }

    CD3DX12_RESOURCE_BARRIER barrier2 = CD3DX12_RESOURCE_BARRIER::Transition(
        CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PRESENT);
    m_command_list_->ResourceBarrier(1, &barrier2);

    ThrowIfFailed(m_command_list_->Close());

    ID3D12CommandList* cmdLists[] = { m_command_list_.Get() };
    m_command_queue_->ExecuteCommandLists(1, cmdLists);

    ThrowIfFailed(m_swap_chain_->Present(0, 0));
    m_current_back_buffer_ = (m_current_back_buffer_ + 1) % 2;

    FlushCommandQueue();
}