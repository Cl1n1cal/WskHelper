#include "pch.h"
#include "..\WskHelper\WskHelperCommon.h"


//Error function for making error messages easier to handle
int Error(const char* msg)
{
	printf("%s error: %lu\n", msg, GetLastError());
	return 1;
}

int main() {
	//Open file handle to the driver
	HANDLE hDevice = ::CreateFile(L"\\\\.\\WskHelper", GENERIC_READ | GENERIC_WRITE, 0, nullptr,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

	if (hDevice == INVALID_HANDLE_VALUE)
	{
		return Error("Failed to open device");
	}

	//Try to call Device Control dispatch routine and see if we can get a value back
	DWORD bytes;
	Message  msg;
	bool deviceControl = DeviceIoControl(hDevice, IOCTL_WSKHELPER_CREATE_CONNECTION,
		nullptr, 0, &msg, sizeof(msg), &bytes, nullptr);

	if (!deviceControl)
	{
		return Error("Failed in DeviceIoControl");
	}

	::CloseHandle(hDevice);
	return 0;
}