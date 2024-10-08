#include <Windows.h>
#include <TlHelp32.h>
#include <iostream>

void*  g_DataPtr = 0;

enum Request {
	GETBASE = 32,
	READPROCESSMEMORY = 43,
	UNLOADEAC = 255
};


struct Communication {

	Request Request;
	HANDLE processID;
	DWORD Reason; // must be 0xDEADBEEF....
	PVOID Outbase; // output image base for process.

	/*
	* READ/WRITE PROCESS MEMORY.
	*/
	PVOID Address;
	PVOID result;
	size_t size;
};

BOOL Setup() {

	LoadLibraryA("user32.dll");
	LoadLibraryA("win32u.dll");
	LoadLibraryA("ntdll.dll");

	auto win32u = GetModuleHandle("win32u.dll");
	if (!win32u) {
		printf("failed to load win32u.dll\n");
		return FALSE;
	}

	g_DataPtr = GetProcAddress(win32u, "NtUserGetObjectInformation");
	if (!g_DataPtr) {
		printf("failed to find NtUserGetObjectInformation\n");
		return FALSE;
	}

	return TRUE;
}

template<typename ... Arg>
PVOID CallCommunication(const Arg ... args)
{
	if (!g_DataPtr)
		return NULL;

	auto aFunc = static_cast<PVOID(_fastcall*)(Arg...)>(g_DataPtr);
	return aFunc(args ...);
}

PVOID GetBaseAddress(HANDLE processID) {
	Communication request = {};
	SecureZeroMemory(&request, sizeof(Communication));

	request.Request = Request::GETBASE;
	request.Reason = 0xDEADBEEF;
	request.processID = processID;
	request.Outbase = 0;

	CallCommunication(0, 0, &request);
	return request.Outbase;
}

template <typename T>
T ReadMemory(HANDLE processID, T Address) {
	Communication request = {};
	SecureZeroMemory(&request, sizeof(Communication));

	request.Request = Request::READPROCESSMEMORY;
	request.Reason = 0xDEADBEEF;
	request.processID = processID;
	request.Address = Address;
	request.size = sizeof(T);

	CallCommunication(0, 0, &request);
	return request.result;
}

DWORD GetProcessID(std::string processName) {

	DWORD processpid = 0;
	PROCESSENTRY32 entry;
	entry.dwSize = sizeof(PROCESSENTRY32);

	HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);

	if (Process32First(snapshot, &entry) == TRUE)
	{
		while (Process32Next(snapshot, &entry) == TRUE)
		{
			if (stricmp(entry.szExeFile, processName.c_str()) == 0)
			{
				processpid = entry.th32ProcessID;
				break;
			}
		}
	}

	CloseHandle(snapshot);
	return processpid;
}

int main()
{
	if (!Setup()) {
		printf("failed to initialize communication!\n");
		Sleep(-1);
		return -1;
	}

	auto procID = GetProcessID("DeadByDaylight-Win64-Shipping.exe");
	if (!procID) {
		printf("process not found!\n");
		Sleep(-1);
		return -1;
	}

	printf("Process ID -> %d\n", procID);

	auto base = GetBaseAddress((HANDLE)procID);
	if (!base) {
		printf("failed to get base address of process id -> %d\n", procID);
		Sleep(-1);
		return -1;
	}

	printf("Image Base -> 0x%p\n", base);


	/*ULONG Offset = 0x77;
	auto Value = ReadMemory((HANDLE)procID, PVOID((uint64_t)base + Offset));
	printf("Value -> %llu\n", (uint64_t)Value);*/

	Sleep(-1);

	return 0;
}