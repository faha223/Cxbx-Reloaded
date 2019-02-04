// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
// ******************************************************************
// *
// *    .,-:::::    .,::      .::::::::.    .,::      .:
// *  ,;;;'````'    `;;;,  .,;;  ;;;'';;'   `;;;,  .,;;
// *  [[[             '[[,,[['   [[[__[[\.    '[[,,[['
// *  $$$              Y$$$P     $$""""Y$$     Y$$$P
// *  `88bo,__,o,    oP"``"Yo,  _88o,,od8P   oP"``"Yo,
// *    "YUMMMMMP",m"       "Mm,""YUMMMP" ,m"       "Mm,
// *
// *   Common->VerifyAddressRanges.cpp
// *
// *  This file is part of the Cxbx project.
// *
// *  Cxbx is free software; you can redistribute it
// *  and/or modify it under the terms of the GNU General Public
// *  License as published by the Free Software Foundation; either
// *  version 2 of the license, or (at your option) any later version.
// *
// *  This program is distributed in the hope that it will be useful,
// *  but WITHOUT ANY WARRANTY; without even the implied warranty of
// *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// *  GNU General Public License for more details.
// *
// *  You should have recieved a copy of the GNU General Public License
// *  along with this program; see the file COPYING.
// *  If not, write to the Free Software Foundation, Inc.,
// *  59 Temple Place - Suite 330, Bostom, MA 02111-1307, USA.
// *
// *  (c) 2017-2019 Patrick van Logchem <pvanlogchem@gmail.com>
// *
// *  All rights reserved
// *
// ******************************************************************

#include "AddressRanges.h"

bool VerifyAddressRange(int index)
{
	unsigned __int32 BaseAddress = XboxAddressRanges[index].Start;
	int Size = XboxAddressRanges[index].Size;

	if (BaseAddress == 0) {
		// The zero page (the entire first 64 KB block) can't be verified
		BaseAddress += BLOCK_SIZE;
		Size -= BLOCK_SIZE;
	}

	// Verify this range in 64 Kb block increments, as they are supposed
	// to have been reserved like that too:
	const DWORD AllocationProtect = (XboxAddressRanges[index].Start == 0) ? PAGE_EXECUTE_WRITECOPY : XboxAddressRanges[index].InitialMemoryProtection;
	MEMORY_BASIC_INFORMATION mbi;
	while (Size > 0) {
		// Expected values
		PVOID AllocationBase = (PVOID)BaseAddress;
		SIZE_T RegionSize = (SIZE_T)(Size > BLOCK_SIZE) ? BLOCK_SIZE : (Size > PAGE_SIZE) ? Size : PAGE_SIZE;
		DWORD State = MEM_RESERVE;
		DWORD Protect = 0;
		DWORD Type = MEM_PRIVATE;

		// Allowed deviations
		if (XboxAddressRanges[index].Start == 0) {
			AllocationBase = (PVOID)0x10000;
			switch (BaseAddress) {
				case 0x10000: // Image Header
					RegionSize = PAGE_SIZE;
					Protect = PAGE_READONLY;
					break;
				case 0x11000: // section .textbss
					RegionSize = BLOCK_SIZE;
					Protect = PAGE_EXECUTE_READWRITE;
					break;
				case 0x21000: // section .text
					RegionSize = 0x2000;
					Protect = PAGE_EXECUTE_READ;
					break;
				case 0x23000: // section .data 
					RegionSize = PAGE_SIZE;
					Protect = PAGE_READONLY;
					break;
				case 0x24000: // section .idata
					RegionSize = 0x0999b000; // == ? - ?
					Protect = PAGE_READWRITE;
					break;
				}

			State = MEM_COMMIT;
			Type = MEM_IMAGE;
			break;
		}

		// Verify each block
		bool Okay = (VirtualQuery((LPVOID)BaseAddress, &mbi, sizeof(mbi)) != 0);
		if (Okay) 
			Okay = (mbi.BaseAddress == (LPVOID)BaseAddress);
		if (Okay) 
			Okay = (mbi.AllocationBase == AllocationBase);
		if (Okay) 
			Okay = (mbi.AllocationProtect == AllocationProtect);
		if (Okay) 
			Okay = (mbi.RegionSize == RegionSize);
		if (Okay) 
			Okay = (mbi.State == State);
		if (Okay) 
			Okay = (mbi.Protect == Protect);
		if (Okay) 
			Okay = (mbi.Type == Type);

		if (!Okay)
			if (!IsOptionalAddressRange(index))
				return false;

		// Handle the next region
		BaseAddress += RegionSize;
		Size -= RegionSize;
	}

	return true;
}

bool VerifyAddressRanges(const int system)
{
	// Loop over all Xbox address ranges
	for (int i = 0; i < ARRAY_SIZE(XboxAddressRanges); i++) {
		// Skip address ranges that don't match the given flags
		if (!AddressRangeMatchesFlags(i, system))
			continue;

		if (!VerifyAddressRange(i)) {
			if (!IsOptionalAddressRange(i)) {
				return false;
			}
		}
	}

	return true;
}

void UnreserveMemoryRange(const int index)
{
	unsigned __int32 Start = XboxAddressRanges[index].Start;
	int Size = XboxAddressRanges[index].Size;

	while (Size > 0) {
		VirtualFree((LPVOID)Start, (SIZE_T)0, MEM_RELEASE); // To release, dwSize must be zero!
		Start += BLOCK_SIZE;
		Size -= BLOCK_SIZE;
	}
}

bool AllocateMemoryRange(const int index)
{
	unsigned __int32 Start = XboxAddressRanges[index].Start;
	int Size = XboxAddressRanges[index].Size;

	while (Size > 0) {
		int BlockSize = (Size > BLOCK_SIZE) ? BLOCK_SIZE : Size;
		if (nullptr == VirtualAlloc((void*)Start, BlockSize, MEM_COMMIT, PAGE_READWRITE)) { // MEM_RESERVE already done by Cxbx-Loader.exe
			return false;
		}

		Start += BLOCK_SIZE;
		Size -= BLOCK_SIZE;
	}

	return true;
}