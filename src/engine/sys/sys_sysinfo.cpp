/*
===========================================================================

OpenWolf GPL Source Code
Copyright (C) 2007 HermitWorks Entertainment Corporation
Copyright (C) 2011 Dusan Jocic

OpenWolf is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

OpenWolf is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

===========================================================================
*/

#include "sys_local.h"
#include "sys_win32.h"
#include "../qcommon/qcommon.h"
#ifdef WIN32
#include <windows.h>
#endif

/*
===============
PrintRawHexData
===============
*/
static void PrintRawHexData( const byte *buf, size_t buf_len ) {
	uint i;

	for( i = 0; i < buf_len; i++ ) {
		Com_Printf( "%02X", (int)buf[i] );
	}
}

/*
===============
PrintCpuInfoFromRegistry
===============
*/
static qboolean PrintCpuInfoFromRegistry( void ) {
	DWORD i, numPrinted;
	HKEY kCpus;

	char name_buf[256];
	DWORD name_buf_len;

	if( RegOpenKeyEx( HKEY_LOCAL_MACHINE, "Hardware\\Description\\System\\CentralProcessor",
		0, KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE, &kCpus ) != ERROR_SUCCESS ) {
			return qfalse;
	}

	numPrinted = 0;
	for( i = 0; name_buf_len = lengthof( name_buf ),
		RegEnumKeyEx( kCpus, i, name_buf, &name_buf_len,
		NULL, NULL, NULL, NULL ) == ERROR_SUCCESS; i++ ) {
			HKEY kCpu;
		
			int value_buf_i[256];
			LPBYTE value_buf = (LPBYTE)value_buf_i;
			DWORD value_buf_len;

			if( RegOpenKeyEx( kCpus, name_buf, 0, KEY_QUERY_VALUE, &kCpu ) != ERROR_SUCCESS ) {
				continue;
			}

			Com_Printf( "    Processor %i:\n", (int)i );

			value_buf_len = sizeof( value_buf_i );
			if( RegQueryValueEx( kCpu, "ProcessorNameString", NULL, NULL, value_buf, &value_buf_len ) == ERROR_SUCCESS ) {
				Com_Printf( "        Name: %s\n", value_buf );
			}

			value_buf_len = sizeof( value_buf_i );
			if( RegQueryValueEx( kCpu, "~MHz", NULL, NULL, value_buf, &value_buf_len ) == ERROR_SUCCESS ) {
				Com_Printf( "        Speed: %i MHz\n", (int)*(DWORD*)value_buf_i );
			}

			value_buf_len = sizeof( value_buf_i );
			if( RegQueryValueEx( kCpu, "VendorIdentifier", NULL, NULL, value_buf, &value_buf_len ) == ERROR_SUCCESS ) {
				Com_Printf( "        Vendor: %s\n", value_buf );
			}

			value_buf_len = sizeof( value_buf_i );
			if( RegQueryValueEx( kCpu, "Identifier", NULL, NULL, value_buf, &value_buf_len ) == ERROR_SUCCESS ) {
				Com_Printf( "        Identifier: %s\n", value_buf );
			}

			value_buf_len = sizeof( value_buf_i );
			if( RegQueryValueEx( kCpu, "FeatureSet", NULL, NULL, value_buf, &value_buf_len ) == ERROR_SUCCESS ) {
				Com_Printf( "        Feature Bits: %08X\n", (int)*(DWORD*)value_buf_i );
			}

			RegCloseKey( kCpu );

			numPrinted++;
	}

	RegCloseKey( kCpus );

	return (qboolean)(numPrinted > 0);
}

/*
===============
Sys_PrintCpuInfo
===============
*/
void Sys_PrintCpuInfo( void ) {
	SYSTEM_INFO si;

	GetSystemInfo( &si );

	if( si.dwNumberOfProcessors == 1 ) {
		Com_Printf( "Processor:\n" );
	} else {
		Com_Printf( "Processors (%i):\n", (int)si.dwNumberOfProcessors );
	}

	if( PrintCpuInfoFromRegistry() )
		return;

	Com_Printf( "        Architecture: " );

	switch( si.wProcessorArchitecture ) {
		case PROCESSOR_ARCHITECTURE_INTEL:
			Com_Printf( "x86" );
			break;

		case PROCESSOR_ARCHITECTURE_MIPS:
			Com_Printf( "MIPS" );
			break;

		case PROCESSOR_ARCHITECTURE_ALPHA:
			Com_Printf( "ALPHA" );
			break;

		case PROCESSOR_ARCHITECTURE_PPC:
			Com_Printf( "PPC" );
			break;

		case PROCESSOR_ARCHITECTURE_SHX:
			Com_Printf( "SHX" );
			break;

		case PROCESSOR_ARCHITECTURE_ARM:
			Com_Printf( "ARM" );
			break;

		case PROCESSOR_ARCHITECTURE_IA64:
			Com_Printf( "IA64" );
			break;

		case PROCESSOR_ARCHITECTURE_ALPHA64:
			Com_Printf( "ALPHA64" );
			break;

		case PROCESSOR_ARCHITECTURE_MSIL:
			Com_Printf( "MSIL" );
			break;

		case PROCESSOR_ARCHITECTURE_AMD64:
			Com_Printf( "x64" );
			break;

		case PROCESSOR_ARCHITECTURE_IA32_ON_WIN64:
			Com_Printf( "WoW64" );
			break;

		default:
			Com_Printf( "UNKNOWN: %i", (int)si.wProcessorArchitecture );
			break;
	}

	Com_Printf( "\n" );

	Com_Printf( "        Revision: %04X\n", (int)si.wProcessorRevision );
}

/*
===============
Sys_PrintOSInfo
===============
*/
void Sys_PrintOSInfo ( void ) {
	OSVERSIONINFOEX	osVersionInfo;
	SYSTEM_INFO		si;

	// Get OS version info
	osVersionInfo.dwOSVersionInfoSize = sizeof( OSVERSIONINFOEX );
	ZeroMemory(&si, sizeof(SYSTEM_INFO));

	if (!GetVersionEx((OSVERSIONINFO *)&osVersionInfo)) {
		Sys_Error( "Couldn't get OS info" );
	}

	if (osVersionInfo.dwPlatformId != VER_PLATFORM_WIN32_NT) {
		Com_Error( ERR_FATAL, PRODUCT_NAME " requires Windows XP or later");
	}
	if ( osVersionInfo.dwMajorVersion < 5 || (osVersionInfo.dwMajorVersion == 5 && osVersionInfo.dwMinorVersion < 1)) {
		Sys_Error( PRODUCT_NAME " requires Windows XP or later");
	}

	// Windows XP
	if (osVersionInfo.dwMajorVersion == 5 && osVersionInfo.dwMinorVersion == 1) {
		Cvar_Set("sys_osVersion", "Windows XP");
		Com_Printf("Operating System:    Windows XP\n");
	} else if (osVersionInfo.dwMajorVersion == 5 && osVersionInfo.dwMinorVersion == 2) {
		if (osVersionInfo.wProductType == VER_NT_WORKSTATION) {
			Cvar_Set("sys_osVersion", "Windows XP");
			Com_Printf("Operating System:    Windows XP\n");
		} else {
			Cvar_Set("sys_osVersion", "Windows Server 2003");
			Com_Printf("Operating System:    Windows Server 2003\n");
		}
	}
	// Windows Server editions
	if ( osVersionInfo.dwMajorVersion == 5 && osVersionInfo.dwMinorVersion == 2 ) {
		if( GetSystemMetrics(SM_SERVERR2) ) {
			Cvar_Set("sys_osVersion", "Windows Server 2003 R2");
			Com_Printf("Operating System:    Windows Server 2003 R2\n");
		} else if ( osVersionInfo.wSuiteMask & VER_SUITE_STORAGE_SERVER ) {
			Cvar_Set("sys_osVersion", "Windows Storage Server 2003");
			Com_Printf("Operating System:    Windows Storage Server 2003\n");
		} else if ( osVersionInfo.wSuiteMask & VER_SUITE_WH_SERVER ) {
			Cvar_Set("sys_osVersion", "Windows Home Server");
			Com_Printf("Operating System:    Windows Home Server\n");
		} else if( osVersionInfo.wProductType == VER_NT_WORKSTATION && si.wProcessorArchitecture==PROCESSOR_ARCHITECTURE_AMD64) {
			Cvar_Set("sys_osVersion", "Windows XP Professional x64 Edition");
			Com_Printf("Operating System:    Windows XP Professional x64 Edition\n");
		}
		else {
			Cvar_Set("sys_osVersion", "Windows Server 2003");
			Com_Printf("Operating System:    Windows Server 2003\n");
		}
	}

	// Windows Vista
    if ( osVersionInfo.dwMajorVersion == 6 && osVersionInfo.dwMinorVersion == 0 ) {
		if( osVersionInfo.wProductType == VER_NT_WORKSTATION ) {
			Cvar_Set("sys_osVersion", "Windows Vista");
			Com_Printf("Operating System:    Windows Vista\n");
		} else {
			Cvar_Set("sys_osVersion", "Windows Server 2008" );
			Com_Printf("Operating System:    Windows Server 2008\n");
		}
	}

	// Windows 7
	if ( osVersionInfo.dwMajorVersion == 6 && osVersionInfo.dwMinorVersion == 1 ) {
		if( osVersionInfo.wProductType == VER_NT_WORKSTATION) {
			Cvar_Set("sys_osVersion", "Windows 7" );
			Com_Printf("Operating System:    Windows 7\n");
		} else {
			Cvar_Set("sys_osVersion", "Windows Server 2008 R2");
			Com_Printf("Operating System:    Windows Server 2008 R2\n");
		}
	}

	// Windows 8
	if ( osVersionInfo.dwMajorVersion == 6 && osVersionInfo.dwMinorVersion == 2 ) {
		if( osVersionInfo.wProductType == VER_NT_WORKSTATION ) {
			Cvar_Set("sys_osVersion", "Windows 8");
			Com_Printf("\nOperating System:    Windows 8\n");
		} else {
			Cvar_Set("sys_osVersion", "Windows 8 Server" );
			Com_Printf("Operating System:    Windows 8 Server\n");
		}
	}

	// Dushan - this is same type of information if you do "ver" in command prompt in Windows
	Com_Printf("                     Microsoft Windows [Version %i.%i.%i]\n", osVersionInfo.dwMajorVersion, osVersionInfo.dwMinorVersion, osVersionInfo.dwBuildNumber);
}

/*
===============
Sys_PrintMemoryInfo
===============
*/
void Sys_PrintMemoryInfo( void ) {
	SYSTEM_INFO si;
	MEMORYSTATUS ms;

	GetSystemInfo( &si );

	GlobalMemoryStatus( &ms );

	Com_Printf( "Memory:\n" );
	Com_Printf( "    Total Physical: %i MB\n", ms.dwTotalPhys / 1024 / 1024 );
	Com_Printf( "    Total Page File: %i MB\n", ms.dwTotalPageFile / 1024 / 1024 );
	Com_Printf( "    Load: %i%%\n", ms.dwMemoryLoad );
	Com_Printf( "    Page Size: %i K\n", si.dwPageSize / 1024 );
}