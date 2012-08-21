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

#include "../qcommon/q_shared.h"
#include "qcommon.h"

struct vm_s {
																			// DO NOT MOVE OR CHANGE THESE WITHOUT CHANGING THE VM_OFFSET_* DEFINES
																			// USED BY THE ASM CODE
    int					programStack;										// the vm may be recursively entered
    intptr_t			(*systemCall)( intptr_t *parms );
																			//------------------------------------
	char				name[MAX_QPATH];
	void				*searchPath;										// hint for FS_ReadFileDir()
																			// for dynamic linked modules
	void				*dllHandle;
	intptr_t			(QDECL *entryPoint)( int callNum, ... );
	byte				*dataBase;
	char				fqpath[MAX_QPATH + 1];
	vmInterpret_t		interpret;
};

extern	vm_t	*currentVM;
extern	int		vm_debugLevel;
