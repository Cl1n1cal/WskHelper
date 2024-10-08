#pragma once

// Context structure for each socket
typedef struct _WSK_APP_SOCKET_CONTEXT {
	PWSK_SOCKET Socket;

} WSK_APP_SOCKET_CONTEXT, * PWSK_APP_SOCKET_CONTEXT;


NTSTATUS CreateConnectionSocket(PWSK_PROVIDER_NPI WskProviderNpi, PWSK_APP_SOCKET_CONTEXT SocketContext, PWSK_CLIENT_CONNECTION_DISPATCH Dispatch);
NTSTATUS CreateConnectionSocketComplete(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context);
NTSTATUS InitializeWsk();

void PrintMessage();

