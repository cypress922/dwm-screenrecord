#pragma once
#include "includes.h"

struct DllParams
{
	uint32_t header;
	uintptr_t payload_dll_base;
	uint32_t payload_dll_size;
};

enum INJECTOR_CONSTANTS
{
	mapped_dll_header = 0x12345678,
};

#define ENTRYPOINT_NAME	"ManualMapEntry"
