// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"


void StartRecording()
{
    while (1)
    {
        Sleep(3);
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

        CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)StartRecording, nullptr, 0, nullptr);
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

