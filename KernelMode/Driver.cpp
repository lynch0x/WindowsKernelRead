
#include "Utils.h"

DECLSPEC_NOINLINE INT64 __fastcall Khg_Function(PVOID, PVOID, PVOID, PVOID, PVOID);

uint32_t FixImports(PVOID Function, SIZE_T Size, PVOID Imports)
{
	uint32_t Index = 0;
	for (SIZE_T i = 0; i < Size - sizeof(ULONG64); i++)
	{
		if (*(ULONG64*)((ULONG64)Function + i) - MAGICNUMBER < 0x10000 &&
			(LONG64)(*(ULONG64*)((ULONG64)Function + i) - MAGICNUMBER) >= 0)
		{
			Index++;
			auto Address = ((*(ULONG64*)((ULONG64)Function + i)) - MAGICNUMBER);
			auto Value = ((DWORD64)Imports + Address);
			*(ULONG64*)((ULONG64)Function + i) = Value;
		}
	}
	return Index;
}

PVOID AllocatePage()
{
	typedef PVOID
	(*MiMapSinglePage_t)(
		PVOID VirtualAddress,
		ULONGLONG PageFrameNumber
		);

	auto MiMapSinglePage = reinterpret_cast<MiMapSinglePage_t>(
		FindPatternImage((PCHAR)g_KernelBase, 
			(PCHAR)"\x48\x8B\xC4\x48\x89\x00\x00\x48\x89\x00\x00\x48\x00\x00\x00\x48\x89\x00\x00\x00\x00\x00\x55\x00\x00\x48\x83\x00\x00\x41\x8B\x00\x45\x8B", 
			(PCHAR)"xxxxx??xx??x???xx?????x??xx??xx?xx"));

	auto Result = MiMapSinglePage(nullptr, 0);
	if (!Result)
		return NULL;

	auto PTE = GetPteAddress(Result);
	MiMakePageValid(PTE);

	auto PDE = GetPdeAddress(Result);
	MiMakePageValid(PDE);

	return Result;
}

BOOL ReadVirtualMemory(PVOID dest, PVOID src, size_t size) {

	size_t pSize;
	if (NT_SUCCESS(g_Imports->MmCopyVirtualMemory(g_Imports->IoGetCurrentProcess(), src,
		g_Imports->IoGetCurrentProcess(), dest, size, KernelMode, &pSize)
	) && size == pSize) {
		return TRUE;
	}

	return FALSE;
}

NTSTATUS MapPage()
{
	auto win32kbase = GetModuleBase(L"win32kbase.sys");
	if (!win32kbase)
		return STATUS_UNSUCCESSFUL;

	auto DataPtr = (uint64_t)FindPatternImage(
		(PCHAR)win32kbase,
		(PCHAR)"\x74\x20\x48\x8B\x44\x24\x00\x44", 
		(PCHAR)"xxxxxx?x");

	if (!DataPtr)
		return 3;

	DataPtr = uint64_t((uint64_t)DataPtr - 0xA);
	DataPtr = (uint64_t)DataPtr + *(uint32_t*)((PBYTE)DataPtr + 3) + 7;

	g_Code = reinterpret_cast<ImportsList*>(ExAllocatePool2(POOL_FLAG_NON_PAGED, sizeof(ImportsList),'NULL'));
	if (!g_Code)return 4;

	RtlZeroMemory(g_Code, sizeof(ImportsList));

	g_Code->ExGetPreviousMode = ExGetPreviousMode;
	g_Code->DbgPrintEx = DbgPrintEx;
	g_Code->PsGetProcessSectionBaseAddress = PsGetProcessSectionBaseAddress;
	g_Code->PsLookupProcessByProcessId = PsLookupProcessByProcessId;
	g_Code->MmCopyVirtualMemory = MmCopyVirtualMemory;
	g_Code->IoGetCurrentProcess = IoGetCurrentProcess;

	g_Code->FunctionHook = reinterpret_cast<Khg_Function_t>(AllocatePage());
	if (!g_Code->FunctionHook)
		return 5;

	memcpy(g_Code->FunctionHook, &Khg_Function, PAGE_SIZE);

	auto Count = FixImports(g_Code->FunctionHook, PAGE_SIZE, g_Code);

	*(PVOID*)&g_Code->FunctionOriginal = _InterlockedExchangePointer(
		(PVOID*)DataPtr,
		(PVOID)g_Code->FunctionHook);

	if (!g_Code->FunctionOriginal)
		return 6;

	

	return STATUS_SUCCESS;
}

NTSTATUS g_DriverEntry(PVOID Base, PVOID Size)
{
	UNREFERENCED_PARAMETER(Base);
	UNREFERENCED_PARAMETER(Size);
	g_KernelBase = (ULONG64)GetModuleBase(L"ntoskrnl.exe");
	if (!g_KernelBase)
		return 2;

	return MapPage();
}

#pragma code_seg(".dump")
DECLSPEC_NOINLINE INT64 __fastcall Khg_Function(PVOID a1, PVOID a2, PVOID a3, PVOID a4, PVOID a5)
{
	if (g_Imports->ExGetPreviousMode() != UserMode) {
		return g_Imports->FunctionOriginal(a1, a2, a3, a4, a5);
	}

	Communication comms = {};
	if (!ReadVirtualMemory(&comms, a3, sizeof(Communication)) || comms.Reason != COMMUNICATION_KEY) {
		return g_Imports->FunctionOriginal(a1, a2, a3, a4, a5);
	}

	auto args = (Communication*)a3;

	switch (comms.Request) {

		case Request::GETBASE: {
			if (comms.processID) {
				PEPROCESS process = { 0 };
				g_Imports->PsLookupProcessByProcessId((HANDLE)args->processID, &process);
				args->Outbase = g_Imports->PsGetProcessSectionBaseAddress(process);
				ObDereferenceObject(process);
			}
			break;
		}
		case Request::READPROCESSMEMORY: {
			if (comms.processID) {
				PEPROCESS process = { 0 };
				g_Imports->PsLookupProcessByProcessId((HANDLE)args->processID, &process);
					SIZE_T Bytes = 0;
					
					 
					g_Imports->MmCopyVirtualMemory(process, args->Address, g_Imports->IoGetCurrentProcess(), &args->result, args->size, KernelMode, &Bytes);
					ObDereferenceObject(process);
			}
			break;
		}
	}

	return NULL;
}
#pragma code_seg()