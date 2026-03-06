#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cmath>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <wrl.h>

#include <vector>
#include <cstdint>
#include <cstring>
#include <chrono>

#include <DirectXMath.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

// Отключаем предупреждение о SAL-аннотациях для WinMain
#pragma warning(disable: 28251)

using Microsoft::WRL::ComPtr;
using namespace DirectX;

// ============================================================
// Предварительные объявления
// ============================================================
bool InitWindow(HINSTANCE hInstance, int nCmdShow);
bool InitD3D();
bool InitSwapChainAndRTV();
bool InitDepthBuffer();
bool InitConstantBuffer();
bool CreatePipeline();
bool CreateCubeBuffers();
void Draw();
int  Run();

void WaitForGPU();
void MoveToNextFrame();

// ============================================================
// Глобальные переменные
// ============================================================
HWND g_hWnd = nullptr;

ComPtr<IDXGIFactory4> g_dxgiFactory;
ComPtr<ID3D12Device> g_device;
ComPtr<ID3D12CommandQueue> g_commandQueue;
ComPtr<ID3D12CommandAllocator> g_commandAllocator;
ComPtr<ID3D12GraphicsCommandList> g_commandList;

ComPtr<ID3D12Fence> g_fence;
UINT64 g_fenceValue = 0;
HANDLE g_fenceEvent = nullptr;

static const UINT g_frameCount = 2;
ComPtr<IDXGISwapChain3> g_swapChain;
UINT g_frameIndex = 0;

ComPtr<ID3D12DescriptorHeap> g_rtvHeap;
UINT g_rtvDescriptorSize = 0;

ComPtr<ID3D12Resource> g_renderTargets[g_frameCount];
D3D12_CPU_DESCRIPTOR_HANDLE g_rtvHandles[g_frameCount]{};

D3D12_VIEWPORT g_viewport{};
D3D12_RECT g_scissorRect{};

// Depth buffer
ComPtr<ID3D12DescriptorHeap> g_dsvHeap;
ComPtr<ID3D12Resource> g_depthBuffer;
DXGI_FORMAT g_depthFormat = DXGI_FORMAT_D32_FLOAT;

// Вершинная структура: позиция, нормаль, цвет
struct Vertex
{
    float px, py, pz;
    float nx, ny, nz;
    float r, g, b;
};

// Буферы геометрии
ComPtr<ID3D12Resource> g_vertexBuffer;
ComPtr<ID3D12Resource> g_indexBuffer;
ComPtr<ID3D12Resource> g_vertexUpload;
ComPtr<ID3D12Resource> g_indexUpload;

D3D12_VERTEX_BUFFER_VIEW g_vbView{};
D3D12_INDEX_BUFFER_VIEW  g_ibView{};
UINT g_indexCount = 0;

// Pipeline
ComPtr<ID3D12RootSignature> g_rootSignature;
ComPtr<ID3D12PipelineState> g_pso;

// Константный буфер для данных освещения (выровнен по 256 байт)
struct alignas(256) CBData
{
    XMFLOAT4X4 mvp;       // world * view * proj (транспонирована, column‑major)
    XMFLOAT4X4 world;     // мировая матрица (транспонирована)
    XMFLOAT3   lightPos;  float _pad0;
    XMFLOAT3   eyePos;    float _pad1;
    XMFLOAT3   lightColor; float _pad2;
    float      ambient;   float _pad3[3];
};

ComPtr<ID3D12Resource> g_cbUpload;
CBData* g_cbMapped = nullptr;
UINT64 g_cbSize = 0;

static std::chrono::steady_clock::time_point g_startTime;

// ============================================================
// Переменные для камеры и ввода
// ============================================================
struct Camera
{
    XMFLOAT3 position = { 0.0f, 0.3f, -2.5f };
    float yaw = 0.0f;
    float pitch = 0.0f;
};
Camera g_camera;
float g_cameraSpeed = 3.0f;
float g_mouseSensitivity = 0.002f;
bool g_keys[256] = {};
int g_mouseDeltaX = 0, g_mouseDeltaY = 0;
bool g_mouseCaptured = false;
int g_lastMouseX = 0, g_lastMouseY = 0;
int g_centerX = 0, g_centerY = 0;          // центр окна для захвата мыши
bool g_ignoreMouseMove = false;             // флаг для игнорирования программного перемещения мыши

// ============================================================
// Динамическая загрузка D3DCompile (чтобы не линковаться с d3dcompiler.lib)
// ============================================================
typedef HRESULT(WINAPI* D3DCompileFunc)(
    LPCVOID pSrcData, SIZE_T SrcDataSize, LPCSTR pSourceName,
    const D3D_SHADER_MACRO* pDefines, ID3DInclude* pInclude,
    LPCSTR pEntrypoint, LPCSTR pTarget, UINT Flags1, UINT Flags2,
    ID3DBlob** ppCode, ID3DBlob** ppErrorMsgs);

static D3DCompileFunc g_D3DCompile = nullptr;

bool InitD3DCompile()
{
    HMODULE dll = LoadLibraryExA("d3dcompiler_47.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!dll) return false;
    g_D3DCompile = (D3DCompileFunc)GetProcAddress(dll, "D3DCompile");
    return g_D3DCompile != nullptr;
}

// ============================================================
// Вспомогательные функции (без d3dx12.h)
// ============================================================
static void FailFast(const char* msg)
{
    MessageBoxA(nullptr, msg, "DX12Phong - ошибка", MB_ICONERROR | MB_OK);
    ExitProcess(1);
}

#define HR(x, msg) do { HRESULT _hr = (x); if (FAILED(_hr)) FailFast(msg); } while(0)

static D3D12_RESOURCE_DESC BufferDesc(UINT64 byteSize)
{
    D3D12_RESOURCE_DESC d = {};
    d.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    d.Alignment = 0;
    d.Width = byteSize;
    d.Height = 1;
    d.DepthOrArraySize = 1;
    d.MipLevels = 1;
    d.Format = DXGI_FORMAT_UNKNOWN;
    d.SampleDesc.Count = 1;
    d.SampleDesc.Quality = 0;
    d.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    d.Flags = D3D12_RESOURCE_FLAG_NONE;
    return d;
}

// Создание буфера в default‑куче и копирование данных через upload‑буфер
static void CreateDefaultBuffer(
    const void* initData,
    UINT64 byteSize,
    ComPtr<ID3D12Resource>& defaultBuffer,
    ComPtr<ID3D12Resource>& uploadBuffer)
{
    D3D12_RESOURCE_DESC desc = BufferDesc(byteSize);

    D3D12_HEAP_PROPERTIES defaultHeap = {};
    defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;

    HR(g_device->CreateCommittedResource(
        &defaultHeap,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&defaultBuffer)), "CreateCommittedResource DEFAULT failed");

    D3D12_HEAP_PROPERTIES uploadHeap = {};
    uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;

    HR(g_device->CreateCommittedResource(
        &uploadHeap,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&uploadBuffer)), "CreateCommittedResource UPLOAD failed");

    void* mapped = nullptr;
    D3D12_RANGE readRange = { 0, 0 };
    HR(uploadBuffer->Map(0, &readRange, &mapped), "uploadBuffer Map failed");
    std::memcpy(mapped, initData, (size_t)byteSize);
    uploadBuffer->Unmap(0, nullptr);

    HR(g_commandAllocator->Reset(), "CommandAllocator Reset failed (copy)");
    HR(g_commandList->Reset(g_commandAllocator.Get(), nullptr), "CommandList Reset failed (copy)");

    g_commandList->CopyBufferRegion(defaultBuffer.Get(), 0, uploadBuffer.Get(), 0, byteSize);

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = defaultBuffer.Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
    g_commandList->ResourceBarrier(1, &barrier);

    HR(g_commandList->Close(), "CommandList Close failed (copy)");
    ID3D12CommandList* lists[] = { g_commandList.Get() };
    g_commandQueue->ExecuteCommandLists(1, lists);

    WaitForGPU();
}

// ============================================================
// Геометрия куба (24 вершины, 36 индексов)
// ============================================================
static void BuildCubeGeometry(std::vector<Vertex>& outVerts, std::vector<uint16_t>& outIndices)
{
    const float s = 0.6f;

    outVerts =
    {
        // +X
        {+s,-s,-s,  +1,0,0,  1,0,0},
        {+s,+s,-s,  +1,0,0,  1,0,0},
        {+s,+s,+s,  +1,0,0,  1,0,0},
        {+s,-s,+s,  +1,0,0,  1,0,0},
        // -X
        {-s,-s,+s,  -1,0,0,  0,1,0},
        {-s,+s,+s,  -1,0,0,  0,1,0},
        {-s,+s,-s,  -1,0,0,  0,1,0},
        {-s,-s,-s,  -1,0,0,  0,1,0},
        // +Y
        {-s,+s,-s,  0,+1,0,  0,0,1},
        {-s,+s,+s,  0,+1,0,  0,0,1},
        {+s,+s,+s,  0,+1,0,  0,0,1},
        {+s,+s,-s,  0,+1,0,  0,0,1},
        // -Y
        {-s,-s,+s,  0,-1,0,  1,1,0},
        {-s,-s,-s,  0,-1,0,  1,1,0},
        {+s,-s,-s,  0,-1,0,  1,1,0},
        {+s,-s,+s,  0,-1,0,  1,1,0},
        // +Z
        {-s,-s,+s,  0,0,+1,  1,0,1},
        {+s,-s,+s,  0,0,+1,  1,0,1},
        {+s,+s,+s,  0,0,+1,  1,0,1},
        {-s,+s,+s,  0,0,+1,  1,0,1},
        // -Z
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
    g_indexCount = (UINT)outIndices.size();
}

// Компиляция шейдера из строки (используем динамически загруженную функцию)
static bool CompileShader(const char* src, const char* entry, const char* target, ComPtr<ID3DBlob>& outBlob)
{
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(_DEBUG)
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    ComPtr<ID3DBlob> errors;
    HRESULT hr = g_D3DCompile(src, strlen(src), nullptr, nullptr, nullptr, entry, target, flags, 0, &outBlob, &errors);
    return SUCCEEDED(hr);
}

// ============================================================
// Шейдеры (модель Фонга)
// ============================================================
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

    float specPow = 64.0;
    float spec = pow(max(dot(R, V), 0.0), specPow);

    float3 ambient = g_ambient * pin.col;
    float3 diffuse = ndotl * pin.col * g_lightColor;
    float3 specular = spec * g_lightColor;

    float3 color = ambient + diffuse + specular;
    return float4(color, 1.0);
}
)";

// ============================================================
// Инициализация Direct3D
// ============================================================
bool InitD3D()
{
    HR(CreateDXGIFactory1(IID_PPV_ARGS(&g_dxgiFactory)), "CreateDXGIFactory1 failed");
    HR(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&g_device)), "D3D12CreateDevice failed");

    D3D12_COMMAND_QUEUE_DESC q = {};
    q.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    HR(g_device->CreateCommandQueue(&q, IID_PPV_ARGS(&g_commandQueue)), "CreateCommandQueue failed");

    HR(g_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_commandAllocator)), "CreateCommandAllocator failed");

    HR(g_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_commandAllocator.Get(), nullptr, IID_PPV_ARGS(&g_commandList)), "CreateCommandList failed");
    HR(g_commandList->Close(), "CommandList Close failed (init)");

    HR(g_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence)), "CreateFence failed");

    g_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (!g_fenceEvent) FailFast("CreateEvent failed");

    return true;
}

// Swap chain и RTV
bool InitSwapChainAndRTV()
{
    RECT rc{};
    GetClientRect(g_hWnd, &rc);
    UINT width = rc.right - rc.left;
    UINT height = rc.bottom - rc.top;

    DXGI_SWAP_CHAIN_DESC1 sc = {};
    sc.BufferCount = g_frameCount;
    sc.Width = width;
    sc.Height = height;
    sc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> sc1;
    HR(g_dxgiFactory->CreateSwapChainForHwnd(g_commandQueue.Get(), g_hWnd, &sc, nullptr, nullptr, &sc1), "CreateSwapChainForHwnd failed");
    g_dxgiFactory->MakeWindowAssociation(g_hWnd, DXGI_MWA_NO_ALT_ENTER);
    HR(sc1.As(&g_swapChain), "SwapChain As IDXGISwapChain3 failed");
    g_frameIndex = g_swapChain->GetCurrentBackBufferIndex();

    g_rtvDescriptorSize = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heapDesc.NumDescriptors = g_frameCount;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    HR(g_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&g_rtvHeap)), "Create RTV heap failed");

    D3D12_CPU_DESCRIPTOR_HANDLE h = g_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < g_frameCount; ++i)
    {
        HR(g_swapChain->GetBuffer(i, IID_PPV_ARGS(&g_renderTargets[i])), "SwapChain GetBuffer failed");
        g_device->CreateRenderTargetView(g_renderTargets[i].Get(), nullptr, h);
        g_rtvHandles[i] = h;
        h.ptr += g_rtvDescriptorSize;
    }

    g_viewport = { 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f };
    g_scissorRect = { 0, 0, (LONG)width, (LONG)height };

    return true;
}

// Буфер глубины
bool InitDepthBuffer()
{
    RECT rc{};
    GetClientRect(g_hWnd, &rc);
    UINT width = rc.right - rc.left;
    UINT height = rc.bottom - rc.top;

    D3D12_DESCRIPTOR_HEAP_DESC dsvDesc = {};
    dsvDesc.NumDescriptors = 1;
    dsvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    HR(g_device->CreateDescriptorHeap(&dsvDesc, IID_PPV_ARGS(&g_dsvHeap)), "Create DSV heap failed");

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = g_depthFormat;
    texDesc.SampleDesc.Count = 1;
    texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = g_depthFormat;
    clearValue.DepthStencil.Depth = 1.0f;
    clearValue.DepthStencil.Stencil = 0;

    HR(g_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &texDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearValue, IID_PPV_ARGS(&g_depthBuffer)), "Create depth buffer failed");

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvView = {};
    dsvView.Format = g_depthFormat;
    dsvView.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    g_device->CreateDepthStencilView(g_depthBuffer.Get(), &dsvView, g_dsvHeap->GetCPUDescriptorHandleForHeapStart());

    return true;
}

// Константный буфер (размечен как upload)
bool InitConstantBuffer()
{
    g_cbSize = (sizeof(CBData) + 255) & ~255ULL;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC desc = BufferDesc(g_cbSize);

    HR(g_device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&g_cbUpload)), "Create constant buffer failed");

    HR(g_cbUpload->Map(0, nullptr, reinterpret_cast<void**>(&g_cbMapped)), "CB Map failed");

    // Начальные значения (будут перезаписываться каждый кадр)
    XMStoreFloat4x4(&g_cbMapped->mvp, XMMatrixIdentity());
    XMStoreFloat4x4(&g_cbMapped->world, XMMatrixIdentity());
    g_cbMapped->lightPos = XMFLOAT3(2.0f, 2.0f, -2.0f);
    g_cbMapped->eyePos = g_camera.position;
    g_cbMapped->lightColor = XMFLOAT3(1.0f, 1.0f, 1.0f);
    g_cbMapped->ambient = 0.15f;

    g_startTime = std::chrono::steady_clock::now();
    return true;
}

// Создание корневой сигнатуры и PSO
bool CreatePipeline()
{
    // Корневая сигнатура: один CBV в корне (регистр b0)
    D3D12_ROOT_PARAMETER param = {};
    param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    param.Descriptor.ShaderRegister = 0;
    param.Descriptor.RegisterSpace = 0;
    param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC rs = {};
    rs.NumParameters = 1;
    rs.pParameters = &param;
    rs.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> serialized, errors;
    HR(D3D12SerializeRootSignature(&rs, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errors), "D3D12SerializeRootSignature failed");
    HR(g_device->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS(&g_rootSignature)), "CreateRootSignature failed");

    // Компиляция шейдеров
    ComPtr<ID3DBlob> vs, ps;
    if (!CompileShader(g_vsSrc, "main", "vs_5_0", vs)) FailFast("VS compile failed");
    if (!CompileShader(g_psSrc, "main", "ps_5_0", ps)) FailFast("PS compile failed");

    // Описание входных элементов (POSITION, NORMAL, COLOR)
    D3D12_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
    pso.InputLayout = { layout, _countof(layout) };
    pso.pRootSignature = g_rootSignature.Get();
    pso.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
    pso.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };

    // Растеризатор
    pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pso.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    pso.RasterizerState.FrontCounterClockwise = FALSE;
    pso.RasterizerState.DepthClipEnable = TRUE;

    // Состояние смешивания (без блендинга)
    pso.BlendState.AlphaToCoverageEnable = FALSE;
    pso.BlendState.IndependentBlendEnable = FALSE;
    D3D12_RENDER_TARGET_BLEND_DESC rt = {};
    rt.BlendEnable = FALSE;
    rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    pso.BlendState.RenderTarget[0] = rt;

    // Буфер глубины/трафарета
    pso.DepthStencilState.DepthEnable = TRUE;
    pso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    pso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    pso.DepthStencilState.StencilEnable = FALSE;
    pso.DSVFormat = g_depthFormat;

    pso.SampleMask = UINT_MAX;
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.NumRenderTargets = 1;
    pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pso.SampleDesc.Count = 1;

    HR(g_device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&g_pso)), "CreateGraphicsPipelineState failed");
    return true;
}

// Создание буферов вершин и индексов для куба
bool CreateCubeBuffers()
{
    std::vector<Vertex> verts;
    std::vector<uint16_t> indices;
    BuildCubeGeometry(verts, indices);

    const UINT64 vbSize = (UINT64)verts.size() * sizeof(Vertex);
    const UINT64 ibSize = (UINT64)indices.size() * sizeof(uint16_t);

    CreateDefaultBuffer(verts.data(), vbSize, g_vertexBuffer, g_vertexUpload);
    CreateDefaultBuffer(indices.data(), ibSize, g_indexBuffer, g_indexUpload);

    g_vbView.BufferLocation = g_vertexBuffer->GetGPUVirtualAddress();
    g_vbView.StrideInBytes = sizeof(Vertex);
    g_vbView.SizeInBytes = (UINT)vbSize;

    g_ibView.BufferLocation = g_indexBuffer->GetGPUVirtualAddress();
    g_ibView.Format = DXGI_FORMAT_R16_UINT;
    g_ibView.SizeInBytes = (UINT)ibSize;

    return true;
}

// ============================================================
// Синхронизация с GPU
// ============================================================
void WaitForGPU()
{
    const UINT64 fenceToWaitFor = ++g_fenceValue;
    HR(g_commandQueue->Signal(g_fence.Get(), fenceToWaitFor), "Signal fence failed");

    if (g_fence->GetCompletedValue() < fenceToWaitFor)
    {
        HR(g_fence->SetEventOnCompletion(fenceToWaitFor, g_fenceEvent), "SetEventOnCompletion failed");
        WaitForSingleObject(g_fenceEvent, INFINITE);
    }
}

void MoveToNextFrame()
{
    const UINT64 fenceToWaitFor = ++g_fenceValue;
    HR(g_commandQueue->Signal(g_fence.Get(), fenceToWaitFor), "Signal fence failed (next frame)");

    g_frameIndex = g_swapChain->GetCurrentBackBufferIndex();

    if (g_fence->GetCompletedValue() < fenceToWaitFor)
    {
        HR(g_fence->SetEventOnCompletion(fenceToWaitFor, g_fenceEvent), "SetEventOnCompletion failed (next frame)");
        WaitForSingleObject(g_fenceEvent, INFINITE);
    }
}

// ============================================================
// Обновление камеры
// ============================================================
void UpdateCamera(float deltaTime)
{
    if (g_mouseCaptured)
    {
        g_camera.yaw += g_mouseDeltaX * g_mouseSensitivity;
        g_camera.pitch -= g_mouseDeltaY * g_mouseSensitivity; // инвертировано

        if (g_camera.pitch > XM_PIDIV2 - 0.01f) g_camera.pitch = XM_PIDIV2 - 0.01f;
        if (g_camera.pitch < -XM_PIDIV2 + 0.01f) g_camera.pitch = -XM_PIDIV2 + 0.01f;

        g_mouseDeltaX = g_mouseDeltaY = 0;
    }

    float cosPitch = cosf(g_camera.pitch);
    float sinPitch = sinf(g_camera.pitch);
    float cosYaw = cosf(g_camera.yaw);
    float sinYaw = sinf(g_camera.yaw);

    XMFLOAT3 forward(
        cosPitch * sinYaw,
        sinPitch,
        cosPitch * cosYaw
    );
    XMFLOAT3 right(
        cosYaw,
        0.0f,
        -sinYaw
    );

    float speed = g_cameraSpeed * deltaTime;
    if (g_keys['W'])
    {
        g_camera.position.x += forward.x * speed;
        g_camera.position.y += forward.y * speed;
        g_camera.position.z += forward.z * speed;
    }
    if (g_keys['S'])
    {
        g_camera.position.x -= forward.x * speed;
        g_camera.position.y -= forward.y * speed;
        g_camera.position.z -= forward.z * speed;
    }
    if (g_keys['A'])
    {
        g_camera.position.x -= right.x * speed;
        g_camera.position.z -= right.z * speed;
    }
    if (g_keys['D'])
    {
        g_camera.position.x += right.x * speed;
        g_camera.position.z += right.z * speed;
    }
}

// ============================================================
// Отрисовка кадра
// ============================================================
void Draw()
{
    HR(g_commandAllocator->Reset(), "CommandAllocator Reset failed (draw)");
    HR(g_commandList->Reset(g_commandAllocator.Get(), g_pso.Get()), "CommandList Reset failed (draw)");

    g_commandList->RSSetViewports(1, &g_viewport);
    g_commandList->RSSetScissorRects(1, &g_scissorRect);

    // Переход заднего буфера в состояние Render Target
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = g_renderTargets[g_frameIndex].Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    g_commandList->ResourceBarrier(1, &barrier);

    D3D12_CPU_DESCRIPTOR_HANDLE dsv = g_dsvHeap->GetCPUDescriptorHandleForHeapStart();
    g_commandList->OMSetRenderTargets(1, &g_rtvHandles[g_frameIndex], FALSE, &dsv);

    const float clearColor[] = { 0.07f, 0.12f, 0.35f, 1.0f };
    g_commandList->ClearRenderTargetView(g_rtvHandles[g_frameIndex], clearColor, 0, nullptr);
    g_commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // Время и дельта
    static auto previousTime = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    float deltaTime = std::chrono::duration<float>(now - previousTime).count();
    previousTime = now;

    // Обновление камеры
    UpdateCamera(deltaTime);

    float t = std::chrono::duration<float>(now - g_startTime).count();

    // Мировая матрица (куб вращается)
    XMMATRIX world = XMMatrixRotationY(t) * XMMatrixRotationX(t * 0.6f);

    // Матрица вида из камеры
    XMVECTOR eyePos = XMLoadFloat3(&g_camera.position);
    float cosPitch = cosf(g_camera.pitch);
    float sinPitch = sinf(g_camera.pitch);
    float cosYaw = cosf(g_camera.yaw);
    float sinYaw = sinf(g_camera.yaw);
    XMVECTOR forward = XMVectorSet(cosPitch * sinYaw, sinPitch, cosPitch * cosYaw, 0.0f);
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMVECTOR at = XMVectorAdd(eyePos, forward);
    XMMATRIX view = XMMatrixLookAtLH(eyePos, at, up);

    // Проекция
    float aspect = g_viewport.Width / g_viewport.Height;
    XMMATRIX proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), aspect, 0.1f, 100.0f);

    XMMATRIX worldViewProj = world * view * proj;

    // Обновление константного буфера
    XMStoreFloat4x4(&g_cbMapped->mvp, XMMatrixTranspose(worldViewProj));
    XMStoreFloat4x4(&g_cbMapped->world, XMMatrixTranspose(world));
    XMStoreFloat3(&g_cbMapped->eyePos, eyePos);
    g_cbMapped->lightPos = XMFLOAT3(2.0f * cosf(t), 1.5f, 2.0f * sinf(t));
    g_cbMapped->lightColor = XMFLOAT3(1.0f, 1.0f, 1.0f);
    g_cbMapped->ambient = 0.12f;

    // Привязка корневой сигнатуры и константного буфера
    g_commandList->SetGraphicsRootSignature(g_rootSignature.Get());
    g_commandList->SetGraphicsRootConstantBufferView(0, g_cbUpload->GetGPUVirtualAddress());

    g_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    g_commandList->IASetVertexBuffers(0, 1, &g_vbView);
    g_commandList->IASetIndexBuffer(&g_ibView);
    g_commandList->DrawIndexedInstanced(g_indexCount, 1, 0, 0, 0);

    std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
    g_commandList->ResourceBarrier(1, &barrier);

    HR(g_commandList->Close(), "CommandList Close failed (draw)");
    ID3D12CommandList* lists[] = { g_commandList.Get() };
    g_commandQueue->ExecuteCommandLists(1, lists);

    g_swapChain->Present(1, 0);
    MoveToNextFrame();
}

// ============================================================
// Оконный код Win32
// ============================================================
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_KEYDOWN:
        if (wParam < 256) g_keys[wParam] = true;
        if (wParam == VK_ESCAPE) PostQuitMessage(0);
        return 0;
    case WM_KEYUP:
        if (wParam < 256) g_keys[wParam] = false;
        return 0;
    case WM_MOUSEMOVE:
    {
        if (g_ignoreMouseMove)
        {
            g_ignoreMouseMove = false;
            return 0;
        }
        int x = LOWORD(lParam);
        int y = HIWORD(lParam);
        if (g_mouseCaptured)
        {
            g_mouseDeltaX = x - g_lastMouseX;
            g_mouseDeltaY = y - g_lastMouseY;

            // Возвращаем курсор в центр окна
            POINT centerPoint = { g_centerX, g_centerY };
            ClientToScreen(g_hWnd, &centerPoint);
            g_ignoreMouseMove = true;
            SetCursorPos(centerPoint.x, centerPoint.y);
            g_lastMouseX = g_centerX;
            g_lastMouseY = g_centerY;
        }
        else
        {
            g_lastMouseX = x;
            g_lastMouseY = y;
        }
        return 0;
    }
    case WM_LBUTTONDOWN:
    {
        g_mouseCaptured = true;
        SetCapture(g_hWnd);
        ShowCursor(FALSE);

        // Вычисляем центр области
        RECT clientRect;
        GetClientRect(g_hWnd, &clientRect);
        g_centerX = (clientRect.right - clientRect.left) / 2;
        g_centerY = (clientRect.bottom - clientRect.top) / 2;
        POINT centerPoint = { g_centerX, g_centerY };
        ClientToScreen(g_hWnd, &centerPoint);
        SetCursorPos(centerPoint.x, centerPoint.y);
        g_lastMouseX = g_centerX;
        g_lastMouseY = g_centerY;
        g_mouseDeltaX = 0;
        g_mouseDeltaY = 0;
        return 0;
    }
    case WM_LBUTTONUP:
        g_mouseCaptured = false;
        ReleaseCapture();
        ShowCursor(TRUE);
        return 0;
    case WM_SIZE:
    {
        // При изменении размера окна обновляем центр, если мышь захвачена
        if (g_mouseCaptured)
        {
            RECT clientRect;
            GetClientRect(g_hWnd, &clientRect);
            g_centerX = (clientRect.right - clientRect.left) / 2;
            g_centerY = (clientRect.bottom - clientRect.top) / 2;
            POINT centerPoint = { g_centerX, g_centerY };
            ClientToScreen(g_hWnd, &centerPoint);
            SetCursorPos(centerPoint.x, centerPoint.y);
            g_lastMouseX = g_centerX;
            g_lastMouseY = g_centerY;
        }
        return 0;
    }
    case WM_CLOSE:
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool InitWindow(HINSTANCE hInstance, int nCmdShow)
{
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"MyWindowClass";

    if (!RegisterClassEx(&wc))
        return false;

    RECT rect = { 0, 0, 800, 600 };
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

    g_hWnd = CreateWindowEx(0, L"MyWindowClass", L"DX12 Cube (Phong + free camera)", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top,
        nullptr, nullptr, hInstance, nullptr);
    if (!g_hWnd) return false;

    ShowWindow(g_hWnd, nCmdShow);
    return true;
}

int Run()
{
    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            Draw();
        }
    }
    return (int)msg.wParam;
}

// ============================================================
// Точка входа
// ============================================================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, PSTR, int nCmdShow)
{
    // Инициализируем динамическую загрузку D3DCompile
    if (!InitD3DCompile())
    {
        MessageBoxA(nullptr, "Failed to load d3dcompiler_47.dll", "Error", MB_ICONERROR);
        return 1;
    }

    if (!InitWindow(hInstance, nCmdShow)) return 0;
    if (!InitD3D()) return 0;
    if (!InitSwapChainAndRTV()) return 0;
    if (!InitDepthBuffer()) return 0;
    if (!InitConstantBuffer()) return 0;
    if (!CreatePipeline()) FailFast("CreatePipeline failed");
    if (!CreateCubeBuffers()) FailFast("CreateCubeBuffers failed");

    int code = Run();

    WaitForGPU();
    if (g_fenceEvent) CloseHandle(g_fenceEvent);
    return code;
}