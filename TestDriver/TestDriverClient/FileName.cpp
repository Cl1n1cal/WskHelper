#include "pch.h"
#include "../TestDriver/TestDriverCommon.h"

//Function definitions
int OptionOne();
void HandleChoice(int Choice);

//Error function for making error messages easier to handle
int Error(const char* msg)
{
	printf("%s error: %lu\n", msg, GetLastError());
	return 1;
}


int main()
{
	int choice;
	while (true) {
		// Display menu
		std::cout << "Please choose an option:\n";
		std::cout << "1. Display running processes\n";
		std::cout << "2. Connect to a remote system \n";
		std::cout << "3. Option 3\n";
		std::cout << "Enter your choice: ";

		// Get user input
		std::cin >> choice;

		if (choice == 0)
		{
			break;
		}
		// Pass the user's choice to the selector function
		HandleChoice(choice);
	}
}

void HandleChoice(int Choice)
{
	switch (Choice)
	{
	case 1:
		OptionOne();
		break;

	case 2:
		std::cout << "option 2 nothing yet\n";
		break;
	case 3:
		std::cout << "You selected Option 3. Logic for Option 3...\n";
		// Add logic for Option 3
		break;
	default:
		std::cout << "Invalid input, please enter 1, 2 or 3\n";
		break;
	}
}

int OptionOne()
{
	//Open file handle to the driver
	HANDLE hDevice = ::CreateFile(L"\\\\.\\TestDriver", GENERIC_READ | GENERIC_WRITE, 0, nullptr,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

	if (hDevice == INVALID_HANDLE_VALUE)
	{
		return Error("Failed to open device");
	}

	//Try to call Device Control dispatch routine and see if we can get a value back
	DWORD bytes;
	ProcessInfo  info;
	bool deviceControl = DeviceIoControl(hDevice, IOCTL_TEST_GET_STATS,
		nullptr, 0, &info, sizeof(info), &bytes, nullptr);

	if (!deviceControl)
	{
		return Error("Failed in DeviceIoControl");
	}

	::CloseHandle(hDevice);

	return 0;
}
