#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl.h>
#include <wincodec.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "windowscodecs.lib")

using Microsoft::WRL::ComPtr;

ComPtr<ID3D11Device> device;
ComPtr<ID3D11DeviceContext> context;
ComPtr<IDXGIOutputDuplication> outputDuplication;

void InitializeDirect3D()
{
    D3D_FEATURE_LEVEL featureLevel;
    D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &device, &featureLevel, &context);

    ComPtr<IDXGIDevice> dxgiDevice;
    device.As(&dxgiDevice);

    ComPtr<IDXGIAdapter> dxgiAdapter;
    dxgiDevice->GetAdapter(&dxgiAdapter);

    ComPtr<IDXGIOutput> dxgiOutput;
    dxgiAdapter->EnumOutputs(0, &dxgiOutput);

    ComPtr<IDXGIOutput1> dxgiOutput1;
    dxgiOutput.As(&dxgiOutput1);
    dxgiOutput1->DuplicateOutput(device.Get(), &outputDuplication);
}

ComPtr<ID3D11Texture2D> CaptureScreen()
{
    ComPtr<IDXGIResource> desktopResource;
    DXGI_OUTDUPL_FRAME_INFO frameInfo;

    // Capture frame
    outputDuplication->AcquireNextFrame(500, &frameInfo, &desktopResource);

    // Get the texture containing the screen
    ComPtr<ID3D11Texture2D> screenTexture;
    desktopResource.As(&screenTexture);

    outputDuplication->ReleaseFrame();

    return screenTexture; // Return the captured texture
}

void SaveTextureToFile(ComPtr<ID3D11Texture2D> screenTexture, const wchar_t *filePath)
{
    // Initialize WIC Factory
    ComPtr<IWICImagingFactory> wicFactory;
    CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wicFactory));

    // Create WIC stream to save the image
    ComPtr<IWICStream> wicStream;
    wicFactory->CreateStream(&wicStream);
    wicStream->InitializeFromFilename(filePath, GENERIC_WRITE);

    // Create an encoder (JPEG or PNG)
    ComPtr<IWICBitmapEncoder> wicEncoder;
    wicFactory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &wicEncoder);
    wicEncoder->Initialize(wicStream.Get(), WICBitmapEncoderNoCache);

    // Create a frame and set its properties
    ComPtr<IWICBitmapFrameEncode> frameEncode;
    wicEncoder->CreateNewFrame(&frameEncode, nullptr);
    frameEncode->Initialize(nullptr);

    // Convert the Direct3D texture to WIC-compatible format and copy the data
    // You would map the texture and transfer it to the frame (simplified here)

    frameEncode->Commit();
    wicEncoder->Commit();
}
