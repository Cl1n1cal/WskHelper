#pragma once

#define DRIVER_PREFIX_WSKHELPER "WSKH"


// TODO: Include localaddr and remoteaddr in the context to reduce amount og global variables
// Context structure for each socket
typedef struct _WSK_APP_SOCKET_CONTEXT {
	PWSK_SOCKET Socket;
	KEVENT OperationCompleteEvent;

} WSK_APP_SOCKET_CONTEXT, * PWSK_APP_SOCKET_CONTEXT;


// Driver dispatch routines
void WskHelperUnload(PDRIVER_OBJECT DriverObject);
NTSTATUS WskHelperDispatchCreate(PDEVICE_OBJECT, PIRP Irp);
NTSTATUS WskHelperDispatchClose(PDEVICE_OBJECT, PIRP Irp);
NTSTATUS WskHelperDispatchDeviceControl(PDEVICE_OBJECT, PIRP Irp);

// Wsk related functions
NTSTATUS InitializeWsk();
NTSTATUS TerminateWsk();
NTSTATUS CreateConnectionSocket(PWSK_PROVIDER_NPI WskProviderNpi, PWSK_APP_SOCKET_CONTEXT SocketContext, PWSK_CLIENT_CONNECTION_DISPATCH Dispatch);
NTSTATUS CreateConnectionSocketComplete(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context); // Prototype for the socvisuket creation IoCompletion routine
NTSTATUS BindConnectionSocket(PWSK_APP_SOCKET_CONTEXT SocketContext, PSOCKADDR LocalAddress);
NTSTATUS BindComplete(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context); // Prototype for the bind IoCompletion routine
NTSTATUS ConnectSocket(PWSK_APP_SOCKET_CONTEXT SocketContext, PSOCKADDR RemoteAddress); // Function to connect a socket to a remote transport address
NTSTATUS ConnectComplete(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context); // Prototype for the connect IoCompletion routine
NTSTATUS SendData(PWSK_APP_SOCKET_CONTEXT SocketContext, PWSK_BUF DataBuffer); // Function to send data
NTSTATUS SendComplete(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context); // Prototype for the send IoCompletion routine
NTSTATUS DisconnectSocket(PWSK_APP_SOCKET_CONTEXT SocketContext);
NTSTATUS DisconnectComplete(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context);
NTSTATUS CloseSocket(PWSK_SOCKET Socket, PWSK_APP_SOCKET_CONTEXT SocketContext);
NTSTATUS CloseSocketComplete(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context);

NTSTATUS DelayForMilliseconds(LONG milliseconds);
