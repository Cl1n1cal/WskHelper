#include "pch.h"

// Function to convert 16-bit integer from host byte order to network byte order
USHORT Khtons(USHORT HostShort) {
	return (HostShort << 8) | (HostShort >> 8);
}

// Function to convert 32-bit integer from host byte order to network byte order
ULONG Khtonl(ULONG HostLong) {
	return ((HostLong << 24) & 0xFF000000) |
		((HostLong << 8) & 0x00FF0000) |
		((HostLong >> 8) & 0x0000FF00) |
		((HostLong >> 24) & 0x000000FF);
}

// Function to convert 16-bit integer from network byte order to host byte order
USHORT Kntohs(USHORT NetShort) {
	return Khtons(NetShort); // Reuse the same function since it's symmetric
}

// Function to convert 32-bit integer from network byte order to host byte order
ULONG Kntohl(ULONG NetLong) {
	return Khtonl(NetLong); // Reuse the same function since it's symmetric
}