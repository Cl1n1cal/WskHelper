#include "TestDriver.h"
#include "pch.h"
#include "TestDriverCommon.h"
#include "WskHelper.h"
#include "WskHelperCommon.h"

void TestDriverUnload(PDRIVER_OBJECT DriverObject);

UNICODE_STRING g_targetDeviceName;
PDEVICE_OBJECT g_targetDeviceObject;
PFILE_OBJECT g_fileObject;

AddressInfo g_addressInfo;


NTSTATUS TestDriverDispatchCreate(PDEVICE_OBJECT, PIRP Irp);
NTSTATUS TestDriverDispatchClose(PDEVICE_OBJECT, PIRP Irp);
NTSTATUS TestDriverDispatchRead(PDEVICE_OBJECT, PIRP Irp);
NTSTATUS TestDriverDispatchWrite(PDEVICE_OBJECT, PIRP Irp);
NTSTATUS TestDriverDispatchDeviceControl(PDEVICE_OBJECT, PIRP Irp);
NTSTATUS GetTargetDeviceObject(PUNICODE_STRING DeviceName, PDEVICE_OBJECT* DeviceObject, PFILE_OBJECT* FileObject);
NTSTATUS SendIoctlToDevice(
	PDEVICE_OBJECT TargetDeviceObject,
	PFILE_OBJECT FileObject,
	ULONG IoctlCode,
	PVOID InputBuffer,
	ULONG InputBufferLength,
	PVOID OutputBuffer,
	ULONG OutputBufferLength);
NTSTATUS WSKHRegister();
NTSTATUS WSKHUnRegister();
NTSTATUS WSKHSend(char* Data);
NTSTATUS WSKHConnect(const char* IpAddress, USHORT PortNumber);
NTSTATUS WSKHDisconnect();
NTSTATUS WSKHCloseSocket();


void TestSendData();
void TestDisconnect();
void TestCloseSocket();

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	UNREFERENCED_PARAMETER(RegistryPath);
	NTSTATUS status = STATUS_SUCCESS;


	// Setup Device Object and Symbolic Link
	UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\Device\\TestDriver");
	UNICODE_STRING symLinkName = RTL_CONSTANT_STRING(L"\\??\\TestDriver");
	PDEVICE_OBJECT deviceObject = nullptr;

	// Set unload routine to be able to use sc stop command
	DriverObject->DriverUnload = TestDriverUnload;

	DriverObject->MajorFunction[IRP_MJ_CREATE] = TestDriverDispatchCreate;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = TestDriverDispatchClose;
	DriverObject->MajorFunction[IRP_MJ_READ] = TestDriverDispatchRead;
	DriverObject->MajorFunction[IRP_MJ_WRITE] = TestDriverDispatchWrite;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = TestDriverDispatchDeviceControl;

	do
	{
		status = IoCreateDevice(DriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, 0, TRUE, &deviceObject);

		if (!NT_SUCCESS(status))
		{
			DbgPrint( "Failed to create device (0x%08X)\n", status);
			break;
		}

		// Set up direct I/O
		deviceObject->Flags |= DO_DIRECT_IO;
		status = IoCreateSymbolicLink(&symLinkName, &devName);

		if (!NT_SUCCESS(status))
		{
			DbgPrint("Failed to create symbolic link (0x%08X)\n", status);
			break;
		}

	} while (false);

	//###############################

	//Register WskHelper
	status = WSKHRegister();

	if (!NT_SUCCESS(status))
	{
		DbgPrint("Failed to register WskHelper (0x%08X)\n", status);
	}


	DbgPrint("TestSendData\n");
	TestSendData();

	//DbgPrint("TestDisconnect\n");
	//TestDisconnect();

	//DbgPrint("TestCloseSocket\n");
	//TestCloseSocket();

	//Unregister WskHelper
	status = WSKHUnRegister();

	//#############################
	return status;
}

NTSTATUS CompleteIrp(PIRP Irp, NTSTATUS Status = STATUS_SUCCESS, ULONG_PTR Info = 0)
{
	Irp->IoStatus.Status = Status;
	Irp->IoStatus.Information = Info;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return Status;
}

void TestDriverUnload(PDRIVER_OBJECT DriverObject)
{
	UNICODE_STRING symLinkName = RTL_CONSTANT_STRING(L"\\??\\TestDriver");


	DbgPrint("Unloading Test driver\n");
	IoDeleteSymbolicLink(&symLinkName);
	IoDeleteDevice(DriverObject->DeviceObject);
}

NTSTATUS TestDriverDispatchCreate(PDEVICE_OBJECT, PIRP Irp)
{
	return CompleteIrp(Irp);
}

NTSTATUS TestDriverDispatchClose(PDEVICE_OBJECT, PIRP Irp)
{
	return CompleteIrp(Irp);
}

NTSTATUS TestDriverDispatchRead(PDEVICE_OBJECT, PIRP Irp)
{
	return CompleteIrp(Irp);
}

NTSTATUS TestDriverDispatchWrite(PDEVICE_OBJECT, PIRP Irp)
{
	return CompleteIrp(Irp);
}

NTSTATUS TestDriverDispatchDeviceControl(PDEVICE_OBJECT, PIRP Irp)
{
	auto status = STATUS_SUCCESS;
	auto len = 0;
	return CompleteIrp(Irp, status, len);
}

NTSTATUS WSKHRegister()
{
	NTSTATUS status = STATUS_SUCCESS;

	// The target device's name (e.g., for the second driver)
	RtlInitUnicodeString(&g_targetDeviceName, L"\\Device\\WskHelper");

	// Step 1: Get the target device object pointer
	status = GetTargetDeviceObject(&g_targetDeviceName, &g_targetDeviceObject, &g_fileObject);
	if (!NT_SUCCESS(status)) {
		DbgPrint("Failed to get the target device object\n");
		return status;
	}

	return status;
}

NTSTATUS WSKHUnRegister()
{
	// Dereference the file object when done
	ObDereferenceObject(g_fileObject);

	return STATUS_SUCCESS;
}


NTSTATUS GetTargetDeviceObject(PUNICODE_STRING DeviceName, PDEVICE_OBJECT* DeviceObject, PFILE_OBJECT* FileObject) {
	NTSTATUS status;

	// Get a pointer to the device object of the target driver
	status = IoGetDeviceObjectPointer(DeviceName, FILE_READ_DATA, FileObject, DeviceObject);
	if (!NT_SUCCESS(status)) {
		DbgPrint("Failed to get device object pointer for %wZ, Status: 0x%x\n", DeviceName, status);
	}

	return status;
}

NTSTATUS SendIoctlToDevice(
	PDEVICE_OBJECT	TargetDeviceObject,
	PFILE_OBJECT	FileObject,
	ULONG			IoctlCode,
	PVOID			InputBuffer,
	ULONG			InputBufferLength,
	PVOID			OutputBuffer,
	ULONG			OutputBufferLength)
{
	NTSTATUS status;
	PIRP irp;
	KEVENT event;
	IO_STATUS_BLOCK ioStatusBlock;
	UNREFERENCED_PARAMETER(FileObject);

	// Initialize an event to wait for completion of the request
	KeInitializeEvent(&event, NotificationEvent, FALSE);

	// Build the IRP for the device control request (IOCTL)
	irp = IoBuildDeviceIoControlRequest(
		IoctlCode,                    // IOCTL code
		TargetDeviceObject,            // Target device
		InputBuffer,                   // Input buffer (optional)
		InputBufferLength,             // Input buffer size
		OutputBuffer,                  // Output buffer (optional)
		OutputBufferLength,            // Output buffer size
		FALSE,                         // Is it an internal device control request?
		&event,                        // Event to signal on completion
		&ioStatusBlock                 // IO status block
	);

	if (irp == NULL) {
		DbgPrint("Failed to build IOCTL request\n");
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	// Send the IRP to the target driver
	status = IoCallDriver(TargetDeviceObject, irp);

	if (status == STATUS_PENDING) {
		// Wait for the request to complete
		KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
		status = ioStatusBlock.Status;
	}

	return status;
}

NTSTATUS WSKHConnect(const char* IpAddress, const USHORT PortNumber)
{
	NTSTATUS	status						=	STATUS_SUCCESS;
	auto		ioctlCode				=	IOCTL_WSKHELPER_CREATE_CONNECTION;  // Define your IOCTL code
	UCHAR		inputOutputBuffer[2048]		=	{ 0 };								// Input and output buffer is the same

	// Ensure that the provided IpAddress is not NULL
	if (IpAddress == NULL) {
		DbgPrint("Error: IpAddress is NULL\n");
		return STATUS_INVALID_PARAMETER;
	}

	// Clear the global address info struct to avoid garbage data
	RtlZeroMemory(&g_addressInfo, sizeof(g_addressInfo));

	// Safely copy the IP address and ensure null termination
	strncpy(g_addressInfo.IpAddress, IpAddress, sizeof(g_addressInfo.IpAddress) - 1);
	g_addressInfo.IpAddress[sizeof(g_addressInfo.IpAddress) - 1] = '\0';  // Null-terminate explicitly

	// Set the port number
	g_addressInfo.PortNumber = PortNumber;

	// Copy g_addressInfo to the input/output buffer for the IOCTL
	memcpy(inputOutputBuffer, (PVOID)&g_addressInfo, sizeof(g_addressInfo));

	status = SendIoctlToDevice(g_targetDeviceObject, g_fileObject, ioctlCode, inputOutputBuffer, sizeof(inputOutputBuffer), inputOutputBuffer, sizeof(inputOutputBuffer));

	return status;
}


NTSTATUS WSKHSend(char* Data)
{
	NTSTATUS	status						=	STATUS_SUCCESS;
	auto		ioctlCode				=	IOCTL_WSKHELPER_SEND_DATA;	// Define your IOCTL code
	UCHAR		inputOutputBuffer[2048]	=	{ 0 };							// Input and output buffer is the same

	memcpy(inputOutputBuffer, Data, strlen(Data) + 1);
	status = SendIoctlToDevice(g_targetDeviceObject, g_fileObject, ioctlCode, inputOutputBuffer, sizeof(inputOutputBuffer), inputOutputBuffer, sizeof(inputOutputBuffer));

	return status;
}

NTSTATUS WSKHDisconnect()
{
	NTSTATUS	status						=	STATUS_SUCCESS;
	auto		ioctlCode				=	IOCTL_WSKHELPER_DISCONNECT;	// Define your IOCTL code
	UCHAR		inputOutputBuffer[2048]		=	{ 0 };						// Input and output buffer is the same

	status = SendIoctlToDevice(g_targetDeviceObject, g_fileObject, ioctlCode, inputOutputBuffer, sizeof(inputOutputBuffer), inputOutputBuffer, sizeof(inputOutputBuffer));

	return status;
}

NTSTATUS WSKHCloseSocket()
{
	NTSTATUS	status						=	STATUS_SUCCESS;
	auto		ioctlCode				=	IOCTL_WSKHELPER_CLOSE;	// Define your IOCTL code
	UCHAR		inputOutputBuffer[2048]		=	{ 0 };					// Input and output buffer is the same
	status = SendIoctlToDevice(g_targetDeviceObject, g_fileObject, ioctlCode, inputOutputBuffer, sizeof(inputOutputBuffer), inputOutputBuffer, sizeof(inputOutputBuffer));

	return status;
}


void TestSendData() 
{
	NTSTATUS	status		=	STATUS_SUCCESS;
	char		someData[]	=	"SomeData\n";

	status = WSKHConnect("127.0.0.1", 9999);

	if (!NT_SUCCESS(status))
	{
		DbgPrint("WSKHConnect failed: 0x%x\n", status);
	}

	status = WSKHSend(someData);

	if (NT_SUCCESS(status)) {
		DbgPrint("Send Data IOCTL sent successfully!\n");
		DbgPrint("+++Send Data Test Finished!+++\n");
	}
	else {
		DbgPrint("Failed to send IOCTL, status: 0x%x\n", status);
		DbgPrint("---Send data Test Failed---\n");
	}
}

void TestDisconnect()
{
	NTSTATUS status = STATUS_SUCCESS;

	status = WSKHConnect("127.0.0.1", 9999);

	if (NT_SUCCESS(status)) {
		DbgPrint("Create Conn IOCTL sent successfully!\n");
	}
	else {
		DbgPrint("Failed to send IOCTL, status: 0x%x\n", status);
	}

	status = WSKHDisconnect();

	if (NT_SUCCESS(status)) {
		DbgPrint("Disconnect IOCTL sent successfully!\n");
		DbgPrint("+++Disconnect Test Finished!+++\n");
	}
	else {
		DbgPrint("Failed to send IOCTL, status: 0x%x\n", status);
		DbgPrint("---Disconnect Test Failed---\n");
	}
}

void TestCloseSocket()
{
	NTSTATUS status = STATUS_SUCCESS;

	status = WSKHConnect("127.0.0.1", 9999);

	if (NT_SUCCESS(status)) {
		DbgPrint("Create Conn IOCTL sent successfully!\n");
	}
	else {
		DbgPrint("Failed to send IOCTL, status: 0x%x\n", status);
	}

	status = WSKHDisconnect();

	if (NT_SUCCESS(status)) {
		DbgPrint("CloseSocket IOCTL sent successfully!\n");
	}
	else {
		DbgPrint("Failed to send IOCTL, status: 0x%x\n", status);
	}

	status = WSKHCloseSocket();

	if (NT_SUCCESS(status)) {
		DbgPrint("CloseSocket IOCTL sent successfully!\n");
		DbgPrint("+++CloseSocket test finished!+++\n");
	}
	else {
		DbgPrint("Failed to send IOCTL, status: 0x%x\n", status);
		DbgPrint("---CloseSocket Test Failed---\n");
	}
}