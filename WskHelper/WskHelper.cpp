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
const WSK_CLIENT_DISPATCH g_wskAppDispatch = {
  MAKE_WSK_VERSION(1,0), // Use WSK version 1.0
  0,		// Reserved
  nullptr	// WskClientEvent callback not required for WSK version 1.0
};



// WSK Registration object. Must be kept valid in memory
WSK_REGISTRATION g_wskRegistration;

PWSK_APP_SOCKET_CONTEXT g_socketContext;


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
			WskDeregister(&g_wskRegistration);
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
			status = WskCaptureProviderNPI(&g_wskRegistration, WSK_NO_WAIT, &wskProviderNpi);

			

			if (!NT_SUCCESS(status))
			{
				DbgPrint("WskCaptureProviderNpi failed (0x%08X)\n", status);
			}




			// WskSocketConnect can be used for the 3 steps below, but for educational purposes it will be done manually - for now

			// 2. Create the connection socket
			// Allocate memory for socketContext
			// Pointer to a driver - determined context to pass to the IoCompletion routine.
			// Context information must be stored in nonpaged memory, because the IoCompletion routine is called at IRQL <= DISPATCH_LEVEL.
			g_socketContext = (PWSK_APP_SOCKET_CONTEXT)ExAllocatePool2(
				POOL_FLAG_NON_PAGED, sizeof(WSK_APP_SOCKET_CONTEXT), 'ASOC');
			if (!g_socketContext)
			{
				DbgPrint("Allocating Socket context failed");
				status = STATUS_INSUFFICIENT_RESOURCES;
				break;
			}

			// Initialize the event for synchronization
			KeInitializeEvent(&g_socketContext->OperationCompleteEvent, NotificationEvent, FALSE);

			DbgPrint("CreateConnectionSocket\n");
			status = CreateConnectionSocket(&wskProviderNpi, g_socketContext, nullptr); // Not using event callbacks for now, hence the nullptr on Dispatch


			if (!NT_SUCCESS(status))
			{
				DbgPrint("CreateConnectionSocket failed (0x%08X)\n", status);
			}

			// 3. Bind the socket to a local transport address
			// Prepare the local transport address (e.g., bind to 127.0.0.1:8080)
			SOCKADDR_IN localAddr;
			localAddr.sin_family = AF_INET;
			localAddr.sin_port = 0;               // Bind to port any
			localAddr.sin_addr.s_addr = INADDR_ANY;  // Bind to any local address

			DbgPrint("BindConnectionSocket\n");
			status = BindConnectionSocket(g_socketContext, (PSOCKADDR)&localAddr);
			if (!NT_SUCCESS(status))
			{
				DbgPrint("BinConnectionSocket failed: (0x%08X)\n", status);
			}

			// 4. Create a connection with the socket
			SOCKADDR_IN remoteAddr;
			remoteAddr.sin_family = AF_INET;
			remoteAddr.sin_port = Khtons((SHORT)9999);  // Port 9999
			remoteAddr.sin_addr.s_addr = Khtonl(0x7f000001);  // IP address 127.0.0.1
			DbgPrint("ConnectSocket\n");

			if (g_socketContext->Socket == NULL) {
				DbgPrint("Socket is NULL after WskSocket call\n");
				return STATUS_INVALID_HANDLE;  // Or a similar error status
			}

			status = ConnectSocket(g_socketContext, (PSOCKADDR)&remoteAddr);
			DelayForMilliseconds(3000);
			if (!NT_SUCCESS(status))
			{
				DbgPrint("ConnectSocket failed: (0x%08X)\n", status);
			}

			break;
		}

		case IOCTL_WSKHELPER_SEND_DATA:
		{
			// 5. Send data over the socket connection oriented socket
			// Allocate memory for the data

			 // Access the input/output buffer
			PVOID IoData = Irp->AssociatedIrp.SystemBuffer;

			// Get the lengths of the input and output buffers
			ULONG inputBufferLength = irpSp->Parameters.DeviceIoControl.InputBufferLength;
			//ULONG outputBufferLength = irpSp->Parameters.DeviceIoControl.OutputBufferLength;

			// Ensure that we have input data to work with
			if (IoData != NULL && inputBufferLength > 0) {
				// Cast the buffer to a specific type if needed, here it's a char* for strings
				DbgPrint("Received input data: %s\n", (char*)IoData);

				// Optionally, modify the output buffer here if needed
				// E.g., modify the buffer in place if acting as an output buffer too
			}
			else {
				DbgPrint("No valid input data received.\n");
				status = STATUS_INVALID_PARAMETER;
			}

			PVOID bufferMemory = ExAllocatePool2(POOL_FLAG_NON_PAGED, inputBufferLength, '1gaT');
			if (!bufferMemory)
			{
				DbgPrint("Exallocatepool2 failed\n");
				return STATUS_INSUFFICIENT_RESOURCES; // Allocation failed
			}

			// Copy the data into the allocated memory
			RtlCopyMemory(bufferMemory, IoData, inputBufferLength);

			// Allocate an MDL for the buffer memory
			DbgPrint("IoAllocatedMdl\n");
			PMDL mdl = IoAllocateMdl(bufferMemory, inputBufferLength, FALSE, FALSE, NULL);
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
			dataBuffer.Length = inputBufferLength;           // Set the length of the buffer

			DbgPrint("SendData\n");
			status = SendData(g_socketContext, &dataBuffer);

			if (!NT_SUCCESS(status))
			{
				DbgPrint("SendData failed: (0x%08X)\n", status);
			}

			break;
		}

		case IOCTL_WSKHELPER_DISCONNECT:
		{
			DbgPrint("IOCTL_WSKHELPER_DISCONNECT\n");
			status = DisconnectSocket(g_socketContext);
			
			if (!NT_SUCCESS(status)) {
				DbgPrint("DisconnectSocket failed: (0x%08X)\n", status);
			}

			break;
		}

		case IOCTL_WSKHELPER_CLOSE:
		{
			DbgPrint("IOCTL_WSKHELPER_CLOSE\n");
			status = CloseSocket(g_socketContext);

			if (!NT_SUCCESS(status)) {
				DbgPrint("CloseSocket failed: (0x%08X)\n", status);
			}

			break;
		}
	}

	DbgPrint("WskHelper dispatch finished with status: (0x%08X)\n", status);
	return status;
}



NTSTATUS InitializeWsk() {
	NTSTATUS status = STATUS_SUCCESS;
	// Global WskProvider
	WSK_PROVIDER_NPI wskProviderNpi;

	// Necessary for registering this driver as a WSK app
	WSK_CLIENT_NPI wskClientNpi;

	do
	{
		// Register the WSK application
		wskClientNpi.ClientContext = nullptr;
		wskClientNpi.Dispatch = &g_wskAppDispatch;
		status = WskRegister(&wskClientNpi, &g_wskRegistration);

		if (!NT_SUCCESS(status)) {
			DbgPrint("WsKRegister failed in InitializeWsk(): %d\n", status);
			break;
		}

		// Capture provider
		status = WskCaptureProviderNPI(&g_wskRegistration, WSK_NO_WAIT, &wskProviderNpi);

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
	WskReleaseProviderNPI(&g_wskRegistration);
	WskDeregister(&g_wskRegistration);

	return status;
}
NTSTATUS CreateConnectionSocket(PWSK_PROVIDER_NPI WskProviderNpi, PWSK_APP_SOCKET_CONTEXT SocketContext, PWSK_CLIENT_CONNECTION_DISPATCH Dispatch)
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
	IoSetCompletionRoutine(irp,
		&CreateConnectionSocketComplete,
		SocketContext,	// Driver determined socket context
		TRUE,
		TRUE,
		TRUE);

	// WskSocketConnect is used to create a connection oriented socket, bind it to a local transport address
	// and connect it to a remote transport address
	status = WskProviderNpi->Dispatch->WskSocket(
		WskProviderNpi->Client,
		AF_INET,						// IPPROTO_TCP is within this family
		SOCK_STREAM,					// Socket Type: Stream (TCP)
		IPPROTO_TCP,					// TCP protocol
		WSK_FLAG_CONNECTION_SOCKET,		// Connection socket
		SocketContext,
		Dispatch,
		nullptr,
		nullptr,
		nullptr,
		irp);

	// If the WskSocket call can create a socket immediately it will return STATUS_SUCCESS
		// If the socket cannot be created right away it will return STATUS_PENDING and the socket
		// Will be contained in the Irp. See the Completion routine where the socket is fetched from the irp.
	if (status == STATUS_PENDING) {
		// Wait for the event to be signaled by the completion routine
		status = KeWaitForSingleObject(
			&SocketContext->OperationCompleteEvent,  // The event
			Executive,  // Wait at executive level
			KernelMode, // Kernel-mode wait
			FALSE,      // Non-alertable 
			NULL);      // No timeout
	}


	return status;
}

// Socket creation IoCompletion routine
NTSTATUS CreateConnectionSocketComplete(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context)
{
	UNREFERENCED_PARAMETER(DeviceObject);

	PWSK_APP_SOCKET_CONTEXT SocketContext; // Only local variable in the completion routine

	// Check the result of the socket creation
	DbgPrint("Irp status = (0x%08X)", Irp->IoStatus.Status);
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

		// Notify the waiter that the socket was created
		KeSetEvent(&SocketContext->OperationCompleteEvent, IO_NO_INCREMENT, FALSE);

		// Reset the event after the operation is complete
		//KeResetEvent(&SocketContext->OperationCompleteEvent);

	}

	// Error status
	else
	{
		// Notify the waiter that the socket was created
	

		// Reset the event after the operation is complete
		//KeResetEvent(&SocketContext->OperationCompleteEvent);

		// TODO: Handle error
		// TODO: What if status was not STATUS_SUCCESS, what should then be done
	}



	

	// Free the IRP
	IoFreeIrp(Irp);

	// Always return STATUS_MORE_PROCESSING_REQUIRED to
	// terminate the completion processing of the IRP.
	return STATUS_MORE_PROCESSING_REQUIRED;
}


// Function to bind a connection socket to a local transport address
NTSTATUS BindConnectionSocket(PWSK_APP_SOCKET_CONTEXT SocketContext, PSOCKADDR LocalAddress)
{
	PWSK_PROVIDER_CONNECTION_DISPATCH Dispatch;
	PIRP Irp;
	NTSTATUS status;

	// Initialize the event for synchronization
	KeResetEvent(&SocketContext->OperationCompleteEvent);

	// Get pointer to the socket's provider dispatch structure
	Dispatch = (PWSK_PROVIDER_CONNECTION_DISPATCH)(SocketContext->Socket->Dispatch);

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
		SocketContext,  // Use the socket context object for the context. This is because kewait needs notify in completion routine. Driver determined.
		TRUE,
		TRUE,
		TRUE
	);

	// Initiate the bind operation on the socket
	status =
		Dispatch->WskBind(
			SocketContext->Socket,
			LocalAddress,
			0,  // No flags for wsk application
			Irp
		);


	// If the WskSocket call can bind socket immediately it will return STATUS_SUCCESS
	// If the socket cannot be bound right away it will return STATUS_PENDING and the socket
	// Will be contained in the Irp. See the Completion routine where the socket is fetched from the irp.
	if (status == STATUS_PENDING) {
		// Wait for the event to be signaled by the completion routine
		status = KeWaitForSingleObject(
			&SocketContext->OperationCompleteEvent,  // The event
			Executive,  // Wait at executive level
			KernelMode, // Kernel-mode wait
			FALSE,      // Non-alertable
			NULL);      // No timeout
	}

	return status;
}



// Bind IoCompletion routine
NTSTATUS BindComplete(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context)
{
	UNREFERENCED_PARAMETER(DeviceObject);

	PWSK_APP_SOCKET_CONTEXT SocketContext;

	// Check the result of the bind operation
	if (Irp->IoStatus.Status == STATUS_SUCCESS)
	{
		// Get the socket context object from the context
		SocketContext = (PWSK_APP_SOCKET_CONTEXT)Context;

		// Save the socket object for the new socket in the socket context
		//SocketContext->Socket =(PWSK_SOCKET)(Irp->IoStatus.Information);


		// Perform the next operation on the socket

		// Notify the waiter that the socket was bound
		KeSetEvent(&SocketContext->OperationCompleteEvent, IO_NO_INCREMENT, FALSE);

	}

	// Error status
	else
	{
		// TODO: Handle error
		// TODO: What if the socket was not bound and status was not STATUS_SUCCESS

	}

	// Free the IRP
	IoFreeIrp(Irp);

	// Always return STATUS_MORE_PROCESSING_REQUIRED to
	// terminate the completion processing of the IRP.
	return STATUS_MORE_PROCESSING_REQUIRED;
}

// Function to connect a socket to a remote transport address
NTSTATUS ConnectSocket(PWSK_APP_SOCKET_CONTEXT SocketContext, PSOCKADDR RemoteAddress)
{
	PWSK_PROVIDER_CONNECTION_DISPATCH Dispatch;
	PIRP Irp;
	NTSTATUS status = INVALID_KERNEL_HANDLE;

	do {
		if (!SocketContext->Socket->Dispatch) {
			DbgPrint("Dispatch was nullptr");
			break;
		}

		// Get pointer to the socket's provider dispatch structure
		// TODO: CHECK FOR NULL
		Dispatch =
			(PWSK_PROVIDER_CONNECTION_DISPATCH)(SocketContext->Socket->Dispatch); // This should be a connection socket TODO: ARE WE USING DISPATCH???


		// Initialize the event for synchronization
		KeResetEvent(&SocketContext->OperationCompleteEvent);

		
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
			SocketContext,  // Use the socket object for the context
			TRUE,
			TRUE,
			TRUE
		);

		// Initiate the connect operation on the socket
		status =
			Dispatch->WskConnect(
				SocketContext->Socket,
				RemoteAddress,
				0,  // No flags
				Irp
			);

		// TODO: Might need to destroy ke event if STATUS_SUCCESS

		// If the WskSocket call can bind socket immediately it will return STATUS_SUCCESS
		// If the socket cannot be bound right away it will return STATUS_PENDING and the socket
		// Will be contained in the Irp. See the Completion routine where the socket is fetched from the irp.
		if (status == STATUS_PENDING) {
			// Wait for the event to be signaled by the completion routine
			status = KeWaitForSingleObject(
				&SocketContext->OperationCompleteEvent,  // The event
				Executive,  // Wait at executive level
				KernelMode, // Kernel-mode wait
				FALSE,      // Non-alertable
				NULL);      // No timeout
		}

	} while (false);
	return status;
}


// Connect IoCompletion routine
NTSTATUS ConnectComplete(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context)
{
	UNREFERENCED_PARAMETER(DeviceObject);

	PWSK_APP_SOCKET_CONTEXT SocketContext;

	// Check the result of the connect operation
	if (Irp->IoStatus.Status == STATUS_SUCCESS)
	{
		// Get the socket object from the context
		SocketContext = (PWSK_APP_SOCKET_CONTEXT)Context;
		
		//SocketContext->Socket = (PWSK_SOCKET)Irp->IoStatus.Information;

		// Perform the next operation on the socket

			// Notify the waiter that the socket was bound
		KeSetEvent(&SocketContext->OperationCompleteEvent, IO_NO_INCREMENT, FALSE);



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


// Function to send data
NTSTATUS SendData(PWSK_APP_SOCKET_CONTEXT SocketContext, PWSK_BUF DataBuffer)
{
	PWSK_PROVIDER_CONNECTION_DISPATCH Dispatch;
	PIRP Irp;
	NTSTATUS status;

	// Initialize the event for synchronization
	KeResetEvent(&SocketContext->OperationCompleteEvent);

	// Get pointer to the provider dispatch structure
	Dispatch =
		(PWSK_PROVIDER_CONNECTION_DISPATCH)(SocketContext->Socket->Dispatch);

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
		DbgPrint("Irp allocation failed\n");
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	// Set the completion routine for the IRP
	IoSetCompletionRoutine(
		Irp,
		SendComplete,
		SocketContext,  // Use the SocketContext testing// data buffer for the context
		TRUE,
		TRUE,
		TRUE
	);

	// Initiate the send operation on the socket
	DbgPrint("Calling WskSend\n");
	status =
		Dispatch->WskSend(
			SocketContext->Socket,
			DataBuffer,
			0,  // No flags
			Irp
		);

	// If the WskSocket call can bind socket immediately it will return STATUS_SUCCESS
	// If the socket cannot be bound right away it will return STATUS_PENDING and the socket
	// Will be contained in the Irp. See the Completion routine where the socket is fetched from the irp.
	if (status == STATUS_PENDING) {
		// Wait for the event to be signaled by the completion routine
		status = KeWaitForSingleObject(
			&SocketContext->OperationCompleteEvent,  // The event
			Executive,  // Wait at executive level
			KernelMode, // Kernel-mode wait
			FALSE,      // Non-alertable
			NULL);      // No timeout
	}

	// Return the status of the call to WskSend()
	DbgPrint("Send operation completed with status: 0x%08X\n", status);
	return status;
}

// Send IoCompletion routine
NTSTATUS
SendComplete(
	PDEVICE_OBJECT DeviceObject,
	PIRP Irp,
	PVOID Context
)
{
	UNREFERENCED_PARAMETER(DeviceObject);

	//PWSK_BUF DataBuffer;
	ULONG ByteCount;
	PWSK_APP_SOCKET_CONTEXT SocketContext;

	// Check the result of the send operation
	if (Irp->IoStatus.Status == STATUS_SUCCESS)
	{
		// Get the pointer to the data buffer
		//DataBuffer = (PWSK_BUF)Context;
		SocketContext = (PWSK_APP_SOCKET_CONTEXT)Context;

		// Get the number of bytes sent
		ByteCount = (ULONG)(Irp->IoStatus.Information);

		// Re-use or free the data buffer


		// Notify the waiter that the socket was bound
		KeSetEvent(&SocketContext->OperationCompleteEvent, IO_NO_INCREMENT, FALSE);
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

// Function to disconnect a socket from a remote transport address
NTSTATUS
DisconnectSocket(
	PWSK_APP_SOCKET_CONTEXT SocketContext
)
{
	PWSK_PROVIDER_CONNECTION_DISPATCH Dispatch;
	PIRP Irp;
	NTSTATUS status;

	// Initialize the event for synchronization
	KeResetEvent(&SocketContext->OperationCompleteEvent);

	// Get pointer to the socket's provider dispatch structure
	Dispatch =
		(PWSK_PROVIDER_CONNECTION_DISPATCH)(SocketContext->Socket->Dispatch);

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
		DisconnectComplete,
		SocketContext,  // Use the socket object for the context
		TRUE,
		TRUE,
		TRUE
	);

	// Initiate the disconnect operation on the socket
	status =
		Dispatch->WskDisconnect(
			SocketContext->Socket,
			NULL,  // No final data to be transmitted
			0,     // No flags (graceful disconnect)
			Irp
		);

	// If the WskSocket call can bind socket immediately it will return STATUS_SUCCESS
	// If the socket cannot be bound right away it will return STATUS_PENDING and the socket
	// Will be contained in the Irp. See the Completion routine where the socket is fetched from the irp.
	if (status == STATUS_PENDING) {
		// Wait for the event to be signaled by the completion routine
		status = KeWaitForSingleObject(
			&SocketContext->OperationCompleteEvent,  // The event
			Executive,  // Wait at executive level
			KernelMode, // Kernel-mode wait
			FALSE,      // Non-alertable
			NULL);      // No timeout
	}

	return status;
}

// Disconnect IoCompletion routine
NTSTATUS
DisconnectComplete(
	PDEVICE_OBJECT DeviceObject,
	PIRP Irp,
	PVOID Context
)
{
	UNREFERENCED_PARAMETER(DeviceObject);

	PWSK_APP_SOCKET_CONTEXT socketContext;

	// Check the result of the disconnect operation
	if (Irp->IoStatus.Status == STATUS_SUCCESS)
	{
		// Get the socket object from the context
		socketContext = (PWSK_APP_SOCKET_CONTEXT)Context;

		// Perform the next operation on the socket
		//...


		// Notify the waiter that the socket was bound
		KeSetEvent(&socketContext->OperationCompleteEvent, IO_NO_INCREMENT, FALSE);

	}

	// Error status
	else
	{
		// Handle error
		//...
	}

	// Free the IRP
	IoFreeIrp(Irp);

	// Always return STATUS_MORE_PROCESSING_REQUIRED to
	// terminate the completion processing of the IRP.
	return STATUS_MORE_PROCESSING_REQUIRED;
}

// Function to close a socket
NTSTATUS
CloseSocket(
	PWSK_APP_SOCKET_CONTEXT SocketContext
)
{
	PWSK_PROVIDER_BASIC_DISPATCH Dispatch;
	PIRP Irp;
	NTSTATUS status;

	// Initialize the event for synchronization
	KeResetEvent(&SocketContext->OperationCompleteEvent);

	// Get pointer to the socket's provider dispatch structure
	Dispatch =
		(PWSK_PROVIDER_BASIC_DISPATCH)(SocketContext->Socket->Dispatch);

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
		CloseSocketComplete,
		SocketContext,
		TRUE,
		TRUE,
		TRUE
	);

	// Initiate the close operation on the socket
	status =
		Dispatch->WskCloseSocket(
			SocketContext->Socket,
			Irp
		);

	// If the WskSocket call can bind socket immediately it will return STATUS_SUCCESS
	// If the socket cannot be bound right away it will return STATUS_PENDING and the socket
	// Will be contained in the Irp. See the Completion routine where the socket is fetched from the irp.
	if (status == STATUS_PENDING) {
		// Wait for the event to be signaled by the completion routine
		status = KeWaitForSingleObject(
			&SocketContext->OperationCompleteEvent,  // The event
			Executive,  // Wait at executive level
			KernelMode, // Kernel-mode wait
			FALSE,      // Non-alertable
			NULL);      // No timeout
	}

	return status;
}

// Socket close IoCompletion routine
NTSTATUS
CloseSocketComplete(
	PDEVICE_OBJECT DeviceObject,
	PIRP Irp,
	PVOID Context
)
{
	UNREFERENCED_PARAMETER(DeviceObject);

	PWSK_APP_SOCKET_CONTEXT socketContext;

	// Check the result of the socket close operation
	if (Irp->IoStatus.Status == STATUS_SUCCESS)
	{
		// Get the pointer to the socket context
		socketContext =
			(PWSK_APP_SOCKET_CONTEXT)Context;

		// Perform any cleanup and/or deallocation of the socket context
		//...


		// Notify the waiter that the socket was bound
		KeSetEvent(&socketContext->OperationCompleteEvent, IO_NO_INCREMENT, FALSE);
	}

	// Error status
	else
	{
		// Handle error
		//...
	}

	// Free the IRP
	IoFreeIrp(Irp);

	// Always return STATUS_MORE_PROCESSING_REQUIRED to
	// terminate the completion processing of the IRP.
	return STATUS_MORE_PROCESSING_REQUIRED;
}