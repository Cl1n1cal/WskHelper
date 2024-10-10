#include "pch.h"
#include "WskHelper.h"
#include "Conversions.h"
#include "WskHelperCommon.h"

/*
 * Welcome to WskHelper version 1.0
 * This version enables the use of only 1 instance of Network Provider Interface (NPI)
 * Also, it is only possible to make 1 listener or 1 connection to send/receive data on.
 */



// WSK Client Dispatch table that denotes the WSK version
// Must be kept valid in memory until WskDeregister has been called and registration is no longer valid
const WSK_CLIENT_DISPATCH wskAppDispatch = {
  MAKE_WSK_VERSION(1,0), // Use WSK version 1.0
  0,		// Reserved
  nullptr	// WskClientEvent callback not required for WSK version 1.0
};

// Global WskProvider
WSK_PROVIDER_NPI wskProviderNpi;

// Necessary for registering this driver as a WSK app
WSK_CLIENT_NPI wskClientNpi;

// WSK Registration object. Must be kept valid in memory
WSK_REGISTRATION wskRegistration;

PWSK_APP_SOCKET_CONTEXT socketContext;

//ipv4 Socket address
SOCKADDR_IN localAddress;


SOCKADDR_IN remoteAddress;


void PrintMessage() {
	DbgPrint("Hello world");
}



extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)
{
	DbgPrint("Starting DriverEntry");

	UNREFERENCED_PARAMETER(RegistryPath);

	// Setup Device Object and Symbolic Link
	UNICODE_STRING devName = RTL_CONSTANT_STRING(L"\\Device\\WskHelper");
	UNICODE_STRING symLinkName = RTL_CONSTANT_STRING(L"\\??\\WskHelper");
	PDEVICE_OBJECT deviceObject = nullptr;

	// Set driver dispatch routines
	DriverObject->DriverUnload = WskHelperUnload;
	DriverObject->MajorFunction[IRP_MJ_CREATE] = WskHelperDispatchCreate;
	DriverObject->MajorFunction[IRP_MJ_CLOSE] = WskHelperDispatchClose;
	DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = WskHelperDispatchDeviceControl;

	// Local variables
	NTSTATUS status = STATUS_SUCCESS;
	auto symLinkCreated = false;
	auto wskInitialized = false;

	do
	{
		status = IoCreateDevice(DriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, 0, TRUE, &deviceObject);

		if (!NT_SUCCESS(status))
		{
			DbgPrint("Failed to create device (0x%08X)\n", status);
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

		status = InitializeWsk();

		if (!NT_SUCCESS(status))
		{
			DbgPrint("Failed to initialize wsk (0x%08X)\n", status);
			break;
		}

		symLinkCreated = true;
		wskInitialized = true;
	} while (false);

	// Error handling
	if (!NT_SUCCESS(status))
	{
		if (wskInitialized)
		{
			// Unregister the WSK application
			WskDeregister(&wskRegistration);
		}

		if (symLinkCreated)
		{
			IoDeleteSymbolicLink(&symLinkName);
		}

		if (deviceObject)
		{
			IoDeleteDevice(deviceObject);
		}
	}

	DbgPrint("DriverEntry finished");
	DbgPrint("It finished with status: (0x%08X)\n", status);
	return status;
}

NTSTATUS CompleteIrp(PIRP Irp, NTSTATUS Status = STATUS_SUCCESS, ULONG_PTR Info = 0)
{
	Irp->IoStatus.Status = Status;
	Irp->IoStatus.Information = Info;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return Status;
}

NTSTATUS DelayForMilliseconds(LONG milliseconds)
{
	LARGE_INTEGER interval;

	// Convert milliseconds to 100-nanosecond units
	interval.QuadPart = -(milliseconds * 10000); // Negative value for relative time

	// Wait for the specified interval
	return KeDelayExecutionThread(KernelMode, FALSE, &interval);
}

void WskHelperUnload(PDRIVER_OBJECT DriverObject)
{
	KdPrint(("Unloading driver\n"));

	UNICODE_STRING symLinkName = RTL_CONSTANT_STRING(L"\\??\\WskHelper");

	// Unregister the WSK application
	

	// Delete symbolic link and device
	IoDeleteSymbolicLink(&symLinkName);
	IoDeleteDevice(DriverObject->DeviceObject);
}

NTSTATUS WskHelperDispatchCreate(PDEVICE_OBJECT, PIRP Irp)
{
	return CompleteIrp(Irp);
}
NTSTATUS WskHelperDispatchClose(PDEVICE_OBJECT, PIRP Irp)
{
	return CompleteIrp(Irp);
}

NTSTATUS WskHelperDispatchDeviceControl(PDEVICE_OBJECT, PIRP Irp)
{
	auto irpSp = IoGetCurrentIrpStackLocation(Irp);
	auto& dic = irpSp->Parameters.DeviceIoControl;
	auto status = STATUS_INVALID_DEVICE_REQUEST;
	//ULONG_PTR len = 0;

	switch (dic.IoControlCode)
	{
	case IOCTL_WSKHELPER_CREATE_CONNECTION:
		{
			// TODO: The ioctl request got this far
			DbgPrint("Create connection called\n");
			DbgPrint("Size of message: %llu\n", sizeof(Message));
			DbgPrint("Size og dic.OuputBufferLength: %d\n", dic.OutputBufferLength);

			if (dic.OutputBufferLength < sizeof(Message))
			{
				status = STATUS_BUFFER_TOO_SMALL;
				DbgPrint("Buffer too small\n");
				break;
			}

			DbgPrint("Send stats called\n");
			WSK_PROVIDER_NPI wskProviderNpi;


			// 1. Register WSK Provider Npi - Gives access to Wsk Functions
			// WSK_NO_WAIT - Return from the function immediately if the NPI is not available
			DbgPrint("WskCaptureProviderNPI\n");
			status = WskCaptureProviderNPI(&WskRegistration, WSK_NO_WAIT, &wskProviderNpi);

			DelayForMilliseconds(3000);

			if (!NT_SUCCESS(status))
			{
				DbgPrint("WskCaptureProviderNpi failed", status);
			}




			// WskSocketConnect can be used for the 3 steps below, but for educational purposes it will be done manually - for now

			// 2. Create the connection socket
			// Allocate memory for socketContext
			PWSK_APP_SOCKET_CONTEXT socketContext = (PWSK_APP_SOCKET_CONTEXT)ExAllocatePool2(
				POOL_FLAG_PAGED, sizeof(WSK_APP_SOCKET_CONTEXT), 'ASOC');
			if (!socketContext)
			{
				DbgPrint("Allocating Socket context failed");
				status = STATUS_INSUFFICIENT_RESOURCES;
				break;
			}

			DbgPrint("CreateConnectionSocket\n");
			status = CreateConnectionSocket(&wskProviderNpi, socketContext, nullptr); // Not using event callbacks for now, hence the nullptr on Dispatch

			DelayForMilliseconds(3000);

			if (!NT_SUCCESS(status))
			{
				DbgPrint("CreateConnectionSocket failed\n", status);
			}

			// 3. Bind the socket to a local transport address
			// Prepare the local transport address (e.g., bind to 127.0.0.1:8080)
			SOCKADDR_IN localAddr;
			localAddr.sin_family = AF_INET;
			localAddr.sin_port = 0;               // Bind to port any
			localAddr.sin_addr.s_addr = INADDR_ANY;  // Bind to any local address

			DbgPrint("BindConnectionSocket\n");
			status = BindConnectionSocket(socketContext->Socket, (PSOCKADDR)&localAddr);
			DelayForMilliseconds(3000);
			if (!NT_SUCCESS(status))
			{
				DbgPrint("BinConnectionSocket failed\n", status);
			}

			// 4. Create a connection with the socket
			SOCKADDR_IN remoteAddr;
			remoteAddr.sin_family = AF_INET;
			remoteAddr.sin_port = Khtons((SHORT)9999);  // Port 9999
			remoteAddr.sin_addr.s_addr = Khtonl(0x7f000001);  // IP address 127.0.0.1
			DbgPrint("ConnectSocket\n");
			status = ConnectSocket(socketContext->Socket, (PSOCKADDR)&remoteAddr);
			DelayForMilliseconds(3000);
			if (!NT_SUCCESS(status))
			{
				DbgPrint("ConnectSocket failed\n", status);
			}

			// 5. Send data over the socket connection oriented socket
			 // Allocate memory for the data
			const char* data = "Hello, WSK!";
			size_t dataLength = strlen(data) + 1;
			PVOID bufferMemory = ExAllocatePool2(POOL_FLAG_NON_PAGED, dataLength, '1gaT');
			if (!bufferMemory)
			{
				DbgPrint("Exallocatepool2 failed\n");
				return STATUS_INSUFFICIENT_RESOURCES; // Allocation failed
			}

			// Copy the data into the allocated memory
			RtlCopyMemory(bufferMemory, data, dataLength);

			// Allocate an MDL for the buffer memory
			DbgPrint("IoAllocatedMdl\n");
			PMDL mdl = IoAllocateMdl(bufferMemory, (ULONG)dataLength, FALSE, FALSE, NULL);
			if (!mdl) {
				DbgPrint("IoAllocateMdl failed\n");
				ExFreePool(bufferMemory);
				return STATUS_INSUFFICIENT_RESOURCES; // MDL allocation failed
			}

			// Build the MDL for non-paged pool
			DbgPrint("MmBuildMdlForNonPagedPool\n");
			MmBuildMdlForNonPagedPool(mdl);


			// Prepare the WSK_BUF structure
			WSK_BUF dataBuffer;
			dataBuffer.Mdl = mdl;                     // Set the MDL for the data
			dataBuffer.Offset = 0; //MmGetMdlByteOffset(mdl);                    
			dataBuffer.Length = dataLength;           // Set the length of the buffer

			DelayForMilliseconds(3000);

			DbgPrint("SendData\n");
			status = SendData(socketContext->Socket, &dataBuffer);
			DelayForMilliseconds(3000);
			if (!NT_SUCCESS(status))
			{
				DbgPrint("SendData failed\n", status);
			}

			status = STATUS_SUCCESS;
			break;
		}
	}

	DbgPrint("WskHelper dispatch finished with status: (0x%08X)\n", status);
	return status;
}



NTSTATUS InitializeWsk() {
	NTSTATUS status = STATUS_SUCCESS;

	do
	{
		// Register the WSK application
		wskClientNpi.ClientContext = nullptr;
		wskClientNpi.Dispatch = &wskAppDispatch;
		status = WskRegister(&wskClientNpi, &wskRegistration);

		if (!NT_SUCCESS(status)) {
			DbgPrint("WsKRegister failed in InitializeWsk(): %d\n", status);
			break;
		}

		// Capture provider
		status = WskCaptureProviderNPI(&wskRegistration, WSK_NO_WAIT, &wskProviderNpi);

		if (!NT_SUCCESS(status))
		{
			DbgPrint("WskCaptureProvider failed in InitializeWsk(): %d\n", status);
			break;
		}

	} while (false);

	return status;
}


/*
 * WskDeregister will wait to return until all the following have completed:
 * All captured instances of the provider NPI are released.
 * Any outstanding calls to functions pointed to by WSK_PROVIDER_DISPATCH members have returned.
 * All sockets are closed. 
 */
NTSTATUS TerminateWsk()
{
	NTSTATUS status = STATUS_SUCCESS;

	// TODO: Close open sockets (None exist for now)

	// For each successful npi capture, there must be a release before calling WskDeregister
	WskReleaseProviderNPI(&wskRegistration);
	WskDeregister(&wskRegistration);

	return status;
}


//NTSTATUS CreateConnectionSocket(PWSK_PROVIDER_NPI WskProviderNpi, PWSK_APP_SOCKET_CONTEXT SocketContext, PWSK_CLIENT_CONNECTION_DISPATCH Dispatch) // Not using event callbacks for now, hence the nullptr on Dispatch
NTSTATUS CreateConnectionSocket()
{
	NTSTATUS	status;
	PIRP		irp;

	// Allocate an IRP - Necessary for WSK operations
	irp = IoAllocateIrp(
		1,		// Stack size at least 1 for Wsk operations
		FALSE);

	if (!irp)
	{
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	// Set the completion routine for the IRP
	IoSetCompletionRoutine(irp, CreateConnectionSocketComplete, &socketContext, TRUE, TRUE, TRUE);

	// WskSocketConnect is used to create a connection oriented socket, bind it to a local transport address
	// and connect it to a remote transport address
	status = wskProviderNpi.Dispatch->WskSocket(
		wskProviderNpi.Client,
		AF_INET,						// IPPROTO_TCP is within this family
		SOCK_STREAM,					// Socket Type: Stream (TCP)
		IPPROTO_TCP,					// TCP protocol
		WSK_FLAG_CONNECTION_SOCKET,		// Connection socket
		&socketContext,
		nullptr,						// Not using callback so nullptr for now
		nullptr,
		nullptr,
		nullptr,
		irp);

	// Return status of call to WskSocket()
	return status;
}

// Socket creation IoCompletion routine
NTSTATUS CreateConnectionSocketComplete(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context)
{
	UNREFERENCED_PARAMETER(DeviceObject);

	PWSK_APP_SOCKET_CONTEXT SocketContext;

	// Check the result of the socket creation
	if (Irp->IoStatus.Status == STATUS_SUCCESS)
	{
		// Get the pointer to the socket context
		SocketContext =
			(PWSK_APP_SOCKET_CONTEXT)Context;

		// Save the socket object for the new socket
		SocketContext->Socket =
			(PWSK_SOCKET)(Irp->IoStatus.Information);

		// Set any socket options for the new socket


			// Enable any event callback functions on the new socket


			// Perform any other initializations

	}

	// Error status
	else
	{
		// Handle error

	}

	// Free the IRP
	IoFreeIrp(Irp);

	// Always return STATUS_MORE_PROCESSING_REQUIRED to
	// terminate the completion processing of the IRP.
	return STATUS_MORE_PROCESSING_REQUIRED;
}


// Function to bind a connection socket to a local transport address
NTSTATUS BindConnectionSocket()
{
	localAddress.sin_family = AF_INET;			// Ipv4 Family
	localAddress.sin_port = 0;					// Bind to port any
	localAddress.sin_addr.s_addr = INADDR_ANY;  // Bind to any local address

	PWSK_PROVIDER_LISTEN_DISPATCH Dispatch;
	PIRP Irp;
	NTSTATUS Status;

	// Get pointer to the socket's provider dispatch structure
	Dispatch =
		(PWSK_PROVIDER_LISTEN_DISPATCH)(socketContext->Socket->Dispatch);

	// Allocate an IRP
	Irp =
		IoAllocateIrp(
			1,		//Stack size must be at least 1 when working with Wsk operations
			FALSE
		);

	// Check result
	if (!Irp)
	{
		// Return error
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	// Set the completion routine for the IRP
	IoSetCompletionRoutine(
		Irp,
		BindComplete,
		socketContext->Socket,  // Use the socket object for the context
		TRUE,
		TRUE,
		TRUE
	);

	// Initiate the bind operation on the socket
	Status =
		Dispatch->WskBind(
			socketContext->Socket,
			(PSOCKADDR)&localAddress,
			0,  // No flags for wsk application
			Irp
		);

	// Return the status of the call to WskBind()
	return Status;
}



// Bind IoCompletion routine
NTSTATUS BindComplete(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context)
{
	UNREFERENCED_PARAMETER(DeviceObject);

	PWSK_SOCKET Socket;

	// Check the result of the bind operation
	if (Irp->IoStatus.Status == STATUS_SUCCESS)
	{
		// Get the socket object from the context
		Socket = (PWSK_SOCKET)Context;

		// Perform the next operation on the socket

	}

	// Error status
	else
	{
		// Handle error

	}

	// Free the IRP
	IoFreeIrp(Irp);

	// Always return STATUS_MORE_PROCESSING_REQUIRED to
	// terminate the completion processing of the IRP.
	return STATUS_MORE_PROCESSING_REQUIRED;
}

// Function to connect a socket to a remote transport address
NTSTATUS ConnectSocket(const char* RemoteIpAddress, short RemotePortNumber)
{
	PWSK_PROVIDER_CONNECTION_DISPATCH Dispatch;
	PIRP Irp;
	NTSTATUS Status;
	UNREFERENCED_PARAMETER(RemoteIpAddress); // TODO
	UNREFERENCED_PARAMETER(RemotePortNumber); // TODO

	//Set remote address
	remoteAddress.sin_family = AF_INET;
	remoteAddress.sin_port =Khtons((SHORT)9999);  // Port 9999
	remoteAddress.sin_addr.s_addr = Khtonl(0x7f000001);  // IP address 127.0.0.1



	// Get pointer to the socket's provider dispatch structure
	Dispatch =
		(PWSK_PROVIDER_CONNECTION_DISPATCH)(socketContext->Socket->Dispatch);

	// Allocate an IRP
	Irp =
		IoAllocateIrp(
			1,
			FALSE
		);

	// Check result
	if (!Irp)
	{
		// Return error
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	// Set the completion routine for the IRP
	IoSetCompletionRoutine(
		Irp,
		ConnectComplete,
		socketContext->Socket,  // Use the socket object for the context
		TRUE,
		TRUE,
		TRUE
	);

	// Initiate the connect operation on the socket
	Status =
		Dispatch->WskConnect(
			socketContext->Socket,
			(PSOCKADDR)&remoteAddress,
			0,  // No flags
			Irp
		);

	// Return the status of the call to WskConnect()
	return Status;
}


// Connect IoCompletion routine
NTSTATUS ConnectComplete(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context)
{
	UNREFERENCED_PARAMETER(DeviceObject);

	PWSK_SOCKET Socket;

	// Check the result of the connect operation
	if (Irp->IoStatus.Status == STATUS_SUCCESS)
	{
		// Get the socket object from the context
		Socket = (PWSK_SOCKET)Context;

		// Perform the next operation on the socket

	}

	// Error status
	else
	{
		// Handle error

	}

	// Free the IRP
	IoFreeIrp(Irp);

	// Always return STATUS_MORE_PROCESSING_REQUIRED to
	// terminate the completion processing of the IRP.
	return STATUS_MORE_PROCESSING_REQUIRED;
}
