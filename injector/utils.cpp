#pragma once
#include "includes.h"

#define StrToWStr(s) (std::wstring(s, &s[strlen(s)]).c_str())

namespace Util
{
	uintptr_t GetModuleBase(DWORD pid, const std::wstring& dllName)
	{
		uintptr_t baseAddress = 0;

		HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
		if (hSnapshot == INVALID_HANDLE_VALUE) {
			std::wcerr << L"Failed to create snapshot." << std::endl;
			return 0;
		}

		MODULEENTRY32W moduleEntry;
		moduleEntry.dwSize = sizeof(moduleEntry);

		if (Module32FirstW(hSnapshot, &moduleEntry)) {
			do {
				if (_wcsicmp(moduleEntry.szModule, dllName.c_str()) == 0) {
					baseAddress = reinterpret_cast<uintptr_t>(moduleEntry.modBaseAddr);
					break;
				}
			} while (Module32NextW(hSnapshot, &moduleEntry));
		}

		CloseHandle(hSnapshot);
		return baseAddress;
	}

	size_t LoadFile(const char* path, uint8_t** buffer)
	{
		auto file_handle = CreateFileA(path, GENERIC_ALL,
			0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
		);

		auto size = GetFileSize(file_handle, NULL);

		*buffer = (uint8_t*)VirtualAlloc(nullptr, 
			size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);

		SetFilePointer(file_handle, 0, 0, 0);

		uint32_t bytes;

		ReadFile(file_handle, 
			*buffer, size, (LPDWORD)&bytes, NULL);

		CloseHandle(file_handle);

		return size;
	}

	bool IsAddressValid(PVOID address)
	{
		if (((uintptr_t)address < 0x7FFFFFFFFFFF) && ((uintptr_t)address > 0x1000))
		{
			return true;
		}
		else
		{
			return false;
		}
	}
};
