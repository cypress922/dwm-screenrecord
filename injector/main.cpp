#include "manual_map.h"
#include "utils.h"
#include "injection_info.h"
#include <iostream>
#include <vector>
#include <assert.h>

extern "C" __declspec(dllexport) int InjectDLLBytes(
	int32_t pid,
	uint8_t* raw_payload_dll, 
	const char* entrypoint_name)
{
	HANDLE h_proc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);

	auto payload_size = PE_HEADER(raw_payload_dll)->OptionalHeader.SizeOfImage;
	auto header_size = PE_HEADER(raw_payload_dll)->OptionalHeader.SizeOfHeaders;

	auto remote_payload_base = (uint8_t*)VirtualAllocEx(h_proc, NULL, payload_size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);

	/*	Prepare the payload DLL for manual mapping	*/

	uint8_t* payload_mapped = NULL;

	PE::RemapImage(raw_payload_dll, &payload_mapped, pid, (uintptr_t)remote_payload_base);

	SIZE_T num_bytes;

	WriteProcessMemory(h_proc, remote_payload_base, payload_mapped, payload_size, &num_bytes);

	/*	write DLL parameters	*/

	DllParams params = {
		params.header = INJECTOR_CONSTANTS::mapped_dll_header,
		params.payload_dll_base = (uintptr_t)remote_payload_base,
		params.payload_dll_size = payload_size,
	};

	/*	Allocate, Initialize, and pass the DLL parameters	*/

	auto params_location = VirtualAllocEx(h_proc, NULL, sizeof(DllParams), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

	WriteProcessMemory(h_proc, params_location, &params, sizeof(params), &num_bytes);

	/*	invoke the entry point	*/

	auto entry_point = (uint8_t*)PE::GetExport((uintptr_t)payload_mapped, entrypoint_name);

	std::cout << std::hex << " entry_point 0x" << (uintptr_t)entry_point + (uintptr_t)remote_payload_base << std::endl;

	auto h_thread = CreateRemoteThread(h_proc, nullptr, 0, (LPTHREAD_START_ROUTINE)(entry_point + (uintptr_t)remote_payload_base), params_location, 0, nullptr);
	
	CloseHandle(h_thread);

	return 0;
}

extern "C" int main()
{
	std::string payload_dll_name;

	std::cout << "Enter the name of the DLL to inject: " << std::endl;

	std::cin >> payload_dll_name;

	int32_t target_pid;

	std::cout << "Enter the pid of the target process: " << std::endl;

	std::cin >> target_pid;

	uint8_t* payload_dll_raw = NULL;

	auto image_size = Util::LoadFile(payload_dll_name.c_str(), &payload_dll_raw);

 	InjectDLLBytes(target_pid, payload_dll_raw, ENTRYPOINT_NAME);

	std::cin.get();
}
