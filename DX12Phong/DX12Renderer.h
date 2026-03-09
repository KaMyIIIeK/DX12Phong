#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

#include <vector>
#include <cstdint>
#include <cstring>
#include <chrono>

class Camera;

class DX12Renderer
{
public:
    bool Initialize(HWND hwnd, unsigned int width, unsigned int height);
    void Render(const Camera& camera);
    void Shutdown();

private:
    bool InitD3D();
    bool InitD3DCompile();
    bool InitSwapChainAndRTV(HWND hwnd, unsigned int width, unsigned int height);
    bool InitDepthBuffer(unsigned int width, unsigned int height);
    bool InitConstantBuffer();
    bool CreatePipeline();
    bool CreateCubeBuffers();

    void WaitForGPU();
    void MoveToNextFrame();

    static void FailFast(const char* msg);
    static D3D12_RESOURCE_DESC BufferDesc(UINT64 byteSize);

    void CreateDefaultBuffer(
        const void* initData,
        UINT64 byteSize,
        Microsoft::WRL::ComPtr<ID3D12Resource>& defaultBuffer,
        Microsoft::WRL::ComPtr<ID3D12Resource>& uploadBuffer);

    bool CompileShader(
        const char* src,
        const char* entry,
        const char* target,
        Microsoft::WRL::ComPtr<ID3DBlob>& outBlob);

    struct Vertex
    {
        float px, py, pz;
        float nx, ny, nz;
        float r, g, b;
    };

    void BuildCubeGeometry(std::vector<Vertex>& outVerts, std::vector<uint16_t>& outIndices);

    struct alignas(256) CBData
    {
        DirectX::XMFLOAT4X4 mvp;
        DirectX::XMFLOAT4X4 world;
        DirectX::XMFLOAT3 lightPos;   float _pad0;
        DirectX::XMFLOAT3 eyePos;     float _pad1;
        DirectX::XMFLOAT3 lightColor; float _pad2;
        float ambient;                float _pad3[3];
    };

    typedef HRESULT(WINAPI* D3DCompileFunc)(
        LPCVOID pSrcData, SIZE_T SrcDataSize, LPCSTR pSourceName,
        const D3D_SHADER_MACRO* pDefines, ID3DInclude* pInclude,
        LPCSTR pEntrypoint, LPCSTR pTarget, UINT Flags1, UINT Flags2,
        ID3DBlob** ppCode, ID3DBlob** ppErrorMsgs);

private:
    HWND m_hWnd = nullptr;

    Microsoft::WRL::ComPtr<IDXGIFactory4> m_dxgiFactory;
    Microsoft::WRL::ComPtr<ID3D12Device> m_device;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_commandAllocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;

    Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValue = 0;
    HANDLE m_fenceEvent = nullptr;

    static const UINT m_frameCount = 2;
    Microsoft::WRL::ComPtr<IDXGISwapChain3> m_swapChain;
    UINT m_frameIndex = 0;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    UINT m_rtvDescriptorSize = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_renderTargets[m_frameCount];
    D3D12_CPU_DESCRIPTOR_HANDLE m_rtvHandles[m_frameCount]{};

    D3D12_VIEWPORT m_viewport{};
    D3D12_RECT m_scissorRect{};

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_depthBuffer;
    DXGI_FORMAT m_depthFormat = DXGI_FORMAT_D32_FLOAT;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_indexBuffer;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexUpload;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_indexUpload;

    D3D12_VERTEX_BUFFER_VIEW m_vbView{};
    D3D12_INDEX_BUFFER_VIEW m_ibView{};
    UINT m_indexCount = 0;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pso;

    Microsoft::WRL::ComPtr<ID3D12Resource> m_cbUpload;
    CBData* m_cbMapped = nullptr;
    UINT64 m_cbSize = 0;

    std::chrono::steady_clock::time_point m_startTime;
    D3DCompileFunc m_D3DCompile = nullptr;
};