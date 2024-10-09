#pragma once

#define DRIVER_PREFIX "WSKH"


// Context structure for each socket
typedef struct _WSK_APP_SOCKET_CONTEXT {
	PWSK_SOCKET Socket;

} WSK_APP_SOCKET_CONTEXT, * PWSK_APP_SOCKET_CONTEXT;


// Driver dispatch routines
void WskHelperUnload(PDRIVER_OBJECT DriverObject);
NTSTATUS WskHelperDispatchCreate(PDEVICE_OBJECT, PIRP Irp);
NTSTATUS WskHelperDispatchClose(PDEVICE_OBJECT, PIRP Irp);

// Wsk related functions
NTSTATUS InitializeWsk();
NTSTATUS TerminateWsk();
NTSTATUS CreateConnectionSocket();
NTSTATUS CreateConnectionSocketComplete(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context);
NTSTATUS BindConnectionSocket();
NTSTATUS BindComplete(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context);
NTSTATUS ConnectSocket(const char* RemoteIpAddress, short RemotePortNumber);
NTSTATUS ConnectComplete(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context);

void PrintMessage();

