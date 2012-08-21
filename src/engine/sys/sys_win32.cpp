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
#include "../qcommon/qcommon.h"
#include "../client/client.h"
#include "sys_local.h"
#include "sys_win32.h"
#include "resource.h"

#include <windows.h>
#include <lmerr.h>
#include <lmcons.h>
#include <lmwksta.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <direct.h>
#include <io.h>
#include <conio.h>
#include <wincrypt.h>
#include <shlobj.h>
#include <psapi.h>
#include <float.h>
#include <setjmp.h>


// Used to determine where to store user-specific files
static char homePath[ MAX_OSPATH ] = { 0 };
static jmp_buf sys_exitframe;
static int sys_retcode;
static char sys_exitstr[MAX_STRING_CHARS];

/*
==================
CON_CtrlHandler

The Windows Console doesn't use signals for terminating the application
with Ctrl-C, logging off, window closing, etc.  Instead it uses a special
handler routine.  Fortunately, the values for Ctrl signals don't seem to
overlap with true signal codes that Windows provides, so calling
Sys_SigHandler() with those numbers should be safe for generating unique
shutdown messages.
==================
*/
static BOOL WINAPI CON_CtrlHandler( DWORD sig ) {
	Sys_SigHandler( sig );
	return TRUE;
}

/*
================
Sys_SetFPUCW
Set FPU control word to default value
================
*/

#ifndef _RC_CHOP
// mingw doesn't seem to have these defined :(

  #define _MCW_EM	0x0008001fU
  #define _MCW_RC	0x00000300U
  #define _MCW_PC	0x00030000U
  #define _RC_CHOP	0x00000300U
  #define _PC_53	0x00010000U
  
  unsigned int _controlfp(unsigned int new, unsigned int mask);
#endif

#define FPUCWMASK1 (_MCW_RC | _MCW_EM)
#define FPUCW (_RC_CHOP | _MCW_EM | _PC_53)

#if idx64
#define FPUCWMASK	(FPUCWMASK1)
#else
#define FPUCWMASK	(FPUCWMASK1 | _MCW_PC)
#endif

/*
==============
Sys_SetFloatEnv
==============
*/
void Sys_SetFloatEnv(void) {
	_controlfp(FPUCW, FPUCWMASK);
}


/*
================
Sys_DefaultHomePath
================
*/
// Dushan - SHFolder.dll is common in Windows nowadays, so we dont need much of this stuff anyway
char *Sys_DefaultHomePath( char * buffer, int size ) {
	if( SHGetSpecialFolderPath( NULL, buffer, CSIDL_PERSONAL, TRUE ) != NOERROR ) {
		Q_strcat( buffer, size, "\\My Games\\OpenWolf" );
	} else {
		Com_Error( ERR_FATAL, "couldn't find home path.\n" );
		buffer[0] = 0;
	}

	return buffer;
}

/*
================
Sys_TempPath
================
*/
const char *Sys_TempPath( void ) {
	static	TCHAR path[ MAX_PATH ];
	DWORD	length;
	char	tmp[ MAX_OSPATH ];

	length = GetTempPath( sizeof( path ), path );

	if( length > sizeof( path ) || length == 0 ) {
		return Sys_DefaultHomePath(tmp, sizeof(tmp));
	} else {
		return path;
	}
}

/*
================
Sys_Milliseconds
================
*/
int sys_timeBase;
int Sys_Milliseconds (void) {
	int             sys_curtime;
	static qboolean initialized = qfalse;

	if (!initialized) {
		sys_timeBase = timeGetTime();
		initialized = qtrue;
	}
	sys_curtime = timeGetTime() - sys_timeBase;

	return sys_curtime;
}

/*
================
Sys_RandomBytes
================
*/
qboolean Sys_RandomBytes( byte *string, int len ) {
	HCRYPTPROV  prov;

	if( !CryptAcquireContext( &prov, NULL, NULL,
		PROV_RSA_FULL, CRYPT_VERIFYCONTEXT ) )  {

		return qfalse;
	}

	if( !CryptGenRandom( prov, len, (BYTE *)string ) )  {
		CryptReleaseContext( prov, 0 );
		return qfalse;
	}
	CryptReleaseContext( prov, 0 );
	return qtrue;
}

/*
================
Sys_GetCurrentUser
================
*/
char *Sys_GetCurrentUser( void ) {
	static char s_userName[1024];
	unsigned long size = sizeof( s_userName );

	if( !GetUserName( s_userName, &size ) ) {
		strcpy( s_userName, "player" );
	}

	if( !s_userName[0] ) {
		strcpy( s_userName, "player" );
	}

	return s_userName;
}

/*
================
Sys_GetClipboardData
================
*/
char *Sys_GetClipboardData( void ) {
	char *data = NULL, *cliptext;

	if ( OpenClipboard( NULL ) != 0 ) {
		HANDLE hClipboardData;

		if ( ( hClipboardData = GetClipboardData( CF_TEXT ) ) != 0 ) {
			if ( ( cliptext = (char*)GlobalLock( hClipboardData ) ) != 0 ) {
				data = (char*)Z_Malloc( GlobalSize( hClipboardData ) + 1 );
				Q_strncpyz( data, cliptext, GlobalSize( hClipboardData ) );
				GlobalUnlock( hClipboardData );
				
				strtok( data, "\n\r\b" );
			}
		}
		CloseClipboard();
	}
	return data;
}

#define MEM_THRESHOLD 96*1024*1024

/*
==================
Sys_LowPhysicalMemory
==================
*/
qboolean Sys_LowPhysicalMemory( void ) {
#if defined (__WIN64__)
	MEMORYSTATUSEX stat;
	GlobalMemoryStatusEx (&stat);

	return (stat.ullTotalPhys <= MEM_THRESHOLD) ? qtrue : qfalse;
#else
	MEMORYSTATUS stat;
	GlobalMemoryStatus (&stat);

	return (stat.dwTotalPhys <= MEM_THRESHOLD) ? qtrue : qfalse;
#endif
}

/*
==============
Sys_Basename
==============
*/
const char *Sys_Basename( char *path ) {
	static char base[ MAX_OSPATH ] = { 0 };
	int			length;

	length = strlen( path ) - 1;

	// Skip trailing slashes
	while( length > 0 && path[ length ] == '\\' ) {
		length--;
	}

	while( length > 0 && path[ length - 1 ] != '\\' ) {
		length--;
	}

	Q_strncpyz( base, &path[ length ], sizeof( base ) );

	length = strlen( base ) - 1;

	// Strip trailing slashes
	while( length > 0 && base[ length ] == '\\' )
    base[ length-- ] = '\0';

	return base;
}

/*
==============
Sys_Dirname
==============
*/
const char *Sys_Dirname( char *path ) {
	static char dir[ MAX_OSPATH ] = { 0 };
	int			length;

	Q_strncpyz( dir, path, sizeof( dir ) );
	length = strlen( dir ) - 1;

	while( length > 0 && dir[ length ] != '\\' ) {
		length--;
	}

	dir[ length ] = '\0';

	return dir;
}

/*
==============
Sys_Mkdir
==============
*/
qboolean Sys_Mkdir( const char *path ) {
	if( !CreateDirectory( path, NULL ) ) {
		if( GetLastError( ) != ERROR_ALREADY_EXISTS ) {
			return qfalse;
		}
	}

	return qtrue;
}

/*
==============
Sys_Cwd
==============
*/
char *Sys_Cwd( void ) {
	static char cwd[MAX_OSPATH];

	_getcwd( cwd, sizeof( cwd ) - 1 );
	cwd[MAX_OSPATH-1] = 0;

	return cwd;
}

/*
==============================================================
DIRECTORY SCANNING
==============================================================
*/

#define MAX_FOUND_FILES 0x1000

/*
==============
Sys_ListFilteredFiles
==============
*/
void Sys_ListFilteredFiles( const char *basedir, char *subdirs, char *filter, char **list, int *numfiles ) {
	char		search[MAX_OSPATH], newsubdirs[MAX_OSPATH];
	char		filename[MAX_OSPATH];
	intptr_t	findhandle;
	struct		_finddata_t findinfo;

	if ( *numfiles >= MAX_FOUND_FILES - 1 ) {
		return;
	}

	if (strlen(subdirs)) {
		Com_sprintf( search, sizeof(search), "%s\\%s\\*", basedir, subdirs );
	} else {
		Com_sprintf( search, sizeof(search), "%s\\*", basedir );
	}

	findhandle = _findfirst (search, &findinfo);
	if (findhandle == -1) {
		return;
	}

	do {
		if (findinfo.attrib & _A_SUBDIR) {
			if (Q_stricmp(findinfo.name, ".") && Q_stricmp(findinfo.name, "..")) {
				if (strlen(subdirs)) {
					Com_sprintf( newsubdirs, sizeof(newsubdirs), "%s\\%s", subdirs, findinfo.name);
				} else {
					Com_sprintf( newsubdirs, sizeof(newsubdirs), "%s", findinfo.name);
				}
				Sys_ListFilteredFiles( basedir, newsubdirs, filter, list, numfiles );
			}
		}
		if ( *numfiles >= MAX_FOUND_FILES - 1 ) {
			break;
		}
		Com_sprintf( filename, sizeof(filename), "%s\\%s", subdirs, findinfo.name );
		if (!Com_FilterPath( filter, filename, qfalse )) {
			continue;
		}
		list[ *numfiles ] = CopyString( filename );
		(*numfiles)++;
	} while ( _findnext (findhandle, &findinfo) != -1 );

	_findclose (findhandle);
}

/*
==============
strgtr
==============
*/
static qboolean strgtr(const char *s0, const char *s1) {
	int l0, l1, i;

	l0 = strlen(s0);
	l1 = strlen(s1);

	if (l1<l0) {
		l0 = l1;
	}

	for(i=0;i<l0;i++) {
		if (s1[i] > s0[i]) {
			return qtrue;
		}
		if (s1[i] < s0[i]) {
			return qfalse;
		}
	}
	return qfalse;
}

/*
==============
Sys_ListFiles
==============
*/
char **Sys_ListFiles( const char *directory, const char *extension, char *filter, int *numfiles, qboolean wantsubs ) {
	char				search[MAX_OSPATH];
	int					nfiles, flag, i;
	char				**listCopy, *list[MAX_FOUND_FILES];
	struct _finddata_t	findinfo;
	intptr_t			findhandle;

	if (filter) {
		nfiles = 0;
		Sys_ListFilteredFiles( directory, "", filter, list, &nfiles );

		list[ nfiles ] = 0;
		*numfiles = nfiles;

		if (!nfiles) {
			return NULL;
		}

		listCopy = (char**)Z_Malloc( ( nfiles + 1 ) * sizeof( *listCopy ) );
		for ( i = 0 ; i < nfiles ; i++ ) {
			listCopy[i] = list[i];
		}
		listCopy[i] = NULL;

		return listCopy;
	}

	if ( !extension) {
		extension = "";
	}

	// passing a slash as extension will find directories
	if ( extension[0] == '/' && extension[1] == 0 ) {
		extension = "";
		flag = 0;
	} else {
		flag = _A_SUBDIR;
	}

	Com_sprintf( search, sizeof(search), "%s\\*%s", directory, extension );

	// search
	nfiles = 0;

	findhandle = _findfirst (search, &findinfo);
	if (findhandle == -1) {
		*numfiles = 0;
		return NULL;
	}

	do {
		if ( (!wantsubs && flag ^ ( findinfo.attrib & _A_SUBDIR )) || (wantsubs && findinfo.attrib & _A_SUBDIR) ) {
			if ( nfiles == MAX_FOUND_FILES - 1 ) {
				break;
			}
			list[ nfiles ] = CopyString( findinfo.name );
			nfiles++;
		}
	} while ( _findnext (findhandle, &findinfo) != -1 );

	list[ nfiles ] = 0;

	_findclose (findhandle);

	// return a copy of the list
	*numfiles = nfiles;

	if ( !nfiles ) {
		return NULL;
	}

	listCopy = (char**)Z_Malloc( ( nfiles + 1 ) * sizeof( *listCopy ) );
	for ( i = 0 ; i < nfiles ; i++ ) {
		listCopy[i] = list[i];
	}
	listCopy[i] = NULL;

	do {
		flag = 0;
		for(i=1; i<nfiles; i++) {
			if (strgtr(listCopy[i-1], listCopy[i])) {
				char *temp = listCopy[i];
				listCopy[i] = listCopy[i-1];
				listCopy[i-1] = temp;
				flag = 1;
			}
		}
	} while(flag);

	return listCopy;
}

/*
==============
Sys_FreeFileList
==============
*/
void Sys_FreeFileList( char **list ) {
	int i;

	if ( !list ) {
		return;
	}

	for ( i = 0 ; list[i] ; i++ ) {
		Z_Free( list[i] );
	}

	Z_Free( list );
}


/*
==============
Sys_Sleep

Block execution for msec or until input is received.
==============
*/
void Sys_Sleep( int msec ) {
	if( msec == 0 )
		return;

#ifdef DEDICATED
	if( msec < 0 ) {
		WaitForSingleObject( GetStdHandle( STD_INPUT_HANDLE ), INFINITE );
	} else {
		WaitForSingleObject( GetStdHandle( STD_INPUT_HANDLE ), msec );
	}
#else
	// Client Sys_Sleep doesn't support waiting on stdin
	if( msec < 0 ) {
		return;
	}

	Sleep( msec );
#endif
}

/*
==============
Sys_OpenUrl
==============
*/
qboolean Sys_OpenUrl( const char *url ) {
	return ((int)ShellExecute( NULL, NULL, url, NULL, NULL, SW_SHOWNORMAL ) > 32) ? qtrue : qfalse; 
}

/*
==============
Sys_ErrorDialog

Display an error message
==============
*/
void Sys_ErrorDialog( const char *error ) {
	const char *homepath = Cvar_VariableString( "fs_homepath" ), *gamedir = Cvar_VariableString( "fs_gamedir" ), *fileName = "crashlog.txt";
	char buffer[ 1024 ], *ospath = FS_BuildOSPath( homepath, gamedir, fileName );
	unsigned int size;
	int f = -1;

	Sys_Print( va( "%s\n", error ) );

#ifndef DEDICATED
	Sys_Dialog( DT_ERROR, va( "%s. See \"%s\" for details.", error, ospath ), "Error" );
#endif

	// Make sure the write path for the crashlog exists...
	if( FS_CreatePath( ospath ) ) {
		Com_Printf( "ERROR: couldn't create path '%s' for crash log.\n", ospath );
		return;
	}

	// We might be crashing because we maxed out the Quake MAX_FILE_HANDLES,
	// which will come through here, so we don't want to recurse forever by
	// calling FS_FOpenFileWrite()...use the Unix system APIs instead.
	f = open( ospath, O_CREAT | O_TRUNC | O_WRONLY, 0640 );
	if( f == -1 ) {
		Com_Printf( "ERROR: couldn't open %s\n", fileName );
		return;
	}

	// We're crashing, so we don't care much if write() or close() fails.
	while( ( size = CON_LogRead( buffer, sizeof( buffer ) ) ) > 0 ) {
		if( write( f, buffer, size ) != size ) {
			Com_Printf( "ERROR: couldn't fully write to %s\n", fileName );
			break;
		}
	}

	close( f );
}

/*
==============
Sys_Dialog

Display a win32 dialog box
==============
*/
dialogResult_t Sys_Dialog( dialogType_t type, const char *message, const char *title ) {
	UINT uType;

	switch( type ) {
		default:
		case DT_INFO:      uType = MB_ICONINFORMATION|MB_OK; break;
		case DT_WARNING:   uType = MB_ICONWARNING|MB_OK; break;
		case DT_ERROR:     uType = MB_ICONERROR|MB_OK; break;
		case DT_YES_NO:    uType = MB_ICONQUESTION|MB_YESNO; break;
		case DT_OK_CANCEL: uType = MB_ICONWARNING|MB_OKCANCEL; break;
	}

	switch( MessageBox( NULL, message, title, uType ) ) {
		default:
		case IDOK:      return DR_OK;
		case IDCANCEL:  return DR_CANCEL;
		case IDYES:     return DR_YES;
		case IDNO:      return DR_NO;
	}
}

#ifndef DEDICATED
static qboolean SDL_VIDEODRIVER_externallySet = qfalse;
#endif

/*
==============
Sys_GLimpSafeInit

Windows specific "safe" GL implementation initialisation
==============
*/
void Sys_GLimpSafeInit( void ) {
#ifndef DEDICATED
	if( !SDL_VIDEODRIVER_externallySet ) {
		// Here, we want to let SDL decide what do to unless
		// explicitly requested otherwise
		_putenv( "SDL_VIDEODRIVER=" );
	}
#endif
}

/*
==============
Sys_GLimpInit

Windows specific GL implementation initialisation
==============
*/
void Sys_GLimpInit( void ) {
#ifndef DEDICATED
	if( !SDL_VIDEODRIVER_externallySet ) {
		// It's a little bit weird having in_mouse control the
		// video driver, but from ioq3's point of view they're
		// virtually the same except for the mouse input anyway
		if( Cvar_VariableIntegerValue( "in_mouse" ) == -1 ) {
			// Use the windib SDL backend, which is closest to
			// the behaviour of idq3 with in_mouse set to -1
			_putenv( "SDL_VIDEODRIVER=windib" );
		} else {
			// Use the DirectX SDL backend
			_putenv( "SDL_VIDEODRIVER=directx" );
		}
	}
#endif
}

/*
==============
Sys_PlatformInit

Windows specific initialisation
==============
*/
static void resetTime(void) { 
	timeEndPeriod(1); 
}

void Sys_PlatformInit( void ) {
#ifndef DEDICATED
	const char *SDL_VIDEODRIVER = getenv( "SDL_VIDEODRIVER" );
#endif

	Sys_SetFloatEnv();

#ifndef DEDICATED
	if( SDL_VIDEODRIVER ) {
		Com_Printf( "SDL_VIDEODRIVER is externally set to \"%s\", "
				"in_mouse -1 will have no effect\n", SDL_VIDEODRIVER );
		SDL_VIDEODRIVER_externallySet = qtrue;
	} else {
		SDL_VIDEODRIVER_externallySet = qfalse;
	}
#endif

	// Handle Ctrl-C or other console termination
	SetConsoleCtrlHandler( CON_CtrlHandler, TRUE );

	// Increase sleep resolution
	timeBeginPeriod(1);
	atexit(resetTime);
}

/*
==============
Sys_SetEnv

set/unset environment variables (empty value removes it)
==============
*/
void Sys_SetEnv(const char *name, const char *value) {
	_putenv(va("%s=%s", name, value));
}

/*
==============
Sys_PID
==============
*/
int Sys_PID( void ) {
	return GetCurrentProcessId( );
}

/*
==============
Sys_PIDIsRunning
==============
*/
qboolean Sys_PIDIsRunning( int pid ) {
	DWORD	processes[ 1024 ], numBytes, numProcesses;
	int		i;

	if( !EnumProcesses( processes, sizeof( processes ), &numBytes ) ) {
		return qfalse; // Assume it's not running
	}

	numProcesses = numBytes / sizeof( DWORD );

	// Search for the pid
	for( i = 0; i < numProcesses; i++ ) {
		if( processes[ i ] == pid ) {
			return qtrue;
		}
	}

	return qfalse;
}

/*
==================
Sys_StartProcess

NERVE - SMF
==================
*/
void Sys_StartProcess( char *exeName, qboolean doexit ) {
	TCHAR				szPathOrig[_MAX_PATH];
	STARTUPINFO			si;
	PROCESS_INFORMATION pi;

	ZeroMemory( &si, sizeof( si ) );
	si.cb = sizeof( si );

	GetCurrentDirectory( _MAX_PATH, szPathOrig );

	// JPW NERVE swiped from Sherman's SP code
	if ( !CreateProcess( NULL, va( "%s\\%s", szPathOrig, exeName ), NULL, NULL,FALSE, 0, NULL, NULL, &si, &pi ) ) {
		// couldn't start it, popup error box
		Com_Error( ERR_DROP, "Could not start process: '%s\\%s' ", szPathOrig, exeName  );
		return;
	}
	// jpw

	// TTimo: similar way of exiting as used in Sys_OpenURL below
	if ( doexit ) {
		Cbuf_ExecuteText( EXEC_APPEND, "quit\n" );
	}
}

/*
==================
Sys_OpenURL

NERVE - SMF
==================
*/
void Sys_OpenURL( const char *url, qboolean doexit ) {
	HWND wnd;

	static qboolean doexit_spamguard = qfalse;

	if ( doexit_spamguard ) {
		Com_DPrintf( "Sys_OpenURL: already in a doexit sequence, ignoring %s\n", url );
		return;
	}

	Com_Printf( "Open URL: %s\n", url );

	if ( !ShellExecute( NULL, "open", url, NULL, NULL, SW_RESTORE ) ) {
		// couldn't start it, popup error box
		Com_Error( ERR_DROP, "Could not open url: '%s' ", url );
		return;
	}

	wnd = GetForegroundWindow();

	if ( wnd ) {
		ShowWindow( wnd, SW_MAXIMIZE );
	}

	if ( doexit ) {
		// show_bug.cgi?id=612
		doexit_spamguard = qtrue;
		Cbuf_ExecuteText( EXEC_APPEND, "quit\n" );
	}
}

/*
========================================================================

EVENT LOOP

========================================================================
*/

#define MAX_QUED_EVENTS     256
#define MASK_QUED_EVENTS    ( MAX_QUED_EVENTS - 1 )

sysEvent_t eventQue[MAX_QUED_EVENTS];
int eventHead, eventTail;
byte sys_packetReceived[MAX_MSGLEN];

/*
================
Sys_QueEvent

A time of 0 will get the current time
Ptr should either be null, or point to a block of data that can
be freed by the game later.
================
*/
void Sys_QueEvent( int time, sysEventType_t type, int value, int value2, int ptrLength, void *ptr ) {
	sysEvent_t  *ev;

	ev = &eventQue[ eventHead & MASK_QUED_EVENTS ];
	if ( eventHead - eventTail >= MAX_QUED_EVENTS ) {
		Com_Printf( "Sys_QueEvent: overflow\n" );
		// we are discarding an event, but don't leak memory
		if ( ev->evPtr ) {
			Z_Free( ev->evPtr );
		}
		eventTail++;
	}

	eventHead++;

	if ( time == 0 ) {
		time = Sys_Milliseconds();
	}

	ev->evTime = time;
	ev->evType = type;
	ev->evValue = value;
	ev->evValue2 = value2;
	ev->evPtrLength = ptrLength;
	ev->evPtr = ptr;
}


/*
==================
WinMain
==================
*/
#if defined (_WIN32) && !defined (_DEBUG)
WinVars_t g_wv;
static char sys_cmdline[MAX_STRING_CHARS];
int totalMsec, countMsec;

int Win_Main( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow ) {
	char	cwd[MAX_OSPATH];
	int		startTime, endTime;

	// should never get a previous instance in Win32
	if ( hPrevInstance ) {
		return 0;
	}

	if( setjmp( sys_exitframe ) ) {
		//make sure that any subsystems that may have spawned threads go down
		__try {
#if !defined(DEDICATED)
			S_Shutdown();
			CL_ShutdownRef();
#endif
		} __finally  { //wheeeee...
			Com_ReleaseMemory();
		}

		return sys_retcode;
	}

#ifdef EXCEPTION_HANDLER
	WinSetExceptionVersion( Q3_VERSION );
#endif

	g_wv.hInstance = hInstance;
	Q_strncpyz( sys_cmdline, lpCmdLine, sizeof( sys_cmdline ) );

	// done before Com/Sys_Init since we need this for error output
	Sys_CreateConsole();

	// no abort/retry/fail errors
	SetErrorMode( SEM_FAILCRITICALERRORS );

	// get the initial time base
	Sys_Milliseconds();

	Com_Init( sys_cmdline );
	NET_Init();

#ifndef DEDICATED
	IN_Init(); // fretn - directinput must be inited after video etc
#endif

	_getcwd( cwd, sizeof( cwd ) );
	Com_Printf( "Working directory: %s\n", cwd );

	// hide the early console since we've reached the point where we
	// have a working graphics subsystems
#if defined (_WIN32) && !defined (_DEBUG)
	if ( !com_dedicated->integer && !com_viewlog->integer ) {
		Sys_ShowConsole( 0, qfalse );
	}
#endif

	SetFocus( g_wv.hWnd );

    // main game loop
	while( 1 ) {
		// if not running as a game client, sleep a bit
		if( g_wv.isMinimized || ( com_dedicated && com_dedicated->integer ) ) {
			Sleep( 5 );
		}

		// set low precision every frame, because some system calls
		// reset it arbitrarily
		//_controlfp( _PC_24, _MCW_PC );
		//_controlfp( -1, _MCW_EM  );	// no exceptions, even if some crappy
										// syscall turns them back on!

		startTime = Sys_Milliseconds();

		// make sure mouse and joystick are only called once a frame
		IN_Frame();

		// run the game
		//Com_FrameExt();
		Com_Frame();

		endTime = Sys_Milliseconds();
		totalMsec += endTime - startTime;
		countMsec++;
	}

	// never gets here
}
#endif

/*
==============
Sys_IsNumLockDown
==============
*/
qboolean Sys_IsNumLockDown(void) {
	SHORT state = GetKeyState(VK_NUMLOCK);

	if(state & 0x01) {
		return qtrue;
	}

	return qfalse;
}

/*
======================
	ReportCrash
======================
*/

#if defined (_WIN32) && !defined (_DEBUG)
#include <DbgHelp.h>

typedef struct {
	DWORD excThreadId;

	DWORD excCode;
	PEXCEPTION_POINTERS pExcPtrs;

	int numIgnoreThreads;
	DWORD ignoreThreadIds[16];

	int numIgnoreModules;
	HMODULE ignoreModules[16];
} dumpParams_t;

typedef struct dumpCbParams_t {
	dumpParams_t	*p;
	HANDLE			hModulesFile;
} dumpCbParams_t;

static BOOL CALLBACK MiniDumpCallback( PVOID CallbackParam, const PMINIDUMP_CALLBACK_INPUT CallbackInput, PMINIDUMP_CALLBACK_OUTPUT CallbackOutput ) {
	const dumpCbParams_t *params = (dumpCbParams_t*)CallbackParam;
	
	switch( CallbackInput->CallbackType ) {
	case IncludeThreadCallback: {
			int i;

			for( i = 0; i < params->p->numIgnoreThreads; i++ ) {
				if( CallbackInput->IncludeThread.ThreadId == params->p->ignoreThreadIds[i] ) {
					return FALSE;
				}
			}
		}
		return TRUE;

	case IncludeModuleCallback: {
			int i;

			for( i = 0; i < params->p->numIgnoreModules; i++ ) {
				if( (HMODULE)CallbackInput->IncludeModule.BaseOfImage == params->p->ignoreModules[i] ) {
					return FALSE;
				}
			}
		}
		return TRUE;

	case ModuleCallback:
		if( params->hModulesFile ) {
#if !defined (_WIN32)
			 PWCHAR packIncludeMods[] = { L"/OpenWolf.exe", L"rendererGL3x86.dll", L"rendererGLx86.dll", L"rendererGL3x86.dll", L"rendererD3Dx86.dll", L"snd_openal.dll", L"qagame_mp_x86.dll", L"cgame_mp_x86.dll", L"ui_mp_x86.dll" };
#else
			PWCHAR packIncludeMods[] = { L"/OpenWolf.exe", L"rendererGL3x86_64.dll", L"rendererGLx86_64.dll", L"rendererD3Dx86_64.dll", L"snd_openal.dll", L"qagame_mp_x86_64.dll", L"cgame_mp_x86_64.dll", L"ui_mp_x86_64.dll" };
#endif

			wchar_t *path = CallbackInput->Module.FullPath;
			size_t len = wcslen( path );
			size_t i, c;

			//path is fully qualified - this won't trash
			for( i = len; i--; ) {
				if( path[i] == L'\\' ) {
					break;
				}
			}

			c = i + 1;

			for( i = 0; i < lengthof( packIncludeMods ); i++ )
				if( wcsicmp( path + c, packIncludeMods[i] ) == 0 ) {
					DWORD dummy;

					WriteFile( params->hModulesFile, path, sizeof( wchar_t ) * len, &dummy, NULL );
					WriteFile( params->hModulesFile, L"\n", sizeof( wchar_t ), &dummy, NULL );
					break;
				}
		}
		return TRUE;

	default:
		return TRUE;
	}
}

typedef BOOL (WINAPI *MiniDumpWriteDumpFunc_t)(
	IN HANDLE hProcess,
    IN DWORD ProcessId,
    IN HANDLE hFile,
    IN MINIDUMP_TYPE DumpType,
    IN CONST PMINIDUMP_EXCEPTION_INFORMATION ExceptionParam, OPTIONAL
    IN CONST PMINIDUMP_USER_STREAM_INFORMATION UserStreamParam, OPTIONAL
    IN CONST PMINIDUMP_CALLBACK_INFORMATION CallbackParam OPTIONAL
);

/*
==============
CreatePath
==============
*/
static void CreatePath( char *OSPath ) {
	char *ofs;

	for( ofs = OSPath + 1; *ofs; ofs++ ) {
		if( *ofs == '\\' || *ofs == '/' ) {
			// create the directory
			*ofs = 0;
			
			CreateDirectory( OSPath, NULL );

			*ofs = '\\';
		}
	}

	if( ofs[-1] != '\\' ) {
		CreateDirectory( OSPath, NULL );
	}
}

/*
==============
DoGenerateCrashDump
==============
*/
static DWORD WINAPI DoGenerateCrashDump( LPVOID pParams ) {
	dumpParams_t *params = (dumpParams_t*)pParams;
	
	static int nDump = 0;

	HANDLE hFile, hIncludeFile;
	DWORD dummy;
	char basePath[MAX_PATH], path[MAX_PATH];

	MiniDumpWriteDumpFunc_t MiniDumpWriteDump;

	{
		HMODULE hDbgHelp = LoadLibrary( "DbgHelp.dll" );

		if( !hDbgHelp ) {
			return 1;
		}

		MiniDumpWriteDump = (MiniDumpWriteDumpFunc_t)GetProcAddress( hDbgHelp, "MiniDumpWriteDump" );

		if( !MiniDumpWriteDump ) {
			return 2;
		}

		params->ignoreModules[params->numIgnoreModules++] = hDbgHelp;
	}

	{
		char homePath[ MAX_OSPATH ];
		Sys_DefaultHomePath( homePath, sizeof(homePath) );
		if( !homePath[0] ) {
			return 3;
		}

		Com_sprintf( basePath, sizeof( basePath ), "%s/bugs", homePath );
	}

#define BASE_DUMP_FILE_NAME "dump%04i"

	CreatePath( basePath );

	for( ; ; ) {
		DWORD err;

		Com_sprintf( path, sizeof( path ), "%s/"BASE_DUMP_FILE_NAME".dmp", basePath, nDump );
		hFile = CreateFile( path, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, 0 );

		if( hFile != INVALID_HANDLE_VALUE ) {
			break;
		}

		err = GetLastError();

		switch( err ) {
		case ERROR_FILE_EXISTS:
		case ERROR_ALREADY_EXISTS:
			break;

		case 0:
			return (DWORD)-1;

		default:
			return 0x80000000 | err;
		}

		nDump++;
	}

	Com_sprintf( path, sizeof( path ), "%s/"BASE_DUMP_FILE_NAME".include", basePath, nDump );
	hIncludeFile = CreateFile( path, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, 0 );

	{
		dumpCbParams_t cbParams;

		MINIDUMP_EXCEPTION_INFORMATION excInfo;
		MINIDUMP_CALLBACK_INFORMATION cbInfo;
		MINIDUMP_TYPE flags;

		excInfo.ThreadId = params->excThreadId;
		excInfo.ExceptionPointers = params->pExcPtrs;
		excInfo.ClientPointers = TRUE;

		cbParams.p = params;
		cbParams.hModulesFile = hIncludeFile;

		cbInfo.CallbackParam = &cbParams;
		cbInfo.CallbackRoutine = MiniDumpCallback;

		MiniDumpWriteDump( GetCurrentProcess(), GetCurrentProcessId(), hFile,
			flags, &excInfo, NULL, &cbInfo );
	}

	CloseHandle( hFile );

	{
		/*
			Write any additional include files here. Remember,
			the include file is unicode (wchar_t), has one
			file per line, and lines are seperated by a single LF (L'\n').
		*/
	}
	CloseHandle( hIncludeFile );

	Com_sprintf( path, sizeof( path ), "%s/"BASE_DUMP_FILE_NAME".build", basePath, nDump );
	hFile = CreateFile( path, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, 0 );

	{
		HMODULE hMod;

		HRSRC rcVer, rcBuild, rcMachine;
		HGLOBAL hgVer, hgBuild, hgMachine;

		DWORD lVer, lBuild, lMachine;
		char *pVer, *pBuild, *pMachine;

		hMod = GetModuleHandle( NULL );

		rcVer = FindResource( hMod, MAKEINTRESOURCE( IDR_INFO_SVNSTAT ), "INFO" );
		rcBuild = FindResource( hMod, MAKEINTRESOURCE( IDR_INFO_BUILDCONFIG ), "INFO" );
		rcMachine = FindResource( hMod, MAKEINTRESOURCE( IDR_INFO_BUILDMACHINE ), "INFO" );

		hgVer = LoadResource( hMod, rcVer );
		hgBuild = LoadResource( hMod, rcBuild );
		hgMachine = LoadResource( hMod, rcMachine );

		lVer = SizeofResource( hMod, rcVer );
		pVer = (char*)LockResource( hgVer );

		lBuild = SizeofResource( hMod, rcBuild );
		pBuild = (char*)LockResource( hgBuild );

		lMachine = SizeofResource( hMod, rcMachine );
		pMachine = (char*)LockResource( hgMachine );

		WriteFile( hFile, pBuild, lBuild, &dummy, NULL );
		WriteFile( hFile, pMachine, lMachine, &dummy, NULL );
		WriteFile( hFile, pVer, lVer, &dummy, NULL );
	}

	CloseHandle( hFile );

	Com_sprintf( path, sizeof( path ), "%s/"BASE_DUMP_FILE_NAME".info", basePath, nDump );
	hFile = CreateFile( path, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, 0 );

	{
		/*
			The .info contains one line of bug title and the rest is all bug description.
		*/

		const char *msg;
		switch( params->excCode ) {
			
		case 1: //SEH_CAPTURE_EXC:
			msg = (const char*)params->pExcPtrs->ExceptionRecord->ExceptionInformation[0];
			break;
			//*/

		default:
			msg = "Crash Dump\nOpenWolf crashed, see the attached dump for more information.";
			break;
		}

		WriteFile( hFile, msg, strlen( msg ), &dummy, NULL );
	}

	CloseHandle( hFile );

	Com_sprintf( path, sizeof( path ), "%s/"BASE_DUMP_FILE_NAME".con", basePath, nDump );
	hFile = CreateFile( path, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, 0 );

	{
		const char *conDump = Con_GetText( 0 );	
		WriteFile( hFile, conDump, strlen( conDump ), &dummy, NULL );
	}

	CloseHandle( hFile );

#undef BASE_DUMP_FILE_NAME

	return 0;
}

/*
==============
DialogProc
==============
*/
static INT_PTR CALLBACK DialogProc( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam ) {
	switch( uMsg ) {
	case WM_COMMAND:
		switch( LOWORD( wParam ) ) {
		case IDOK:
			PostQuitMessage( 0 );
			break;
		}
		return TRUE;

	default:
		return FALSE;
	}
}

/*
==============
DoReportCrashGUI
==============
*/
static DWORD WINAPI DoReportCrashGUI( LPVOID pParams ) {
	dumpParams_t *params = (dumpParams_t*)pParams;

	HWND dlg;
	HANDLE h;
	DWORD tid;
	MSG msg;
	BOOL mRet, ended;

	if( IsDebuggerPresent() ) {
		if( MessageBox( NULL, "An exception has occurred: press Yes to debug, No to create a crash dump.",
			"OpenWolf Error", MB_ICONERROR | MB_YESNO | MB_DEFBUTTON1 ) == IDYES )
			return EXCEPTION_CONTINUE_SEARCH; 
	}

	h = CreateThread( NULL, 0, DoGenerateCrashDump, pParams, CREATE_SUSPENDED, &tid );
	params->ignoreThreadIds[params->numIgnoreThreads++] = tid;

	dlg = CreateDialog( g_wv.hInstance, MAKEINTRESOURCE( IDD_CRASH_REPORT ), NULL, DialogProc );
	ShowWindow( dlg, SW_SHOWNORMAL );

	ended = FALSE;
	ResumeThread( h );

	while( (mRet = GetMessage( &msg, 0, 0, 0 )) != 0 ) {
		if( mRet == -1 ) {
			break;
		}

		TranslateMessage( &msg );
		DispatchMessage( &msg );

		if( !ended && WaitForSingleObject( h, 0 ) == WAIT_OBJECT_0 ) {
			HWND btn = GetDlgItem( dlg, IDOK );

			ended = TRUE;

			if( btn ) {
				EnableWindow( btn, TRUE );
				UpdateWindow( btn );
				InvalidateRect( btn, NULL, TRUE );
			} else {
				break;
			}
		}
	}

	if( !ended ) {
		WaitForSingleObject( h, INFINITE );
	}

	{
		DWORD hRet;
		GetExitCodeThread( h, &hRet );

		if( hRet == 0 ) {
			MessageBox( NULL, "There was an error. Please submit a bug report.", "Error", MB_ICONERROR | MB_OK | MB_DEFBUTTON1 );
		} else {
			char msg[1024];

			_snprintf( msg, sizeof( msg ),
				"There was an error. Please submit a bug report.\n\n"
				"No error info was saved (code:0x%X).", hRet );

			MessageBox( NULL, msg, "Error", MB_ICONERROR | MB_OK | MB_DEFBUTTON1 );
		}
	}

	DestroyWindow( dlg );

	switch( params->excCode ) {
		
	case 1: //SEH_CAPTURE_EXC:
		return (DWORD)EXCEPTION_CONTINUE_EXECUTION;
		

	default:
		return (DWORD)EXCEPTION_EXECUTE_HANDLER;
	}
}

/*
==============
ReportCrash
==============
*/
static int ReportCrash( DWORD excCode, PEXCEPTION_POINTERS pExcPtrs )
{	
#if 0//def _DEBUG
	
	return EXCEPTION_CONTINUE_SEARCH;

#else

	/*
		Launch off another thread to get the crash dump.

		Note that this second thread will pause this thread so *be certain* to
		keep all GUI stuff on it and *not* on this thread.
	*/

	dumpParams_t params;

	DWORD tid, ret;
	HANDLE h;

	Com_Memset( &params, 0, sizeof( params ) );
	params.excThreadId = GetCurrentThreadId();
	params.excCode = excCode;
	params.pExcPtrs = pExcPtrs;

	h = CreateThread( NULL, 0, DoReportCrashGUI, &params, CREATE_SUSPENDED, &tid );
	params.ignoreThreadIds[params.numIgnoreThreads++] = tid;

	ResumeThread( h );
	WaitForSingleObject( h, INFINITE );

	GetExitCodeThread( h, &ret );
	return (int)ret;
#endif
}

/*
==============
Game_Main
==============
*/
int Game_Main( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow ) {
	int retcode;

	__try {
		retcode = Win_Main( hInstance, hPrevInstance, lpCmdLine, nCmdShow );
	}
	__except( ReportCrash( GetExceptionCode(), GetExceptionInformation() ) ) {
		retcode = -1;
	}

	return retcode;
}

/*
==============
WinMain
==============
*/
int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow ) {
	return Game_Main( hInstance, hPrevInstance, lpCmdLine, nCmdShow );
}

#endif