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

// Function to convert IPv4 address string to network long (ULONG in network byte order)
ULONG ipv4_to_network_long(const char* ip_addr) {
    ULONG network_long = 0;
    NTSTATUS status;
    in_addr addr;  // Structure to hold the IPv4 address in binary format
    const char* terminator;

    // Convert IPv4 address string to in_addr structure
    status = RtlIpv4StringToAddressA(ip_addr, TRUE, &terminator, &addr);

    if (NT_SUCCESS(status)) {
        // The s_addr field of in_addr is already in network byte order
        network_long = addr.s_addr;
    }
    else {
        DbgPrint("Invalid IPv4 address: %s\n", ip_addr);
    }

    return network_long;  // Already in network byte order
}