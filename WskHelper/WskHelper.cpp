#include "pch.h"
#include "WskHelper.h"
#include "Conversions.h"

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

WSK_APP_SOCKET_CONTEXT socketContext;

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

	// Local variables
	NTSTATUS status = STATUS_SUCCESS;
	auto symLinkCreated = false;
	auto wskInitialized = false;

	do
	{
		status = IoCreateDevice(DriverObject, 0, &devName, FILE_DEVICE_UNKNOWN, 0, TRUE, &deviceObject);

		if (!NT_SUCCESS(status))
		{
			KdPrint((DRIVER_PREFIX "Failed to create device (0x%08X)\n", status));
			break;
		}

		// Set up direct I/O
		deviceObject->Flags |= DO_DIRECT_IO;
		status = IoCreateSymbolicLink(&symLinkName, &devName);

		if (!NT_SUCCESS(status))
		{
			DbgPrint("Failed to create device (0x%08X)\n", status);
			break;
		}

		status = InitializeWsk();

		if (!NT_SUCCESS(status))
		{
			DbgPrint("Failed to create symbolic link (0x%08X)\n", status);
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
	localAddress.sin_family = AF_INET;
	localAddress.sin_port = 0;               // Bind to port any
	localAddress.sin_addr.s_addr = INADDR_ANY;  // Bind to any local address

	PWSK_PROVIDER_LISTEN_DISPATCH Dispatch;
	PIRP Irp;
	NTSTATUS Status;

	// Get pointer to the socket's provider dispatch structure
	Dispatch =
		(PWSK_PROVIDER_LISTEN_DISPATCH)(socketContext.Socket->Dispatch);

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
		socketContext.Socket,  // Use the socket object for the context
		TRUE,
		TRUE,
		TRUE
	);

	// Initiate the bind operation on the socket
	Status =
		Dispatch->WskBind(
			socketContext.Socket,
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
	SOCKADDR_IN remoteAddr;
	remoteAddr.sin_family = AF_INET;
	remoteAddr.sin_port =Khtons((SHORT)9999);  // Port 9999
	remoteAddr.sin_addr.s_addr = Khtonl(0x7f000001);  // IP address 127.0.0.1



	// Get pointer to the socket's provider dispatch structure
	Dispatch =
		(PWSK_PROVIDER_CONNECTION_DISPATCH)(socketContext.Socket->Dispatch);

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
		socketContext.Socket,  // Use the socket object for the context
		TRUE,
		TRUE,
		TRUE
	);

	// Initiate the connect operation on the socket
	Status =
		Dispatch->WskConnect(
			socketContext.Socket,
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
