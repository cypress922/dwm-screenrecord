#include "pch.h"


#include "portable_executable.h"
#include "utils.h"
#include "globals.h"

using namespace Microsoft::WRL;

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

#define WIDTH 1920
#define HEIGHT 1080
#define FPS 60
#define DURATION_SECONDS 10
#define OUTPUT_FILE L"C:\\Users\\Datur\\Videos\\output.mp4"

ComPtr<ID3D11Device>           g_d3dDevice;
ComPtr<ID3D11DeviceContext>    g_d3dContext;
ComPtr<IDXGIOutputDuplication> g_duplication;
ComPtr<IMFSinkWriter>          g_sinkWriter;
DWORD                          g_streamIndex;



bool InitD3DAndDuplication()
{
    D3D_FEATURE_LEVEL level;
    D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0,
        D3D11_SDK_VERSION, &g_d3dDevice, &level, &g_d3dContext);

    ComPtr<IDXGIDevice> dxgiDevice;
    g_d3dDevice.As(&dxgiDevice);

    ComPtr<IDXGIAdapter> adapter;

    dxgiDevice->GetAdapter(&adapter);

    ComPtr<IDXGIOutput> output;


    DXGI_ADAPTER_DESC desc;
    adapter->GetDesc(&desc);
   
   // Utils::DebugLog(L"dxgi adapter description: %ws", desc.Description);

    HRESULT hr = adapter->EnumOutputs(0, &output);

    if (FAILED(hr)) {
 //       Utils::DebugLog(L"EnumOutputs failed: 0x%08X\n", hr);
    }

    ComPtr<IDXGIOutput1> output1;
    output.As(&output1);

    output1->DuplicateOutput(g_d3dDevice.Get(), &g_duplication);
    return true;
}

bool InitMediaFoundation()
{
    MFStartup(MF_VERSION);

    ComPtr<IMFAttributes> attr;
    MFCreateAttributes(&attr, 1);
    auto hr = MFCreateSinkWriterFromURL(OUTPUT_FILE, nullptr, attr.Get(), &g_sinkWriter);

    if (FAILED(hr)) {
        char errorMsg[256];
        StringCchPrintfA(errorMsg, sizeof(errorMsg),
            "Failed MFCreateSinkWriterFromURL!\nHRESULT: 0x%08X", hr);

        MessageBoxA(NULL, errorMsg, "capturedll", MB_OK | MB_ICONERROR);
    }
    ComPtr<IMFMediaType> mediaTypeOut;
    MFCreateMediaType(&mediaTypeOut);
    MessageBoxA(NULL, "MFCreateMediaType called!!", "capturedll", MB_OK);



    mediaTypeOut->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    mediaTypeOut->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
    mediaTypeOut->SetUINT32(MF_MT_AVG_BITRATE, 8000000);
    mediaTypeOut->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);


    MessageBoxA(NULL, "MediaTypeOUt inited!!", "capturedll", MB_OK);

    MFSetAttributeSize(mediaTypeOut.Get(), MF_MT_FRAME_SIZE, WIDTH, HEIGHT);
    MFSetAttributeRatio(mediaTypeOut.Get(), MF_MT_FRAME_RATE, FPS, 1);
    MFSetAttributeRatio(mediaTypeOut.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);


    MessageBoxA(NULL, "MFSetAttributeRatio called 111111!", "capturedll", MB_OK);


    g_sinkWriter->AddStream(mediaTypeOut.Get(), &g_streamIndex);
    MessageBoxA(NULL, "g_sinkWriter->AddStream called!!", "capturedll", MB_OK);

    ComPtr<IMFMediaType> mediaTypeIn;
    MFCreateMediaType(&mediaTypeIn);
    mediaTypeIn->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
    mediaTypeIn->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_ARGB32);

    MessageBoxA(NULL, "mediaTypeIn->SetGUID called!!", "capturedll", MB_OK);


    MFSetAttributeSize(mediaTypeIn.Get(), MF_MT_FRAME_SIZE, WIDTH, HEIGHT);
    MFSetAttributeRatio(mediaTypeIn.Get(), MF_MT_FRAME_RATE, FPS, 1);
    MFSetAttributeRatio(mediaTypeIn.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);

    MessageBoxA(NULL, "MFSetAttributeRatio called!!", "capturedll", MB_OK);

    mediaTypeIn->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    g_sinkWriter->SetInputMediaType(g_streamIndex, mediaTypeIn.Get(), nullptr);

    MessageBoxA(NULL, "g_sinkWriter->SetInputMediaType called!!", "capturedll", MB_OK);


    g_sinkWriter->BeginWriting();
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
                OutputDebugString(L"Failed to create staging texture\n");
                return;
            }
            
            D3D11_MAPPED_SUBRESOURCE mapped;

            g_d3dContext->CopyResource(stagingTex.Get(), acquiredTex.Get());

            hr = g_d3dContext->Map(stagingTex.Get(), 0, D3D11_MAP_READ, 0, &mapped);
            
            if (FAILED(hr)) {
                OutputDebugString(L"Map failed on staging texture\n");
                return;
            }

            ComPtr<IMFSample> sample;
            MFCreateSample(&sample);

            ComPtr<IMFMediaBuffer> buffer;
            MFCreateMemoryBuffer(WIDTH * HEIGHT * 4, &buffer);

            BYTE* pData = nullptr;
            DWORD maxLen = 0;
            buffer->Lock(&pData, &maxLen, nullptr);



            for (UINT y = 0; y < HEIGHT; ++y)
            {
                memcpy(
                    pData + y * WIDTH * 4,                            // dest row (bottom-up)
                    (BYTE*)mapped.pData + (HEIGHT - 1 - y) * mapped.RowPitch,  // src row (top-down)
                    WIDTH * 4
                );
            }

            buffer->Unlock();
            buffer->SetCurrentLength(WIDTH * HEIGHT * 4);

            sample->AddBuffer(buffer.Get());

            auto now = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - startTime).count();
            LONGLONG sampleTime = elapsed * 10; // convert microseconds to 100-nanosecond units

            sample->SetSampleTime(sampleTime);
            sample->SetSampleDuration(frameDuration); // keep duration fixed


            g_sinkWriter->WriteSample(g_streamIndex, sample.Get()); 

            g_d3dContext->Unmap(stagingTex.Get(), 0);
            g_duplication->ReleaseFrame();
            ++frameCount;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1000 / FPS));
    }

    g_sinkWriter->Finalize();
    MFShutdown();
}


void StartRecording()
{
    InitD3DAndDuplication();
    InitMediaFoundation();

    MessageBoxA(NULL, "media found!!", "capturedll", MB_OK);

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

