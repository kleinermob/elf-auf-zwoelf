#include <d3d11.h>
#include <d3d11on12.h>
#include <d3d12.h>
#include <dxgi1_4.h> // For IDXGIFactory4 and IDXGISwapChain3
#include <wrl/client.h> // For ComPtr
#include <iostream>

using Microsoft::WRL::ComPtr;

static PFN_D3D11ON12_CREATE_DEVICE GetPfnD3D11On12CreateDevice() {
    static PFN_D3D11ON12_CREATE_DEVICE fptr = nullptr;

    if (fptr)
        return fptr;

    HMODULE d3d11module = LoadLibraryA("C:\\Windows\\System32\\d3d11.dll");

    if (!d3d11module)
        return nullptr;

    fptr = reinterpret_cast<PFN_D3D11ON12_CREATE_DEVICE>(GetProcAddress(d3d11module, "D3D11On12CreateDevice"));
    return fptr;
}

HRESULT CreateD3D12Device(IDXGIAdapter* pAdapter, ComPtr<ID3D12Device>& d3d12Device) {
    constexpr D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0,
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0
    };

    return D3D12CreateDevice(pAdapter, featureLevels[0], IID_PPV_ARGS(&d3d12Device));
}

HRESULT CreateD3D12CommandQueue(ID3D12Device* d3d12Device, ComPtr<ID3D12CommandQueue>& d3d12Queue) {
    D3D12_COMMAND_QUEUE_DESC desc = {};
    desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    desc.NodeMask = 0;

    return d3d12Device->CreateCommandQueue(&desc, IID_PPV_ARGS(&d3d12Queue));
}

HRESULT __stdcall D3D11CreateDevice(
    IDXGIAdapter* pAdapter,
    D3D_DRIVER_TYPE DriverType,
    HMODULE Software,
    UINT Flags,
    const D3D_FEATURE_LEVEL* pFeatureLevels,
    UINT FeatureLevels,
    UINT SDKVersion,
    ID3D11Device** ppDevice,
    D3D_FEATURE_LEVEL* pFeatureLevel,
    ID3D11DeviceContext** ppImmediateContext) {

    ComPtr<ID3D12Device> d3d12Device;
    HRESULT hr = CreateD3D12Device(pAdapter, d3d12Device);
    if (FAILED(hr)) {
        std::cerr << "Failed to create D3D12 device. HRESULT: " << hr << std::endl;
        return hr;
    }

    ComPtr<ID3D12CommandQueue> d3d12Queue;
    hr = CreateD3D12CommandQueue(d3d12Device.Get(), d3d12Queue);
    if (FAILED(hr)) {
        std::cerr << "Failed to create D3D12 command queue. HRESULT: " << hr << std::endl;
        return hr;
    }

    auto pfnD3D11on12CreateDevice = GetPfnD3D11On12CreateDevice();
    if (!pfnD3D11on12CreateDevice) {
        std::cerr << "Failed to get D3D11On12CreateDevice function pointer." << std::endl;
        return E_FAIL;
    }

    Flags |= D3D11_CREATE_DEVICE_BGRA_SUPPORT;

    hr = (*pfnD3D11on12CreateDevice)(d3d12Device.Get(), Flags, pFeatureLevels, FeatureLevels,
        reinterpret_cast<IUnknown**>(d3d12Queue.GetAddressOf()), 1, 0, ppDevice, ppImmediateContext, pFeatureLevel);

    if (FAILED(hr)) {
        std::cerr << "Failed to create D3D11 device on D3D12. HRESULT: " << hr << std::endl;
    }

    return hr;
}

HRESULT __stdcall D3D11CreateDeviceAndSwapChain(
    IDXGIAdapter* pAdapter,
    D3D_DRIVER_TYPE DriverType,
    HMODULE Software,
    UINT Flags,
    const D3D_FEATURE_LEVEL* pFeatureLevels,
    UINT FeatureLevels,
    UINT SDKVersion,
    const DXGI_SWAP_CHAIN_DESC* pSwapChainDesc,
    IDXGISwapChain** ppSwapChain,
    ID3D11Device** ppDevice,
    D3D_FEATURE_LEVEL* pFeatureLevel,
    ID3D11DeviceContext** ppImmediateContext) {

    if (ppSwapChain && !pSwapChainDesc)
        return E_INVALIDARG;

    ComPtr<ID3D11Device> d3d11Device;
    ComPtr<ID3D11DeviceContext> d3d11Context;

    HRESULT hr = D3D11CreateDevice(pAdapter, DriverType, Software, Flags, pFeatureLevels,
        FeatureLevels, SDKVersion, &d3d11Device, pFeatureLevel, &d3d11Context);

    if (FAILED(hr)) {
        std::cerr << "Failed to create D3D11 device. HRESULT: " << hr << std::endl;
        return hr;
    }

    if (ppSwapChain) {
        ComPtr<IDXGIDevice> dxgiDevice;
        hr = d3d11Device.As(&dxgiDevice);
        if (FAILED(hr)) {
            std::cerr << "Failed to query DXGI device. HRESULT: " << hr << std::endl;
            return hr;
        }

        ComPtr<IDXGIAdapter> dxgiAdapter;
        hr = dxgiDevice->GetParent(IID_PPV_ARGS(&dxgiAdapter));
        if (FAILED(hr)) {
            std::cerr << "Failed to get DXGI adapter. HRESULT: " << hr << std::endl;
            return hr;
        }

        ComPtr<IDXGIFactory4> dxgiFactory;
        hr = dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory));
        if (FAILED(hr)) {
            std::cerr << "Failed to get DXGI factory. HRESULT: " << hr << std::endl;
            return hr;
        }

        DXGI_SWAP_CHAIN_DESC1 desc = {};
        desc.Width = pSwapChainDesc->BufferDesc.Width;
        desc.Height = pSwapChainDesc->BufferDesc.Height;
        desc.Format = pSwapChainDesc->BufferDesc.Format;
        desc.Stereo = FALSE;
        desc.SampleDesc = pSwapChainDesc->SampleDesc;
        desc.BufferUsage = pSwapChainDesc->BufferUsage;
        desc.BufferCount = pSwapChainDesc->BufferCount;
        desc.Scaling = DXGI_SCALING_STRETCH;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
        desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

        ComPtr<IDXGISwapChain1> swapChain1;
        hr = dxgiFactory->CreateSwapChainForHwnd(
            d3d11Device.Get(), pSwapChainDesc->OutputWindow, &desc, nullptr, nullptr, &swapChain1);
        if (FAILED(hr)) {
            std::cerr << "Failed to create swap chain. HRESULT: " << hr << std::endl;
            return hr;
        }

        hr = swapChain1.As(swapChain1.GetAddressOf());
        if (FAILED(hr)) {
            std::cerr << "Failed to query modern swap chain. HRESULT: " << hr << std::endl;
            return hr;
        }

        hr = swapChain1.As(reinterpret_cast<IDXGISwapChain3**>(ppSwapChain));
        if (FAILED(hr)) {
            std::cerr << "Failed to query modern swap chain. HRESULT: " << hr << std::endl;
            return hr;
        }
    }

    if (ppDevice)
        *ppDevice = d3d11Device.Detach();
    if (ppImmediateContext)
        *ppImmediateContext = d3d11Context.Detach();

    return S_OK;
}
