#define WIN32_LEAN_AND_MEAN
#include "DX12Renderer.h"
#include "Camera.h"

#include <cmath>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

using Microsoft::WRL::ComPtr;
using namespace DirectX;

#pragma warning(disable: 28251)

#define HR(x, msg) do { HRESULT _hr = (x); if (FAILED(_hr)) FailFast(msg); } while(0)

static const char* g_vsSrc = R"(
cbuffer CB : register(b0)
{
    float4x4 g_mvp;
    float4x4 g_world;
    float3   g_lightPos;   float _pad0;
    float3   g_eyePos;     float _pad1;
    float3   g_lightColor; float _pad2;
    float    g_ambient;    float3 _pad3;
};

struct VSIn
{
    float3 pos : POSITION;
    float3 nrm : NORMAL;
    float3 col : COLOR;
};

struct VSOut
{
    float4 pos      : SV_POSITION;
    float3 worldPos : TEXCOORD0;
    float3 normal   : TEXCOORD1;
    float3 col      : COLOR;
};

VSOut main(VSIn vin)
{
    VSOut o;
    float4 wp = mul(float4(vin.pos, 1.0), g_world);
    o.worldPos = wp.xyz;
    o.normal = normalize(mul(vin.nrm, (float3x3)g_world));
    o.pos = mul(float4(vin.pos, 1.0), g_mvp);
    o.col = vin.col;
    return o;
}
)";

static const char* g_psSrc = R"(
cbuffer CB : register(b0)
{
    float4x4 g_mvp;
    float4x4 g_world;
    float3   g_lightPos;   float _pad0;
    float3   g_eyePos;     float _pad1;
    float3   g_lightColor; float _pad2;
    float    g_ambient;    float3 _pad3;
};

struct PSIn
{
    float4 pos      : SV_POSITION;
    float3 worldPos : TEXCOORD0;
    float3 normal   : TEXCOORD1;
    float3 col      : COLOR;
};

float4 main(PSIn pin) : SV_TARGET
{
    float3 N = normalize(pin.normal);
    float3 L = normalize(g_lightPos - pin.worldPos);
    float3 V = normalize(g_eyePos   - pin.worldPos);
    float3 R = reflect(-L, N);

    float ndotl = max(dot(N, L), 0.0);
    float spec = pow(max(dot(R, V), 0.0), 64.0);

    float3 ambient = g_ambient * pin.col;
    float3 diffuse = ndotl * pin.col * g_lightColor;
    float3 specular = spec * g_lightColor;

    float3 color = ambient + diffuse + specular;
    return float4(color, 1.0);
}
)";

void DX12Renderer::FailFast(const char* msg)
{
    MessageBoxA(nullptr, msg, "DX12 Framework Error", MB_ICONERROR | MB_OK);
    ExitProcess(1);
}

D3D12_RESOURCE_DESC DX12Renderer::BufferDesc(UINT64 byteSize)
{
    D3D12_RESOURCE_DESC d = {};
    d.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    d.Width = byteSize;
    d.Height = 1;
    d.DepthOrArraySize = 1;
    d.MipLevels = 1;
    d.Format = DXGI_FORMAT_UNKNOWN;
    d.SampleDesc.Count = 1;
    d.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    return d;
}

bool DX12Renderer::InitD3DCompile()
{
    HMODULE dll = LoadLibraryExA("d3dcompiler_47.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!dll) return false;
    m_D3DCompile = (D3DCompileFunc)GetProcAddress(dll, "D3DCompile");
    return m_D3DCompile != nullptr;
}

bool DX12Renderer::CompileShader(const char* src, const char* entry, const char* target, ComPtr<ID3DBlob>& outBlob)
{
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    ComPtr<ID3DBlob> errors;
    HRESULT hr = m_D3DCompile(src, strlen(src), nullptr, nullptr, nullptr, entry, target, flags, 0, &outBlob, &errors);
    return SUCCEEDED(hr);
}

void DX12Renderer::BuildCubeGeometry(std::vector<Vertex>& outVerts, std::vector<uint16_t>& outIndices)
{
    const float s = 0.6f;

    outVerts =
    {
        {+s,-s,-s,  +1,0,0,  1,0,0},
        {+s,+s,-s,  +1,0,0,  1,0,0},
        {+s,+s,+s,  +1,0,0,  1,0,0},
        {+s,-s,+s,  +1,0,0,  1,0,0},

        {-s,-s,+s,  -1,0,0,  0,1,0},
        {-s,+s,+s,  -1,0,0,  0,1,0},
        {-s,+s,-s,  -1,0,0,  0,1,0},
        {-s,-s,-s,  -1,0,0,  0,1,0},

        {-s,+s,-s,  0,+1,0,  0,0,1},
        {-s,+s,+s,  0,+1,0,  0,0,1},
        {+s,+s,+s,  0,+1,0,  0,0,1},
        {+s,+s,-s,  0,+1,0,  0,0,1},

        {-s,-s,+s,  0,-1,0,  1,1,0},
        {-s,-s,-s,  0,-1,0,  1,1,0},
        {+s,-s,-s,  0,-1,0,  1,1,0},
        {+s,-s,+s,  0,-1,0,  1,1,0},

        {-s,-s,+s,  0,0,+1,  1,0,1},
        {+s,-s,+s,  0,0,+1,  1,0,1},
        {+s,+s,+s,  0,0,+1,  1,0,1},
        {-s,+s,+s,  0,0,+1,  1,0,1},

        {+s,-s,-s,  0,0,-1,  0,1,1},
        {-s,-s,-s,  0,0,-1,  0,1,1},
        {-s,+s,-s,  0,0,-1,  0,1,1},
        {+s,+s,-s,  0,0,-1,  0,1,1},
    };

    outIndices.clear();
    outIndices.reserve(36);
    for (uint16_t f = 0; f < 6; ++f)
    {
        uint16_t base = f * 4;
        outIndices.push_back(base + 0);
        outIndices.push_back(base + 1);
        outIndices.push_back(base + 2);
        outIndices.push_back(base + 0);
        outIndices.push_back(base + 2);
        outIndices.push_back(base + 3);
    }

    m_indexCount = static_cast<UINT>(outIndices.size());
}

void DX12Renderer::CreateDefaultBuffer(
    const void* initData,
    UINT64 byteSize,
    ComPtr<ID3D12Resource>& defaultBuffer,
    ComPtr<ID3D12Resource>& uploadBuffer)
{
    D3D12_RESOURCE_DESC desc = BufferDesc(byteSize);

    D3D12_HEAP_PROPERTIES defaultHeap = {};
    defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;

    HR(m_device->CreateCommittedResource(
        &defaultHeap,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&defaultBuffer)), "CreateCommittedResource DEFAULT failed");

    D3D12_HEAP_PROPERTIES uploadHeap = {};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;

    HR(m_device->CreateCommittedResource(
        &uploadHeap,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&uploadBuffer)), "CreateCommittedResource UPLOAD failed");

    void* mapped = nullptr;
    D3D12_RANGE readRange = { 0, 0 };
    HR(uploadBuffer->Map(0, &readRange, &mapped), "uploadBuffer Map failed");
    std::memcpy(mapped, initData, static_cast<size_t>(byteSize));
    uploadBuffer->Unmap(0, nullptr);

    HR(m_commandAllocator->Reset(), "CommandAllocator Reset failed (copy)");
    HR(m_commandList->Reset(m_commandAllocator.Get(), nullptr), "CommandList Reset failed (copy)");

    m_commandList->CopyBufferRegion(defaultBuffer.Get(), 0, uploadBuffer.Get(), 0, byteSize);

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = defaultBuffer.Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
    m_commandList->ResourceBarrier(1, &barrier);

    HR(m_commandList->Close(), "CommandList Close failed (copy)");
    ID3D12CommandList* lists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(1, lists);

    WaitForGPU();
}

bool DX12Renderer::Initialize(HWND hwnd, unsigned int width, unsigned int height)
{
    m_hWnd = hwnd;

    if (!InitD3DCompile()) return false;
    if (!InitD3D()) return false;
    if (!InitSwapChainAndRTV(hwnd, width, height)) return false;
    if (!InitDepthBuffer(width, height)) return false;
    if (!InitConstantBuffer()) return false;
    if (!CreatePipeline()) return false;
    if (!CreateCubeBuffers()) return false;

    return true;
}

bool DX12Renderer::InitD3D()
{
    HR(CreateDXGIFactory1(IID_PPV_ARGS(&m_dxgiFactory)), "CreateDXGIFactory1 failed");
    HR(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)), "D3D12CreateDevice failed");

    D3D12_COMMAND_QUEUE_DESC q = {};
    q.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    HR(m_device->CreateCommandQueue(&q, IID_PPV_ARGS(&m_commandQueue)), "CreateCommandQueue failed");

    HR(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)), "CreateCommandAllocator failed");
    HR(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_commandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_commandList)), "CreateCommandList failed");
    HR(m_commandList->Close(), "CommandList Close failed (init)");

    HR(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)), "CreateFence failed");

    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!m_fenceEvent) FailFast("CreateEvent failed");

    return true;
}

bool DX12Renderer::InitSwapChainAndRTV(HWND hwnd, unsigned int width, unsigned int height)
{
    DXGI_SWAP_CHAIN_DESC1 sc = {};
    sc.BufferCount = m_frameCount;
    sc.Width = width;
    sc.Height = height;
    sc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> sc1;
    HR(m_dxgiFactory->CreateSwapChainForHwnd(m_commandQueue.Get(), hwnd, &sc, nullptr, nullptr, &sc1), "CreateSwapChainForHwnd failed");
    m_dxgiFactory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
    HR(sc1.As(&m_swapChain), "SwapChain As IDXGISwapChain3 failed");
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heapDesc.NumDescriptors = m_frameCount;
    HR(m_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_rtvHeap)), "Create RTV heap failed");

    D3D12_CPU_DESCRIPTOR_HANDLE h = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < m_frameCount; ++i)
    {
        HR(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i])), "SwapChain GetBuffer failed");
        m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, h);
        m_rtvHandles[i] = h;
        h.ptr += m_rtvDescriptorSize;
    }

    m_viewport = { 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f };
    m_scissorRect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };

    return true;
}

bool DX12Renderer::InitDepthBuffer(unsigned int width, unsigned int height)
{
    D3D12_DESCRIPTOR_HEAP_DESC dsvDesc = {};
    dsvDesc.NumDescriptors = 1;
    dsvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    HR(m_device->CreateDescriptorHeap(&dsvDesc, IID_PPV_ARGS(&m_dsvHeap)), "Create DSV heap failed");

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = m_depthFormat;
    texDesc.SampleDesc.Count = 1;
    texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = m_depthFormat;
    clearValue.DepthStencil.Depth = 1.0f;

    HR(m_device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &clearValue,
        IID_PPV_ARGS(&m_depthBuffer)), "Create depth buffer failed");

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvView = {};
    dsvView.Format = m_depthFormat;
    dsvView.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    m_device->CreateDepthStencilView(m_depthBuffer.Get(), &dsvView, m_dsvHeap->GetCPUDescriptorHandleForHeapStart());

    return true;
}

bool DX12Renderer::InitConstantBuffer()
{
    m_cbSize = (sizeof(CBData) + 255) & ~255ULL;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC desc = BufferDesc(m_cbSize);

    HR(m_device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&m_cbUpload)), "Create constant buffer failed");

    HR(m_cbUpload->Map(0, nullptr, reinterpret_cast<void**>(&m_cbMapped)), "CB Map failed");

    XMStoreFloat4x4(&m_cbMapped->mvp, XMMatrixIdentity());
    XMStoreFloat4x4(&m_cbMapped->world, XMMatrixIdentity());
    m_cbMapped->lightPos = XMFLOAT3(2.0f, 2.0f, -2.0f);
    m_cbMapped->eyePos = XMFLOAT3(0.0f, 0.3f, -2.5f);
    m_cbMapped->lightColor = XMFLOAT3(1.0f, 1.0f, 1.0f);
    m_cbMapped->ambient = 0.15f;

    m_startTime = std::chrono::steady_clock::now();
    return true;
}

bool DX12Renderer::CreatePipeline()
{
    D3D12_ROOT_PARAMETER param = {};
    param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    param.Descriptor.ShaderRegister = 0;
    param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC rs = {};
    rs.NumParameters = 1;
    rs.pParameters = &param;
    rs.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> serialized, errors;
    HR(D3D12SerializeRootSignature(&rs, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errors), "D3D12SerializeRootSignature failed");
    HR(m_device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)), "CreateRootSignature failed");

    ComPtr<ID3DBlob> vs, ps;
    if (!CompileShader(g_vsSrc, "main", "vs_5_0", vs)) FailFast("VS compile failed");
    if (!CompileShader(g_psSrc, "main", "ps_5_0", ps)) FailFast("PS compile failed");

    D3D12_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
    pso.InputLayout = { layout, _countof(layout) };
    pso.pRootSignature = m_rootSignature.Get();
    pso.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
    pso.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };

    pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pso.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    pso.RasterizerState.DepthClipEnable = TRUE;

    pso.BlendState.AlphaToCoverageEnable = FALSE;
    pso.BlendState.IndependentBlendEnable = FALSE;
    D3D12_RENDER_TARGET_BLEND_DESC rt = {};
    rt.BlendEnable = FALSE;
    rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    pso.BlendState.RenderTarget[0] = rt;

    pso.DepthStencilState.DepthEnable = TRUE;
    pso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    pso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    pso.DepthStencilState.StencilEnable = FALSE;
    pso.DSVFormat = m_depthFormat;

    pso.SampleMask = UINT_MAX;
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.NumRenderTargets = 1;
    pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pso.SampleDesc.Count = 1;

    HR(m_device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&m_pso)), "CreateGraphicsPipelineState failed");
    return true;
}

bool DX12Renderer::CreateCubeBuffers()
{
    std::vector<Vertex> verts;
    std::vector<uint16_t> indices;
    BuildCubeGeometry(verts, indices);

    const UINT64 vbSize = static_cast<UINT64>(verts.size()) * sizeof(Vertex);
    const UINT64 ibSize = static_cast<UINT64>(indices.size()) * sizeof(uint16_t);

    CreateDefaultBuffer(verts.data(), vbSize, m_vertexBuffer, m_vertexUpload);
    CreateDefaultBuffer(indices.data(), ibSize, m_indexBuffer, m_indexUpload);

    m_vbView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
    m_vbView.StrideInBytes = sizeof(Vertex);
    m_vbView.SizeInBytes = static_cast<UINT>(vbSize);

    m_ibView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
    m_ibView.Format = DXGI_FORMAT_R16_UINT;
    m_ibView.SizeInBytes = static_cast<UINT>(ibSize);

    return true;
}

void DX12Renderer::Render(const Camera& camera)
{
    HR(m_commandAllocator->Reset(), "CommandAllocator Reset failed (draw)");
    HR(m_commandList->Reset(m_commandAllocator.Get(), m_pso.Get()), "CommandList Reset failed (draw)");

    m_commandList->RSSetViewports(1, &m_viewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = m_renderTargets[m_frameIndex].Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    m_commandList->ResourceBarrier(1, &barrier);

    D3D12_CPU_DESCRIPTOR_HANDLE dsv = m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
    m_commandList->OMSetRenderTargets(1, &m_rtvHandles[m_frameIndex], FALSE, &dsv);

    const float clearColor[] = { 0.07f, 0.12f, 0.35f, 1.0f };
    m_commandList->ClearRenderTargetView(m_rtvHandles[m_frameIndex], clearColor, 0, nullptr);
    m_commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    auto now = std::chrono::steady_clock::now();
    float t = std::chrono::duration<float>(now - m_startTime).count();

    XMMATRIX world = XMMatrixRotationY(t) * XMMatrixRotationX(t * 0.6f);

    XMFLOAT3 camPos = camera.GetPosition();
    XMMATRIX view = camera.GetViewMatrix();

    float aspect = m_viewport.Width / m_viewport.Height;
    XMMATRIX proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), aspect, 0.1f, 100.0f);

    XMMATRIX worldViewProj = world * view * proj;

    XMStoreFloat4x4(&m_cbMapped->mvp, XMMatrixTranspose(worldViewProj));
    XMStoreFloat4x4(&m_cbMapped->world, XMMatrixTranspose(world));
    m_cbMapped->eyePos = camPos;
    m_cbMapped->lightPos = XMFLOAT3(2.0f * cosf(t), 1.5f, 2.0f * sinf(t));
    m_cbMapped->lightColor = XMFLOAT3(1.0f, 1.0f, 1.0f);
    m_cbMapped->ambient = 0.12f;

    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
    m_commandList->SetGraphicsRootConstantBufferView(0, m_cbUpload->GetGPUVirtualAddress());

    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_commandList->IASetVertexBuffers(0, 1, &m_vbView);
    m_commandList->IASetIndexBuffer(&m_ibView);
    m_commandList->DrawIndexedInstanced(m_indexCount, 1, 0, 0, 0);

    std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
    m_commandList->ResourceBarrier(1, &barrier);

    HR(m_commandList->Close(), "CommandList Close failed (draw)");
    ID3D12CommandList* lists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(1, lists);

    m_swapChain->Present(1, 0);
    MoveToNextFrame();
}

void DX12Renderer::Shutdown()
{
    WaitForGPU();

    if (m_cbUpload && m_cbMapped)
    {
        m_cbUpload->Unmap(0, nullptr);
        m_cbMapped = nullptr;
    }

    if (m_fenceEvent)
    {
        CloseHandle(m_fenceEvent);
        m_fenceEvent = nullptr;
    }
}

void DX12Renderer::WaitForGPU()
{
    const UINT64 fenceToWaitFor = ++m_fenceValue;
    HR(m_commandQueue->Signal(m_fence.Get(), fenceToWaitFor), "Signal fence failed");

    if (m_fence->GetCompletedValue() < fenceToWaitFor)
    {
        HR(m_fence->SetEventOnCompletion(fenceToWaitFor, m_fenceEvent), "SetEventOnCompletion failed");
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }
}

void DX12Renderer::MoveToNextFrame()
{
    const UINT64 fenceToWaitFor = ++m_fenceValue;
    HR(m_commandQueue->Signal(m_fence.Get(), fenceToWaitFor), "Signal fence failed (next frame)");

    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

    if (m_fence->GetCompletedValue() < fenceToWaitFor)
    {
        HR(m_fence->SetEventOnCompletion(fenceToWaitFor, m_fenceEvent), "SetEventOnCompletion failed (next frame)");
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }
}