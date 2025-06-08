#pragma once
#include "utils.h"

#define PE_HEADER(image) ((IMAGE_NT_HEADERS64*)((uint64_t)image + ((IMAGE_DOS_HEADER*)image)->e_lfanew))

namespace PE
{
	bool ResolveImports(uint8_t* base)
	{
		auto pe_hdr = PE_HEADER(base);


		auto rva = pe_hdr->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;

		if (!rva)
		{
			return false;
		}

		auto import_desc = (IMAGE_IMPORT_DESCRIPTOR*)(base + rva);

		for (; import_desc->FirstThunk; ++import_desc)
		{
			auto mod_name = (char*)(base + import_desc->Name);

			//Utils::DebugLog(L"loading module %s \n", mod_name);

			if (!mod_name)
			{
				MessageBoxA(NULL, "module load failed!!!!", "capturedll", MB_OK);

				break;
			}

			auto module = LoadLibraryA(mod_name);

//			MessageBoxA(NULL, "module load success!!!!", "capturedll", MB_OK);

			for (auto of_thunk = (IMAGE_THUNK_DATA*)(base + import_desc->OriginalFirstThunk),
				thunk = (IMAGE_THUNK_DATA*)(base + import_desc->FirstThunk);
				of_thunk->u1.AddressOfData;
				++of_thunk, ++thunk)
			{
				auto import_by_name = (IMAGE_IMPORT_BY_NAME*)(base + of_thunk->u1.AddressOfData);

				if (Utils::IsAddressValid(import_by_name))
				{
					thunk->u1.Function = (uintptr_t)GetProcAddress(module, import_by_name->Name);
				}
				else
				{
					thunk->u1.Function = NULL;
				}
			}
		}

		return true;
	}
}