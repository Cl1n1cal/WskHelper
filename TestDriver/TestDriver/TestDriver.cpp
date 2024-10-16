#include "TestDriver.h"
#include "pch.h"
#include "TestDriverCommon.h"
#include "WskHelper.h"
#include "WskHelperCommon.h"




void TestDriverUnload(PDRIVER_OBJECT DriverObject);

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

void ExampleFunction();

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

	DbgPrint("Calling example function\n");
	ExampleFunction();

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
	PDEVICE_OBJECT TargetDeviceObject,
	PFILE_OBJECT FileObject,
	ULONG IoctlCode,
	PVOID InputBuffer,
	ULONG InputBufferLength,
	PVOID OutputBuffer,
	ULONG OutputBufferLength)
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


void ExampleFunction() 
{
	UNICODE_STRING targetDeviceName;
	PDEVICE_OBJECT targetDeviceObject;
	PFILE_OBJECT fileObject;
	NTSTATUS status;

	// The target device's name (e.g., for the second driver)
	RtlInitUnicodeString(&targetDeviceName, L"\\Device\\WskHelper");

	// Step 1: Get the target device object pointer
	status = GetTargetDeviceObject(&targetDeviceName, &targetDeviceObject, &fileObject);
	if (!NT_SUCCESS(status)) {
		DbgPrint("Failed to get the target device object\n");
		return;
	}

	// Step 2: Send an IOCTL to the target driver
	auto ioctlCode = IOCTL_WSKHELPER_CREATE_CONNECTION;  // Define your IOCTL code
	UCHAR inputOutputBuffer[2048] = { 0 };       // Input and output buffer is the same
	status = SendIoctlToDevice(targetDeviceObject, fileObject, ioctlCode, inputOutputBuffer, sizeof(inputOutputBuffer), inputOutputBuffer, sizeof(inputOutputBuffer));

	if (NT_SUCCESS(status)) {
		DbgPrint("IOCTL sent successfully!\n");
	}
	else {
		DbgPrint("Failed to send IOCTL, status: 0x%x\n", status);
	}

	// Dereference the file object when done
	ObDereferenceObject(fileObject);
}
