#pragma once


/*
 * Specifying Device Types
 * If a type of hardware does not match any of the defined types,
 * specify a value of either FILE_DEVICE_UNKNOWN,
 * or a value within the range of 32768 through 65535.
 * We will define a value above 32768
 * that ends in 22 for FILE_DEVICE_UNKNOWN since this is a software driver
 * 0x8000 = 32768 is the first value available to us
 * And ending in 0x22 we get: 0x8022
 */
#define DEVICE_WSKHELPER 0x8022



 /*
  * FunctionCode
  * Identifies the function to be performed by the driver.3
  * Values of less than 0x800 are reserved for Microsoft.
  * Values of 0x800 and higher can be used by vendors.
  * Note that the vendor-assigned values set the Custom bit.
  */
#define IOCTL_WSKHELPER_CREATE_CONNECTION	CTL_CODE(DEVICE_WSKHELPER, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_WSKHELPER_CREATE_LISTENER		CTL_CODE(DEVICE_WSKHELPER, 0x802, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_WSKHELPER_SEND_DATA			CTL_CODE(DEVICE_WSKHELPER, 0x803, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_WSKHELPER_DISCONNECT			CTL_CODE(DEVICE_WSKHELPER, 0x804, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_WSKHELPER_CLOSE				CTL_CODE(DEVICE_WSKHELPER, 0x805, METHOD_BUFFERED, FILE_ANY_ACCESS)

struct Message
{
	char Msg[1024];
};