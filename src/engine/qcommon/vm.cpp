/*
===========================================================================

OpenWolf GPL Source Code
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company. 

This file is part of the OpenWolf GPL Source Code (OpenWolf Source Code).  

OpenWolf Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

OpenWolf Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with OpenWolf Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the OpenWolf Source Code is also subject to certain additional terms. 
You should have received a copy of these additional terms immediately following the 
terms and conditions of the GNU General Public License which accompanied the OpenWolf 
Source Code.  If not, please request a copy in writing from id Software at the address 
below.

If you have questions concerning this license or the applicable additional terms, you 
may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, 
Maryland 20850 USA.

===========================================================================
*/

// vm.c -- virtual machine

/*


intermix code and data
symbol table

a dll has one imported function: VM_SystemCall
and one exported function: Perform


*/

#include "vm_local.h"

vm_t	*currentVM = NULL;
vm_t	*lastVM    = NULL;
int		vm_debugLevel;

// used by Com_Error to get rid of running vm's before longjmp
static int forced_unload;

#define	MAX_VM		3
vm_t	vmTable[MAX_VM];


void VM_VmInfo_f( void );
void VM_VmProfile_f( void );

/*
==============
VM_Debug
==============
*/
void VM_Debug( int level ) {
	vm_debugLevel = level;
}

/*
==============
VM_Init
==============
*/
void VM_Init( void ) {
	Cvar_Get( "vm_cgame", "0", CVAR_ARCHIVE, "test" );
	Cvar_Get( "vm_game", "0", CVAR_ARCHIVE, "test" );
	Cvar_Get( "vm_ui", "0", CVAR_ARCHIVE, "test" );

	Cmd_AddCommand ("vminfo", VM_VmInfo_f, "test" );

	Com_Memset( vmTable, 0, sizeof( vmTable ) );
}


/*
===============
ParseHex
===============
*/
int	ParseHex( const char *text ) {
	int value, c;

	value = 0;
	while ( ( c = *text++ ) != 0 ) {
		if ( c >= '0' && c <= '9' ) {
			value = value * 16 + c - '0';
			continue;
		}
		if ( c >= 'a' && c <= 'f' ) {
			value = value * 16 + 10 + c - 'a';
			continue;
		}
		if ( c >= 'A' && c <= 'F' ) {
			value = value * 16 + 10 + c - 'A';
			continue;
		}
	}

	return value;
}

/*
============
VM_DllSyscall

Dlls will call this directly

 rcg010206 The horror; the horror.

  The syscall mechanism relies on stack manipulation to get its args.
   This is likely due to C's inability to pass "..." parameters to
   a function in one clean chunk. On PowerPC Linux, these parameters
   are not necessarily passed on the stack, so while (&arg[0] == arg)
   is true, (&arg[1] == 2nd function parameter) is not necessarily
   accurate, as arg's value might have been stored to the stack or
   other piece of scratch memory to give it a valid address, but the
   next parameter might still be sitting in a register.

  Quake's syscall system also assumes that the stack grows downward,
   and that any needed types can be squeezed, safely, into a signed int.

  This hack below copies all needed values for an argument to a
   array in memory, so that Quake can get the correct values. This can
   also be used on systems where the stack grows upwards, as the
   presumably standard and safe stdargs.h macros are used.

  As for having enough space in a signed int for your datatypes, well,
   it might be better to wait for DOOM 3 before you start porting.  :)

  The original code, while probably still inherently dangerous, seems
   to work well enough for the platforms it already works on. Rather
   than add the performance hit for those platforms, the original code
   is still in use there.

  For speed, we just grab 15 arguments, and don't worry about exactly
   how many the syscall actually needs; the extra is thrown away.
 
============
*/
intptr_t QDECL VM_DllSyscall( intptr_t arg, ... ) {
#if !id386 || defined __clang__
  // rcg010206 - see commentary above
  intptr_t args[16];
  int i;
  va_list ap;
  
  args[0] = arg;
  
  va_start(ap, arg);
  for (i = 1; i < ARRAY_LEN (args); i++)
    args[i] = va_arg(ap, intptr_t);
  va_end(ap);
  
  return currentVM->systemCall( args );
#else // original id code
	return currentVM->systemCall( &arg );
#endif
}

/*
=================
VM_Restart

Reload the data, but leave everything else in place
This allows a server to do a map_restart without changing memory allocation
=================
*/
vm_t *VM_Restart( vm_t *vm ) {
	// DLL's can't be restarted in place
	char            name[MAX_QPATH];
	vmInterpret_t	interpret;

	intptr_t(*systemCall) (intptr_t * parms);

	systemCall = vm->systemCall;
	Q_strncpyz(name, vm->name, sizeof(name));
	interpret = vm->interpret;

	VM_Free(vm);

	vm = VM_Create(name, systemCall, interpret);
	return vm;
}

/*
================
VM_Create
================
*/
vm_t *VM_Create( const char *module, intptr_t ( *systemCalls )(intptr_t *), vmInterpret_t interpret ) {
	vm_t        *vm;
	int			i, remaining;

	if ( !module || !module[0] || !systemCalls ) {
		Com_Error( ERR_FATAL, "VM_Create: bad parms" );
	}

	remaining = Hunk_MemoryRemaining();

	// see if we already have the VM
	for ( i = 0 ; i < MAX_VM ; i++ ) {
		if ( !Q_stricmp( vmTable[i].name, module ) ) {
			vm = &vmTable[i];
			return vm;
		}
	}

	// find a free vm
	for ( i = 0 ; i < MAX_VM ; i++ ) {
		if ( !vmTable[i].name[0] ) {
			break;
		}
	}

	if ( i == MAX_VM ) {
		Com_Error( ERR_FATAL, "VM_Create: no free vm_t" );
	}

	vm = &vmTable[i];

	Q_strncpyz( vm->name, module, sizeof( vm->name ) );
	vm->systemCall = systemCalls;

	if ( interpret == VMI_NATIVE ) {
		// try to load as a system dll
		Com_Printf("Loading dll file '%s'.\n", vm->name);
		vm->dllHandle = Sys_LoadDll( module, vm->fqpath, &vm->entryPoint, VM_DllSyscall );
		if ( vm->dllHandle ) {
			vm->systemCall = systemCalls;
			return vm;
		}
		Com_Printf("Failed loading DLL.\n");
	}

	Com_Printf("%s loaded in %d bytes on the hunk\n", module, remaining - Hunk_MemoryRemaining());

    return NULL;
}

/*
==============
VM_Free
==============
*/
void VM_Free( vm_t *vm ) {
	if ( vm->dllHandle ) {
		Sys_UnloadDll( vm->dllHandle );
		Com_Memset( vm, 0, sizeof( *vm ) );
	}

	Com_Memset( vm, 0, sizeof( *vm ) );

	currentVM = NULL;
	lastVM = NULL;
}

/*
==============
VM_Clear
==============
*/
void VM_Clear(void) {
	int i;

	for (i=0;i<MAX_VM; i++) {
		VM_Free(&vmTable[i]);
	}
}

/*
==============
VM_Forced_Unload_Start
==============
*/
void VM_Forced_Unload_Start(void) {
	forced_unload = 1;
}

/*
==============
VM_Forced_Unload_Done
==============
*/
void VM_Forced_Unload_Done(void) {
	forced_unload = 0;
}

/*
==============
VM_ArgPtr
==============
*/
void *VM_ArgPtr( intptr_t intValue ) {
	if ( !intValue ) {
		return NULL;
	}
	// currentVM is missing on reconnect
	if ( currentVM==NULL )
	  return NULL;

	return (void *)(currentVM->dataBase + intValue);
}

/*
==============
VM_ExplicitArgPtr
==============
*/
void *VM_ExplicitArgPtr( vm_t *vm, intptr_t intValue ) {
	if ( !intValue ) {
		return NULL;
	}

	// currentVM is missing on reconnect here as well?
	if ( currentVM==NULL ) {
	  return NULL;
	}

	return (void *)(vm->dataBase + intValue);
}


/*
==============
VM_Call

Upon a system call, the stack will look like:
==============
*/

intptr_t QDECL VM_Call( vm_t *vm, int callnum, ... ) {
	vm_t		*oldVM;
	intptr_t	r;
	int			i, args[15];
	va_list     ap;

#if defined (UPDATE_SERVER)
	return 0;
#endif

	if ( !vm ) {
		Com_Error( ERR_FATAL, "VM_Call with NULL vm" );
	}

	oldVM = currentVM;
	currentVM = vm;
	lastVM = vm;

	if ( vm_debugLevel ) {
	  Com_Printf( "VM_Call( %d )\n", callnum );
	}

	va_start(ap, callnum);
	for(i = 0; i < sizeof(args) / sizeof(args[i]); i++)
	{
		args[i] = va_arg(ap, int);
	}
	va_end(ap);

	r = vm->entryPoint(callnum, args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7], args[8], 
		args[9], args[10], args[11], args[12], args[13], args[14], args[15]);

	if ( oldVM != NULL )
	  currentVM = oldVM;
	return r;
}

//=================================================================

/*
==============
VM_VmInfo_f
==============
*/
void VM_VmInfo_f( void ) {
	vm_t	*vm;
	int		i;

	Com_Printf( "Registered virtual machines:\n" );
	for ( i = 0 ; i < MAX_VM ; i++ ) {
		vm = &vmTable[i];
		if ( !vm->name[0] ) {
			break;
		}
		Com_Printf( "%s : ", vm->name );
		if ( vm->dllHandle ) {
			Com_Printf( "native\n" );
			continue;
		}
	}
}
