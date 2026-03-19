#ifndef BUFFER_H_
#define BUFFER_H_

#include "d3dUtil.h"
#include "throw_if_failed.h"
#include "object_constants.h"
#include <iostream>

using namespace Microsoft::WRL;

template<typename T>
class UploadBuffer
{
public:
    UploadBuffer(ID3D12Device* device, UINT elementCount, bool isConstantBuffer) :
        mIsConstantBuffer(isConstantBuffer)
    {
        mElementByteSize = sizeof(T);

        if (isConstantBuffer)
            mElementByteSize = d3dUtil::CalcConstantBufferSize(sizeof(T));
        CD3DX12_HEAP_PROPERTIES heapPr = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        CD3DX12_RESOURCE_DESC rd = CD3DX12_RESOURCE_DESC::Buffer(mElementByteSize * elementCount);
        ThrowIfFailed(device->CreateCommittedResource(
            &heapPr,
            D3D12_HEAP_FLAG_NONE,
            &rd,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&mUploadBuffer)));

        ThrowIfFailed(mUploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mMappedData)));
        std::cout << "Constant buffer is created" << std::endl;
    }

    UploadBuffer(const UploadBuffer& rhs) = delete;
    UploadBuffer& operator=(const UploadBuffer& rhs) = delete;
    ~UploadBuffer()
    {
        if (mUploadBuffer != nullptr)
            mUploadBuffer->Unmap(0, nullptr);

        mMappedData = nullptr;
    }

    ID3D12Resource* Resource()const
    {
        return mUploadBuffer.Get();
    }

    void CopyData(int elementIndex, const T& data)
    {
        memcpy(&mMappedData[elementIndex * mElementByteSize], &data, sizeof(T));
    }

private:
    ComPtr<ID3D12Resource> mUploadBuffer;
    BYTE* mMappedData = nullptr;

    UINT mElementByteSize = 0;
    bool mIsConstantBuffer = false;
};

#endif //UPLOADBUFFER_H_