#include <d3d11.h>
#include <d3d11on12.h>
#include <d3d12.h>
#include <dxgi1_4.h> // For swap chain creation
#include <iostream>
#include <vector>

// Function to dynamically load D3D11On12CreateDevice
static PFN_D3D11ON12_CREATE_DEVICE getPfnD3D11On12CreateDevice() {
    static PFN_D3D11ON12_CREATE_DEVICE fptr = nullptr;

    if (fptr)
        return fptr;

    HMODULE d3d11module = LoadLibraryA("d3d11.dll"); // Use default system DLL

    if (!d3d11module) {
        std::cerr << "Failed to load d3d11.dll" << std::endl;
        return nullptr;
    }

    fptr = reinterpret_cast<PFN_D3D11ON12_CREATE_DEVICE>(
        reinterpret_cast<void*>(GetProcAddress(d3d11module, "D3D11On12CreateDevice"))
    );

    if (!fptr) {
        std::cerr << "Failed to get D3D11On12CreateDevice function pointer" << std::endl;
    }

    return fptr;
}

// Custom D3D11CreateDevice implementation
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
    ID3D11DeviceContext** ppImmediateContext)
{
    (void)DriverType;  // Mark as unused
    (void)Software;    // Mark as unused
    (void)SDKVersion;  // Mark as unused

    if (!ppDevice || !ppImmediateContext) {
        return E_INVALIDARG;
    }

    // Create a D3D12 device
    ID3D12Device* d3d12device = nullptr;
    std::vector<D3D_FEATURE_LEVEL> featureLevels = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_12_0,
        D3D_FEATURE_LEVEL_12_1
    };

    HRESULT hr = D3D12CreateDevice(pAdapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&d3d12device));

    if (FAILED(hr)) {
        std::cerr << "Failed to create D3D12 device" << std::endl;
        return hr;
    }

    // Create a D3D12 command queue
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.NodeMask = 0;

    ID3D12CommandQueue* d3d12queue = nullptr;
    hr = d3d12device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&d3d12queue));

    if (FAILED(hr)) {
        std::cerr << "Failed to create D3D12 command queue" << std::endl;
        d3d12device->Release();
        return hr;
    }

    // Get the D3D11On12CreateDevice function pointer
    auto pfnD3D11on12CreateDevice = getPfnD3D11On12CreateDevice();

    if (!pfnD3D11on12CreateDevice) {
        std::cerr << "Failed to get D3D11On12CreateDevice function pointer" << std::endl;
        d3d12queue->Release();
        d3d12device->Release();
        return E_FAIL;
    }

    // Create the D3D11 device on top of the D3D12 device
    hr = (*pfnD3D11on12CreateDevice)(
        d3d12device,
        Flags,
        pFeatureLevels,
        FeatureLevels,
        reinterpret_cast<IUnknown**>(&d3d12queue),
        1,
        0,
        ppDevice,
        ppImmediateContext,
        pFeatureLevel
    );

    if (FAILED(hr)) {
        std::cerr << "Failed to create D3D11 device on D3D12" << std::endl;
        d3d12queue->Release();
        d3d12device->Release();
        return hr;
    }

    // Release D3D12 resources (they are now managed by D3D11On12)
    d3d12queue->Release();
    d3d12device->Release();

    return S_OK;
}

// Custom D3D11CreateDeviceAndSwapChain implementation
HRESULT __stdcall D3D11CreateDeviceAndSwapChain(
    IDXGIAdapter* pAdapter,
    D3D_DRIVER_TYPE DriverType,
    HMODULE Software,
    UINT Flags,
    const D3D_FEATURE_LEVEL* pFeatureLevels,
    UINT FeatureLevels,
    UINT SDKVersion,
    DXGI_SWAP_CHAIN_DESC* pSwapChainDesc,  // Removed const
    IDXGISwapChain** ppSwapChain,
    ID3D11Device** ppDevice,
    D3D_FEATURE_LEVEL* pFeatureLevel,
    ID3D11DeviceContext** ppImmediateContext)
{
    if (ppSwapChain && !pSwapChainDesc) {
        return E_INVALIDARG;
    }

    // Create the D3D11 device
    ID3D11Device* d3d11Device = nullptr;
    ID3D11DeviceContext* d3d11Context = nullptr;

    HRESULT hr = D3D11CreateDevice(
        pAdapter, DriverType, Software, Flags,
        pFeatureLevels, FeatureLevels, SDKVersion,
        &d3d11Device, pFeatureLevel, &d3d11Context
    );

    if (FAILED(hr)) {
        std::cerr << "Failed to create D3D11 device" << std::endl;
        return hr;
    }

    // Create the swap chain if requested
    if (ppSwapChain) {
        IDXGIDevice* dxgiDevice = nullptr;
        IDXGIAdapter* dxgiAdapter = nullptr;
        IDXGIFactory* dxgiFactory = nullptr;

        hr = d3d11Device->QueryInterface(IID_PPV_ARGS(&dxgiDevice));
        if (FAILED(hr)) {
            std::cerr << "Failed to get DXGI device" << std::endl;
            d3d11Device->Release();
            return hr;
        }

        hr = dxgiDevice->GetParent(IID_PPV_ARGS(&dxgiAdapter));
        dxgiDevice->Release();

        if (FAILED(hr)) {
            std::cerr << "Failed to get DXGI adapter" << std::endl;
            d3d11Device->Release();
            return hr;
        }

        hr = dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory));
        dxgiAdapter->Release();

        if (FAILED(hr)) {
            std::cerr << "Failed to get DXGI factory" << std::endl;
            d3d11Device->Release();
            return hr;
        }

        hr = dxgiFactory->CreateSwapChain(d3d11Device, pSwapChainDesc, ppSwapChain);
        dxgiFactory->Release();

        if (FAILED(hr)) {
            std::cerr << "Failed to create swap chain" << std::endl;
            d3d11Device->Release();
            return hr;
        }
    }

    // Return the device and context
    if (ppDevice) {
        *ppDevice = d3d11Device;
    } else {
        d3d11Device->Release();
    }

    if (ppImmediateContext) {
        *ppImmediateContext = d3d11Context;
    } else {
        d3d11Context->Release();
    }

    return S_OK;
}
