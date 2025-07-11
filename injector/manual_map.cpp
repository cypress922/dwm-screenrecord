#include "manual_map.h"
#include "utils.h"

namespace PE
{
	struct RelocationEntry
	{
		uint16_t offset : 12;
		uint16_t type : 4;
	};

	int64_t	RvaToOffset(IMAGE_DOS_HEADER* base, uintptr_t rva)
	{
		auto pe_hdr = PE_HEADER(base);

		auto section_header = IMAGE_FIRST_SECTION(pe_hdr);
		auto section_count = pe_hdr->FileHeader.NumberOfSections;

		for (auto i = 0; i < section_count; i++)
		{
			if (section_header->VirtualAddress <= rva)
			{
				if ((section_header->VirtualAddress + section_header->Misc.VirtualSize) > rva)
				{
					rva -= section_header->VirtualAddress;
					rva += section_header->PointerToRawData;

					return rva;
				}
			}

			section_header++;
		}

		return -1;
	}

	void ForEachImport(
		uint8_t* image_base,
		void(*IATCallback)(
			char* module_name,
			const char* import_name,
			uintptr_t function_addr,
			void* callback_data
			),
		void* callback_data
	)
	{
		auto nt_header = PE_HEADER(image_base);

		auto import_dir = nt_header->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];

		auto import_desc = (IMAGE_IMPORT_DESCRIPTOR*)(import_dir.VirtualAddress + image_base);

		while (import_desc->Name != NULL)
		{
			auto o_first_thunk = (IMAGE_THUNK_DATA*)(image_base + import_desc->OriginalFirstThunk);

			auto first_thunk = (IMAGE_THUNK_DATA*)(image_base + import_desc->FirstThunk);

			for (o_first_thunk, first_thunk; o_first_thunk->u1.AddressOfData; ++o_first_thunk, ++first_thunk)
			{
				auto import_by_name = (IMAGE_IMPORT_BY_NAME*)(image_base + o_first_thunk->u1.AddressOfData);

				IATCallback(
					(char*)(image_base + import_desc->Name),
					import_by_name->Name,
					first_thunk->u1.Function,
					callback_data
				);
			}

			import_desc += 1;
		}
		return;
	}

	bool ResolveImports(uint8_t* base, int32_t target_pid)
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

			if (!mod_name)
			{
				break;
			}

			auto module = LoadLibraryA(mod_name);

			bool manualmap_dependency = false;

			/*	if the module isn't mapped globally, then you have to manually  */

			auto module_base = Util::GetModuleBase(target_pid,StrToWStr(mod_name));

			std::wcout << StrToWStr(mod_name) << std::endl;

			if (!module_base)
			{
				std::cout << mod_name << " base " << std::hex << module_base << std::endl;

				continue;
			}

			for (auto of_thunk = (IMAGE_THUNK_DATA*)(base + import_desc->OriginalFirstThunk),
				thunk = (IMAGE_THUNK_DATA*)(base + import_desc->FirstThunk);
				of_thunk->u1.AddressOfData;
				++of_thunk, ++thunk)
			{
				auto import_by_name = (IMAGE_IMPORT_BY_NAME*)(base + of_thunk->u1.AddressOfData);

				if (Util::IsAddressValid(import_by_name))
				{
					thunk->u1.Function = (uintptr_t)GetProcAddress(module, import_by_name->Name);

					std::cout << import_by_name->Name << " import: " << std::hex << thunk->u1.Function << std::endl;
				}
				else
				{
					std::cout << "Import thunk is destroyed!" << std::endl;
					thunk->u1.Function = NULL;
				}
			}
		}

		return true;
	}

	IMAGE_SECTION_HEADER* ForEachSection(uint8_t* image_base, bool(*SectionCallback)(IMAGE_SECTION_HEADER*, uintptr_t base, void* callback_data), void* callback_data)
	{
		auto pe_hdr = PE_HEADER(image_base);

		auto section = (IMAGE_SECTION_HEADER*)(pe_hdr + 1);

		for (int i = 0; i < pe_hdr->FileHeader.NumberOfSections; ++i)
		{
			if (!SectionCallback(&section[i], (uintptr_t)image_base, callback_data))
			{
				return &section[i];
			}
		}

		return NULL;
	}

	IMAGE_SECTION_HEADER* GetSection(uint8_t* pe_file, const char* section_name)
	{
		return PE::ForEachSection(
			pe_file,
			[](IMAGE_SECTION_HEADER* section, uintptr_t base, void* callback_data) -> bool
			{
				if (!strcmp((char*)callback_data, (char*)section->Name))
				{
					return false;
				}
				return true;
			},
			(void*)section_name
		);
	}

	void CopyHeaders(uint8_t* src, uint8_t* dest)
	{
		auto pe_hdr = PE_HEADER(src);

		auto section = (IMAGE_SECTION_HEADER*)(pe_hdr + 1);

		memcpy(dest, src, pe_hdr->OptionalHeader.SizeOfHeaders);
	}

	void ResolveRelocations(uint8_t* image_buffer, uint8_t* load_destination)
	{
		auto pe_hdr = PE_HEADER(image_buffer);

		auto& reloc_dir = pe_hdr->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];

		if (!reloc_dir.VirtualAddress)
		{
			return;
		}

		auto reloc = (IMAGE_BASE_RELOCATION*)(image_buffer + reloc_dir.VirtualAddress);

		for (auto cur_size = 0; cur_size < reloc_dir.Size; )
		{
			auto reloc_count = (reloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(int16_t);
			auto reloc_data = (int16_t*)((char*)reloc + sizeof(IMAGE_BASE_RELOCATION));
			auto reloc_base = (char*)(image_buffer + reloc->VirtualAddress);

			for (auto i = 0; i < reloc_count; ++i, ++reloc_data)
			{
				auto entry = (RelocationEntry*)reloc_data;

				if (entry->type == IMAGE_REL_BASED_DIR64)
				{
					*(uint8_t**)(reloc_base + entry->offset) +=
						(load_destination - (uint8_t*)pe_hdr->OptionalHeader.ImageBase);
				}
			}

			cur_size += reloc->SizeOfBlock;
			reloc = (IMAGE_BASE_RELOCATION*)reloc_data;
		}
	}

	void* GetExport(uintptr_t base, const char* export_name)
	{
		auto pe_hdr = PE_HEADER(base);

		IMAGE_DATA_DIRECTORY data_dir =
			pe_hdr->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];

		auto export_dir = (IMAGE_EXPORT_DIRECTORY*)(data_dir.VirtualAddress + base);

		auto function_array = (int*)(export_dir->AddressOfFunctions + base);
		auto name_array = (int*)(export_dir->AddressOfNames + base);
		auto ordinal_array = (int16_t*)(export_dir->AddressOfNameOrdinals + base);

		for (int i = 0; i < export_dir->NumberOfFunctions; ++i)
		{
			char* name = (char*)(name_array[i] + base);

			std::cout << "export name " << name << "\n";

			if (!strcmp(export_name, name))
			{
				int ordinal = ordinal_array[i];
				return (void*)function_array[ordinal];
			}
		}

		return NULL;
	}

	size_t RemapImage(uint8_t* unmapped_pe, uint8_t** out_buffer, int32_t target_pid, uintptr_t map_destination)
	{
		std::cout << *(int*)unmapped_pe;
		auto pe_header = PE_HEADER(unmapped_pe);

		auto virtual_size = pe_header->OptionalHeader.SizeOfImage;

		/* for unicorn engine 4KB alignment    */
		*out_buffer = (uint8_t*)_aligned_malloc(virtual_size, PAGE_SIZE);
		memset(*out_buffer, 0x00, virtual_size);

		PE::CopyHeaders(unmapped_pe, *out_buffer);

		/*	SizeOfRawData & PointerToRawData are 0 in VMP packed bins	*/

		PE::ForEachSection(unmapped_pe,
			[](IMAGE_SECTION_HEADER* section, uintptr_t unmapped_bin, void* out_buffer) -> bool {

				memcpy(*(uint8_t**)out_buffer + section->VirtualAddress,
					(char*)unmapped_bin + section->PointerToRawData, section->SizeOfRawData);

				return true;
			},
			out_buffer
		);

		PE::ResolveRelocations(*out_buffer, (uint8_t*)map_destination);

		if (!PE::ResolveImports(*out_buffer, target_pid))
		{
			std::cout << "Failed to resolve imports!!!!" << std::endl;
		}

		return virtual_size;
	}
};