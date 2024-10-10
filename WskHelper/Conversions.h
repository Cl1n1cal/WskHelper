#pragma once
#include "pch.h"

// Kernel is just an added prefix so the function does not get mixed up
// with the user mode variant

// Kernel Host Short to Network Short
USHORT Khtons(USHORT HostShort);

// Kernel Host Long to Network Long
ULONG Khtonl(ULONG HostLong);

// Kernel Net Short to Host Short
USHORT Kntohs(USHORT NetShort);

// Kernel Net Long to Host Long
ULONG Kntohl(ULONG NetLong);

ULONG ipv4_to_network_long(const char* ip_addr);