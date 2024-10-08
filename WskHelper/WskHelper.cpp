#include "pch.h"
#include "WskHelper.h"
#include "Conversions.h"


void PrintMessage() {
	DbgPrint("Hello wolrd");
}

// WSK Client Dispatch table that denotes the WSK version
// Must be kept valid in memory until WskDeregister has been called and registration is no longer valid
const WSK_CLIENT_DISPATCH WskAppDispatch = {
  MAKE_WSK_VERSION(1,0), // Use WSK version 1.0
  0,		// Reserved
  nullptr	// WskClientEvent callback not required for WSK version 1.0
};

// Global WskProvider
WSK_PROVIDER_NPI wskProviderNpi;

// Necessary for registering this driver as a WSK app
WSK_CLIENT_NPI wskClientNpi;

// WSK Registration object. Must be kept valid in memory
WSK_REGISTRATION WskRegistration;

NTSTATUS InitializeWsk() {
	NTSTATUS status = STATUS_SUCCESS;

	// Register the WSK application
	wskClientNpi.ClientContext = nullptr;
	wskClientNpi.Dispatch = &WskAppDispatch;
	status = WskRegister(&wskClientNpi, &WskRegistration);

	if (!NT_SUCCESS(status)) {
		DbgPrint("WsKRegister failed in InitializeWsk(): %d\n", status);
		return status;
	}

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
	IoSetCompletionRoutine(irp, CreateConnectionSocketComplete, SocketContext, TRUE, TRUE, TRUE);

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