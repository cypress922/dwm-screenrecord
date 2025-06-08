#include "pch.h"

#include "portable_executable.h"
#include "utils.h"
#include "globals.h"

using namespace Microsoft::WRL;

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

#define WIDTH 1920
#define HEIGHT 1080
#define FPS 60
#define DURATION_SECONDS 10
// #define OUTPUT_FILE L"C:\\Users\\Datur\\Videos\\output.mp4"

ComPtr<ID3D11Device>           g_d3dDevice;
ComPtr<ID3D11DeviceContext>    g_d3dContext;
ComPtr<IDXGIOutputDuplication> g_duplication;
DWORD                          g_streamIndex;



bool InitD3DAndDuplication()
{
    D3D_FEATURE_LEVEL level;
    auto hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0,
        D3D11_SDK_VERSION, &g_d3dDevice, &level, &g_d3dContext);

    if (FAILED(hr)) {
        MessageBoxA(NULL, "D3D11CreateDevice failed!!", "capturedll", MB_OK);
    }

    ComPtr<IDXGIDevice> dxgiDevice;
    g_d3dDevice.As(&dxgiDevice);

    ComPtr<IDXGIAdapter> adapter;

    dxgiDevice->GetAdapter(&adapter);

    ComPtr<IDXGIOutput> output;


    DXGI_ADAPTER_DESC desc;
    adapter->GetDesc(&desc);
   
    hr = adapter->EnumOutputs(0, &output);

    if (FAILED(hr)) {
 //       Utils::DebugLog(L"EnumOutputs failed: 0x%08X\n", hr);
    }

    ComPtr<IDXGIOutput1> output1;
    output.As(&output1);

    output1->DuplicateOutput(g_d3dDevice.Get(), &g_duplication);
    return true;
}

bool SaveFrameAsBMP(int frameIndex, BYTE* data, int width, int height)
{
    char filename[256];
    sprintf_s(filename, "C:\\Windows\\Temp\\frame_%04d.bmp", frameIndex);

    DWORD headersSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    DWORD imageSize = width * height * 4;
    DWORD totalSize = headersSize + imageSize;

    HANDLE hFile = CreateFileA(filename, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);



    if (hFile == INVALID_HANDLE_VALUE)
    {
        DWORD err = GetLastError();

        char msg[256];
        sprintf_s(msg, sizeof(msg), "Invalid Handle Val!!\nGetLastError = %lu (0x%08lX)", err, err);

        MessageBoxA(NULL, msg, "capturedll", MB_OK | MB_ICONERROR);
        return false;
    }

    BITMAPFILEHEADER fileHeader = { 0 };
    fileHeader.bfType = 0x4D42; // 'BM'
    fileHeader.bfSize = totalSize;
    fileHeader.bfOffBits = headersSize;

    BITMAPINFOHEADER infoHeader = { 0 };
    infoHeader.biSize = sizeof(BITMAPINFOHEADER);
    infoHeader.biWidth = width;
    infoHeader.biHeight = -height; // negative to store top-down
    infoHeader.biPlanes = 1;
    infoHeader.biBitCount = 32;
    infoHeader.biCompression = BI_RGB;

    DWORD written;
    WriteFile(hFile, &fileHeader, sizeof(fileHeader), &written, nullptr);
    WriteFile(hFile, &infoHeader, sizeof(infoHeader), &written, nullptr);
    WriteFile(hFile, data, imageSize, &written, nullptr);
    CloseHandle(hFile);

    return true;
}


void RecordLoop()
{
    using clock = std::chrono::high_resolution_clock;
    auto startTime = clock::now();

    UINT64 frameDuration = 10'000'000 / FPS;
    int frameCount = 0;

    while (std::chrono::duration_cast<std::chrono::seconds>(clock::now() - startTime).count() < DURATION_SECONDS)
    {
        ComPtr<IDXGIResource> desktopResource;
        DXGI_OUTDUPL_FRAME_INFO frameInfo;
        if (SUCCEEDED(g_duplication->AcquireNextFrame(100, &frameInfo, &desktopResource)))
        {
            ComPtr<ID3D11Texture2D> acquiredTex;
            desktopResource.As(&acquiredTex);

            D3D11_TEXTURE2D_DESC desc;
            acquiredTex->GetDesc(&desc);

            D3D11_TEXTURE2D_DESC stagingDesc = desc;
            stagingDesc.Usage = D3D11_USAGE_STAGING;
            stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            stagingDesc.BindFlags = 0;
            stagingDesc.MiscFlags = 0;
            stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

            ComPtr<ID3D11Texture2D> stagingTex;


            HRESULT hr = g_d3dDevice->CreateTexture2D(&stagingDesc, nullptr, &stagingTex);

            if (FAILED(hr)) {
                MessageBoxA(NULL, "Failed to create staging texture!!", "capturedll", MB_OK);
                return;
            }
            
            D3D11_MAPPED_SUBRESOURCE mapped;

            g_d3dContext->CopyResource(stagingTex.Get(), acquiredTex.Get());

            hr = g_d3dContext->Map(stagingTex.Get(), 0, D3D11_MAP_READ, 0, &mapped);

            if (FAILED(hr)) {
                MessageBoxA(NULL, "Map failed on staging texture!!", "capturedll", MB_OK);
                return;
            }

            DWORD maxLen = 0;

            BYTE* pData = new BYTE[WIDTH * HEIGHT * 4]; // Allocate enough memory for the whole frame

            for (UINT y = 0; y < HEIGHT; ++y)
            {
                memcpy(
                    pData + y * WIDTH * 4,                            // dest row (bottom-up)
                    (BYTE*)mapped.pData + (HEIGHT - 1 - y) * mapped.RowPitch,  // src row (top-down)
                    WIDTH * 4
                );
            }

            auto now = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - startTime).count();
            LONGLONG sampleTime = elapsed * 10; // convert microseconds to 100-nanosecond units

            SaveFrameAsBMP(frameCount, pData, WIDTH, HEIGHT);

            delete[] pData;

            g_d3dContext->Unmap(stagingTex.Get(), 0);
            g_duplication->ReleaseFrame();
            ++frameCount;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1000 / FPS));
    }
}


void StartRecording()
{
    InitD3DAndDuplication();
    RecordLoop();
}

__declspec(dllexport) DllParams* Global::dll_params = 0;

bool entry_not_called = true;

extern "C" __declspec(dllexport) void ManualMapEntry(DllParams* parameter)
{
    if (entry_not_called)
    {
        entry_not_called = false;

        Global::dll_params = parameter;

        PE::ResolveImports((uint8_t*)Global::dll_params->payload_dll_base);

        StartRecording();
    }

    return;
}



BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
//        DisableThreadLibraryCalls(hModule); // optional, avoids thread attach/detach calls
        CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)StartRecording, nullptr, 0, nullptr);
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

