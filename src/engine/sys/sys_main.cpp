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

// Dushan
#include <CPUInfo.h>
// the CPUInfo.h implemntation of lengthof is unsafe, so use the one
// from q_shared.h
#undef lengthof

#include <signal.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/types.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#ifdef _WIN32
#include <windows.h>
#endif

#ifndef DEDICATED
#ifdef USE_LOCAL_HEADERS
#include "SDL.h"
#else
#include <SDL.h>
#endif
#endif

#include "sys_local.h"
#include "sys_loadlib.h"

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"


static char binaryPath[ MAX_OSPATH ] = { 0 };
static char installPath[ MAX_OSPATH ] = { 0 };
static char libPath[ MAX_OSPATH ] = { 0 };

#ifdef USE_CURSES
static bool nocurses = false;
void CON_Init_tty( void );
#endif


/*
=================
Sys_SetBinaryPath
=================
*/
static void Sys_SetBinaryPath(const char *path) {
	Q_strncpyz(binaryPath, path, sizeof(binaryPath));
}

/*
=================
Sys_BinaryPath
=================
*/
static char *Sys_BinaryPath(void) {
	return binaryPath;
}

/*
=================
Sys_SetDefaultInstallPath
=================
*/
static void Sys_SetDefaultInstallPath(const char *path) {
	Q_strncpyz(installPath, path, sizeof(installPath));
}

/*
=================
Sys_DefaultInstallPath
=================
*/
char *Sys_DefaultInstallPath(void) {
	static char installdir[MAX_OSPATH];

	Com_sprintf(installdir, sizeof(installdir), "%s", Sys_Cwd());

	Q_strreplace(installdir, sizeof(installdir), "bin32", "");
	Q_strreplace(installdir, sizeof(installdir), "bin64", "");

	Q_strreplace(installdir, sizeof(installdir), "src/engine", "");
	Q_strreplace(installdir, sizeof(installdir), "src\\engine", "");
	
	Q_strreplace(installdir, sizeof(installdir), "bin/win32", "");
	Q_strreplace(installdir, sizeof(installdir), "bin\\win32", "");
	
	Q_strreplace(installdir, sizeof(installdir), "bin/win64", "");
	Q_strreplace(installdir, sizeof(installdir), "bin\\win64", "");
	
	Q_strreplace(installdir, sizeof(installdir), "bin/linux-x86", "");
	Q_strreplace(installdir, sizeof(installdir), "bin/linux-x86_64", "");

	Q_strreplace(installdir, sizeof(installdir), "bin/freebsd-i386", "");
	Q_strreplace(installdir, sizeof(installdir), "bin/freebsd-amd64", "");
	
	// MacOS X x86 and x64
	Q_strreplace(installdir, sizeof(installdir), "bin/macosx", "");	

	return installdir;
}

/*
=================
Sys_SetDefaultLibPath
=================
*/
void Sys_SetDefaultLibPath(const char *path) {
	Q_strncpyz(libPath, path, sizeof(libPath));
}

/*
=================
Sys_DefaultLibPath
=================
*/
char *Sys_DefaultLibPath(void) {
	if (*libPath) {
		return libPath;
	} else {
		return Sys_Cwd();
	}
}

/*
=================
Sys_DefaultAppPath
=================
*/
char *Sys_DefaultAppPath(void) {
	return Sys_BinaryPath();
}

/*
=================
Sys_In_Restart_f

Restart the input subsystem
=================
*/
void Sys_In_Restart_f( void ) {
	IN_Restart( );
}

/*
=================
Sys_ConsoleInput

Handle new console input
=================
*/
#if !defined (_WIN32)
char *Sys_ConsoleInput(void) {
	return CON_Input( );
}
#endif

#ifdef DEDICATED
#	define PID_FILENAME PRODUCT_NAME_UPPPER "_server.pid"
#else
#	define PID_FILENAME PRODUCT_NAME_UPPPER ".pid"
#endif

/*
=================
Sys_PIDFileName
=================
*/
static char *Sys_PIDFileName( void ) {
	return va( "%s/%s", Sys_TempPath( ), PID_FILENAME );
}

/*
=================
Sys_WritePIDFile

Return true if there is an existing stale PID file
=================
*/
bool Sys_WritePIDFile( void ) {
	char      *pidFile = Sys_PIDFileName( );
	FILE      *f;
	bool	  stale = false;

	// First, check if the pid file is already there
	if( ( f = fopen( pidFile, "r" ) ) != NULL ) {
		char  pidBuffer[ 64 ] = { 0 };
		int   pid;

		fread( pidBuffer, sizeof( char ), sizeof( pidBuffer ) - 1, f );
		fclose( f );

		pid = atoi( pidBuffer );
		if( !Sys_PIDIsRunning( pid ) ) {
			stale = true;
		}
	}

	if( ( f = fopen( pidFile, "w" ) ) != NULL ) {
		fprintf( f, "%d", Sys_PID( ) );
		fclose( f );
	} else {
		Com_Printf( S_COLOR_YELLOW "Couldn't write %s.\n", pidFile );
	}

	return stale;
}

/*
=================
Sys_Exit

Single exit point (regular exit or in case of error)
=================
*/
static void Sys_Exit( int exitCode ) {
	CON_Shutdown( );

#ifndef DEDICATED
	SDL_Quit( );
#endif

	if( exitCode < 2 ) {
		// Normal exit
		remove( Sys_PIDFileName( ) );
	}

	exit( exitCode );
}

/*
=================
Sys_Quit
=================
*/
void Sys_Quit( void ) {
#if defined (_WIN32)
	timeEndPeriod( 1 );
	IN_Shutdown();
 	Sys_DestroyConsole();
#endif
	Sys_Exit( 0 );
}

/*
=================
Sys_GetProcessorFeatures
=================
*/
cpuFeatures_t Sys_GetProcessorFeatures( void )
{
	cpuFeatures_t features = (cpuFeatures_t)0;
	CPUINFO cpuinfo;
	
	GetCPUInfo( &cpuinfo, CI_FALSE );

	/*
	if( HasCPUID() )              features |= CF_RDTSC;
	if( HasMMX( &cpuinfo ) )      features |= CF_MMX;
	if( HasMMXExt( &cpuinfo ) )   features |= CF_MMX_EXT;
	if( Has3DNow( &cpuinfo ) )    features |= CF_3DNOW;
	if( Has3DNowExt( &cpuinfo ) ) features |= CF_3DNOW_EXT;
	if( HasSSE( &cpuinfo ) )      features |= CF_SSE;
	if( HasSSE2( &cpuinfo ) )     features |= CF_SSE2;
	if( HasSSE3( &cpuinfo ) )     features |= CF_SSE3;
	if( HasSSSE3( &cpuinfo ) )    features |= CF_SSSE3;
	if( HasSSE4_1( &cpuinfo ) )   features |= CF_SSE4_1;
	if( HasSSE4_2( &cpuinfo ) )   features |= CF_SSE4_2;
	if( HasHTT( &cpuinfo ) )      features |= CF_HasHTT;
	if( HasSerial( &cpuinfo ) )   features |= CF_HasSerial;
	if( Is64Bit( &cpuinfo ) )     features |= CF_Is64Bit;*/

	return features;
}

/*
=================
Sys_Init
=================
*/
#if defined _WIN32
extern void Sys_ClearViewlog_f( void );
#endif

void Sys_Init(void) {
	Cmd_AddCommand( "in_restart", Sys_In_Restart_f );
#if defined (_WIN32)
	Cmd_AddCommand( "clearviewlog", Sys_ClearViewlog_f );

	Sys_PrintCpuInfo();
	Sys_PrintOSInfo();
	Sys_PrintMemoryInfo();
#endif
	Cvar_Set( "arch", OS_STRING " " ARCH_STRING );
	Cvar_Set( "username", Sys_GetCurrentUser( ) );
}

/*
=================
Sys_AnsiColorPrint

Transform Q3 colour codes to ANSI escape sequences
=================
*/
void Sys_AnsiColorPrint( const char *msg ) {
    static char buffer[ MAXPRINTMSG ];
    int         length = 0;

    // colors hash from http://wolfwiki.anime.net/index.php/Color_Codes
    static int  etAnsiHash[][6] = {
		{0, 30,   0, '0', 'p', 'P'}, 
        {1, 31,   0, '1', 'q', 'Q'},
        {1, 32,   0, '2', 'r', 'R'},
        {1, 33,   0, '3', 's', 'S'},
        {1, 34,   0, '4', 't', 'T'},
        {1, 35,   0, '5', 'u', 'U'},
        {1, 36,   0, '6', 'v', 'V'},
        {1, 37,   0, '7', 'w', 'W'},

        {38, 5, 208, '8', 'x', 'X'},
        {1, 30,   0, '9', 'y', 'Y'},
        {0, 37,   0, 'z', 'Z', ':'}, // the same
        {0, 37,   0, '[', '{', ';'}, // --------
        {0, 32,   0, '<', '\\','|'},
        {0, 33,   0, '=', ']', '}'},
        {0, 34,   0, '>', '~',  0 },
        {0, 31,   0, '?', '_',  0 },
        {38, 5,  94, '@', '`',  0 },

        {38, 5, 214, 'a', 'A', '!'},
        {38, 5,  30, 'b', 'B', '"'},
        {38, 5,  90, 'c', 'C', '#'},
        {38, 5,  33, 'd', 'D', '$'},
        {38, 5,  93, 'e', 'E', '%'},
        {38, 5,  38, 'f', 'F', '&'},
        {38, 5, 194, 'g', 'G', '\''},
        {38, 5,  29, 'h', 'H', '('},

        {38, 5, 197, 'i', 'I', ')'},
        {38, 5, 124, 'j', 'J', '*'},
        {38, 5, 130, 'k', 'K', '+'},
        {38, 5, 179, 'l', 'L', ','},
        {38, 5, 143, 'm', 'M', '-'},
        {38, 5, 229, 'n', 'N', '.'},
        {38, 5, 127, 'o', 'O', '/'}
    };

    while( *msg ) {
		if( Q_IsColorString( msg ) || *msg == '\n' ) {
			// First empty the buffer
            if( length > 0 ) {
                buffer[ length ] = '\0';
                fputs( buffer, stderr );
                length = 0;
            }

            if( *msg == '\n' ) {
                // Issue a reset and then the newline
                fputs( "\033[0m\n", stderr );
                msg++;
            } else {
                // Print the color code
                                
                int i, j, _found = 0;
                for (i = 0; i < 33; i++) {
                    if (_found) {
                        break;
					}
                                        
                    for (j = 3; j < 6; j++) {
                        if (etAnsiHash[i][j] == 0) {
                            break;
						}

                        if (*(msg + 1) == etAnsiHash[i][j]) {
                            if (etAnsiHash[i][2] == 0) {
                                Com_sprintf( buffer, sizeof( buffer ), "\033[%d;%dm", etAnsiHash[i][0], etAnsiHash[i][1]);
                                _found = 1;
                                break;
                            } else {
                                Com_sprintf( buffer, sizeof( buffer ), "\033[%d;%d;%dm", etAnsiHash[i][0], etAnsiHash[i][1], etAnsiHash[i][2]);
                                _found = 1;
                                break;
                            }
                        }
					}
            }
            fputs( buffer, stderr );
            msg += 2;
        }
    } else {
		if(length >= MAXPRINTMSG - 1) {
            break;
		}
		
		buffer[ length ] = *msg;
        length++;
        msg++;
		}
	}

    // Empty anything still left in the buffer
    if( length > 0 ) {
        buffer[ length ] = '\0';
        fputs( buffer, stderr );
    }
}

/*
=================
Host_RecordError
=================
*/
static void Host_RecordError( const char *msg )  {
	msg;
}

/*
=================
Sys_WriteDump
=================
*/
void Sys_WriteDump( const char *fmt, ... ) {
#if defined( _WIN32 )

#ifndef DEVELOPER
	if( com_developer && com_developer->integer )
#endif
	{
		//this memory should live as long as the SEH is doing its thing...I hope
		static char msg[2048];
		va_list vargs;

		/*
			Do our own vsnprintf as using va's will change global state
			that might be pertinent to the dump.
		*/

		va_start( vargs, fmt );
		vsnprintf( msg, sizeof( msg ) - 1, fmt, vargs );
		va_end( vargs );

		msg[sizeof( msg ) - 1] = 0; //ensure null termination

		Host_RecordError( msg );
	}

#endif
}

/*
=================
Sys_Print
=================
*/
void Sys_Print( const char *msg ) {
#if defined (_WIN32)
	Conbuf_AppendText( msg );
#else
	CON_LogWrite( msg );
	CON_Print( msg );
#endif
}

/*
=================
Sys_Error
=================
*/
void QDECL Sys_Error ( const char *error, ... ) {
	va_list argptr;
	char    string[4096];
#if defined (_WIN32)
	MSG		msg;
#endif

	va_start (argptr,error);
	Q_vsnprintf (string, sizeof(string), error, argptr);
	va_end (argptr);

#if defined (_WIN32)
	Conbuf_AppendText( string );
	Conbuf_AppendText( "\n" );
#else
	// Print text in the console window/box
	Sys_Print( string );
	Sys_Print( "\n" );
#endif

#if defined (_WIN32)
	Sys_SetErrorText( string );
	Sys_ShowConsole( 1, true );

	timeEndPeriod( 1 );

	IN_Shutdown();

	// wait for the user to quit
	while ( 1 ) {
		if ( !GetMessage( &msg, NULL, 0, 0 ) ) {
			Com_Quit_f();
		}
		TranslateMessage( &msg );
		DispatchMessage( &msg );
	}

	Sys_DestroyConsole();
#endif

	CL_Shutdown(va("Received signal %d", signal), true, true);

	Sys_ErrorDialog( string );

	Sys_Exit( 3 );
}

/*
=================
Sys_UnloadDll
=================
*/
void Sys_UnloadDll( void *dllHandle ) {
	if( !dllHandle ) {
		Com_Printf("Sys_UnloadDll(NULL)\n");
		return;
	}

	Sys_UnloadLibrary(dllHandle);
}
/*
=================
Sys_GetDLLName

Used to load a development dll instead of a virtual machine
=================
*/
#if defined (UPDATE_SERVER)
int cl_connectedToPureServer;
#else
extern int cl_connectedToPureServer;
#endif

char *Sys_GetDLLName(const char *name) {
#if defined _WIN32
	return va("%s_mp_" ARCH_STRING DLL_EXT, name);
#else
	return va("%s.mp." ARCH_STRING DLL_EXT, name);
#endif
}

/*
=================
Sys_TryLibraryLoad
=================
*/
static void* Sys_TryLibraryLoad(const char* base, const char* gamedir, const char* fname, char* fqpath ) {
    void *libHandle = NULL;
    char *fn;

    *fqpath = 0;

    fn = FS_BuildOSPath( base, gamedir, fname );
    Com_Printf( "Sys_LoadDll(%s)... \n", fn );

    libHandle = Sys_LoadLibrary(fn);

    if(!libHandle) {
        Com_Printf( "Sys_LoadDll(%s) failed:\n\"%s\"\n", fn, Sys_LibraryError() );
        return NULL;
    }

	// Dushan - show info only on developer 1
    Com_DPrintf ( "Sys_LoadDll(%s): succeeded ...\n", fn );
    Q_strncpyz ( fqpath , fn , MAX_QPATH ) ;

    return libHandle;
}

/*
=================
Sys_LoadDll

Used to load a development dll instead of a virtual machine
#1 look in fs_homepath
#2 look in fs_basepath
#4 look in fs_libpath under FreeBSD
=================
*/
void *QDECL Sys_LoadDll( const char *name, char *fqpath, intptr_t (QDECL  **entryPoint)(int, ...), intptr_t (QDECL *systemcalls)(intptr_t, ...) ) {
	void    *libHandle = NULL;
	void	(QDECL *dllEntry)( intptr_t (QDECL *syscallptr)(intptr_t, ...) );
	char    fname[MAX_QPATH], *basepath, *homepath, *gamedir, *libpath;

	assert( name );

	//Q_snprintf (fname, sizeof(fname), Sys_GetDLLName( "%s" ), name);
	Q_strncpyz(fname, Sys_GetDLLName(name), sizeof(fname));

    // TODO: use fs_searchpaths from files.c
    basepath = Cvar_VariableString( "fs_basepath" );
    homepath = Cvar_VariableString( "fs_homepath" );
    gamedir = Cvar_VariableString( "fs_game" );
	libpath = Cvar_VariableString( "fs_libpath" );

#ifndef DEDICATED
    // if the server is pure, extract the dlls from the mp_bin.pk3 so
    // that they can be referenced
    if ( Cvar_VariableValue( "sv_pure" ) && Q_stricmp( name, "qagame" ) ) {
		FS_CL_ExtractFromPakFile( homepath, gamedir, fname );
    }
#endif

	libHandle = Sys_TryLibraryLoad(homepath, gamedir, fname, fqpath);

	if (!libHandle && libpath && libpath[0]) {
		libHandle = Sys_TryLibraryLoad(libpath, gamedir, fname, fqpath);
	}

	if(!libHandle && basepath) {
		libHandle = Sys_TryLibraryLoad(basepath, gamedir, fname, fqpath);
	}

	if(!libHandle) {
		Com_Printf( "Sys_LoadDll(%s) could not find it\n", fname );
		return NULL;
	}

	if(!libHandle) {
		Com_Printf( "Sys_LoadDll(%s) failed:\n\"%s\"\n", name, Sys_LibraryError() );
		return NULL;
	}

	// Try to load the dllEntry and vmMain function.
	dllEntry = ( void ( QDECL * )( intptr_t ( QDECL * )( intptr_t, ... ) ) )Sys_LoadFunction( libHandle, "dllEntry" );
	*entryPoint = ( intptr_t ( QDECL * )( int,... ) )Sys_LoadFunction( libHandle, "vmMain" );

	if ( !*entryPoint || !dllEntry ) {
#ifndef NDEBUG
		if (!dllEntry) {
			Com_Error ( ERR_FATAL, "Sys_LoadDll(%s) failed SDL_LoadFunction(dllEntry):\n\"%p\" !\n", name, Sys_LibraryError() );
		} else {
			Com_Error ( ERR_FATAL, "Sys_LoadDll(%s) failed SDL_LoadFunction(vmMain):\n\"%p\" !\n", name, Sys_LibraryError() );
		}
#else
		if (!dllEntry) {
			Com_Printf ( "Sys_LoadDll(%s) failed SDL_LoadFunction(dllEntry):\n\"%p\" !\n", name, Sys_LibraryError() );
		} else {
			Com_Printf ( "Sys_LoadDll(%s) failed SDL_LoadFunction(vmMain):\n\"%p\" !\n", name, Sys_LibraryError() );
		}
#endif
		Sys_UnloadLibrary(libHandle);
		return NULL;
	}

	Com_Printf ( "Sys_LoadDll(%s) found vmMain function at %p\n", name, *entryPoint );
	dllEntry( systemcalls );

	Com_Printf ( "Sys_LoadDll(%s) succeeded!\n", name );

	// Copy the fname to fqpath.
	Q_strncpyz ( fqpath , fname , MAX_QPATH ) ;

	return libHandle;
}

/*
=================
Sys_ParseArgs
=================
*/
static void Sys_ParseArgs( int argc, char **argv ) {
#if defined(USE_CURSES)
	int i;
#endif

	if( argc == 2 ) {
		if( !strcmp( argv[1], "--version" ) || !strcmp( argv[1], "-v" ) ) {
			const char* date = __DATE__;
#ifdef DEDICATED
			fprintf( stdout, Q3_VERSION " dedicated server (%s)\n", date );
#else
			fprintf( stdout, Q3_VERSION " client (%s)\n", date );
#endif
			Sys_Exit( 0 );
		}
	}
#ifdef USE_CURSES
	for (i = 1; i < argc; i++) {
		if( !strcmp( argv[i], "+nocurses" ) ) {
			nocurses = true;
			break;
		}
	}
#endif
}

#ifndef DEFAULT_BASEDIR
#	ifdef MACOS_X
#		define DEFAULT_BASEDIR Sys_StripAppBundle(Sys_BinaryPath())
#	else
#		define DEFAULT_BASEDIR Sys_BinaryPath()
#	endif
#endif

/*
================
SignalToString
================
*/
static const char *SignalToString(int sig) {
	switch(sig) {
		case SIGINT:
			return "Terminal interrupt signal";
		case SIGILL:
			return "Illegal instruction";
		case SIGFPE:
			return "Erroneous arithmetic operation";
		case SIGSEGV:
			return "Invalid memory reference";
		case SIGTERM:
			return "Termination signal";
		case SIGBREAK:
			return "Control-break";
		case SIGABRT:
			return "Process abort signal";
		default:
			return "unknown";
	}
}

/*
=================
Sys_SigHandler
=================
*/
void Sys_SigHandler( int signal ) {
	static bool signalcaught = false;

	if( signalcaught ) {
		Com_Printf("DOUBLE SIGNAL FAULT: Received signal %d: \"%s\", exiting...\n", signal, SignalToString(signal));
		exit(1);
	} else {
		signalcaught = true;
		VM_Forced_Unload_Start();
#ifndef DEDICATED
		CL_Shutdown(va("Received signal %d", signal), true, true);
#endif
		SV_Shutdown(va("Received signal %d", signal) );
		VM_Forced_Unload_Done();
	}

	if( signal == SIGTERM || signal == SIGINT ) {
		Sys_Exit( 1 );
	} else {
		Sys_Exit( 2 );
	}
}

/*
=================
main
=================
*/
int main( int argc, char **argv ) {
	int   i;
	char  commandLine[ MAX_STRING_CHARS ] = { 0 };
#ifndef DEDICATED
	// Run time
	const SDL_version *ver = SDL_Linked_Version( );
#endif

#ifndef DEDICATED
	// SDL version check

	// Compile time
#	if !SDL_VERSION_ATLEAST(MINSDL_MAJOR,MINSDL_MINOR,MINSDL_PATCH)
#		error A more recent version of SDL is required
#	endif

#define MINSDL_VERSION \
	XSTRING(MINSDL_MAJOR) "." \
	XSTRING(MINSDL_MINOR) "." \
	XSTRING(MINSDL_PATCH)

	if( SDL_VERSIONNUM( ver->major, ver->minor, ver->patch ) < SDL_VERSIONNUM( MINSDL_MAJOR, MINSDL_MINOR, MINSDL_PATCH ) ) {
		Sys_Dialog( DT_ERROR, va( "SDL version " MINSDL_VERSION " or greater is required, "
			"but only version %d.%d.%d was found. You may be able to obtain a more recent copy "
			"from http://www.libsdl.org/.", ver->major, ver->minor, ver->patch ), "SDL Library Too Old" );

		Sys_Exit( 1 );
	}
#endif

	Sys_ParseArgs( argc, argv );

	Sys_PlatformInit( );

	// Set the initial time base
	Sys_Milliseconds( );

	Sys_SetBinaryPath( Sys_Dirname( argv[ 0 ] ) );
	Sys_SetDefaultInstallPath( DEFAULT_BASEDIR );
	Sys_SetDefaultLibPath( DEFAULT_BASEDIR );

	// Concatenate the command line for passing to Com_Init
	for( i = 1; i < argc; i++ ) {
		const bool containsSpaces = strchr(argv[i], ' ') != NULL;

		if( !strcmp( argv[ i ], "+nocurses" ) ) {
			continue;
		}

		if( !strcmp( argv[ i ], "+showconsole" ) ) {
			continue;
		}

		if (containsSpaces) {
			Q_strcat( commandLine, sizeof( commandLine ), "\"" );
		}

		Q_strcat( commandLine, sizeof( commandLine ), argv[ i ] );

		if (containsSpaces) {
			Q_strcat( commandLine, sizeof( commandLine ), "\"" );
		}

		Q_strcat( commandLine, sizeof( commandLine ), " " );
	}

#ifdef USE_CURSES
	if( nocurses ) {
		CON_Init_tty( );
	} else {
		CON_Init( );
	}
#else
	CON_Init( );
#endif

	Com_Init( commandLine );
	NET_Init( );

	signal( SIGILL, Sys_SigHandler );
	signal( SIGFPE, Sys_SigHandler );
	signal( SIGSEGV, Sys_SigHandler );
	signal( SIGTERM, Sys_SigHandler );
	signal( SIGINT, Sys_SigHandler );

	while( 1 ) {
		IN_Frame( );
		Com_Frame( );
	}

	return 0;
}

/*
================
Sys_SnapVector
================
*/
// *INDENT-OFF*
long fastftol( float f )
{
#if defined(__WIN64__)
	return (long)((f>0.0f) ? (f + 0.5f):(f -0.5f));

#elif defined(_WIN32)
	static int tmp;

	__asm fld f
	__asm fistp tmp
	// rain - WTF - why do they set the return this way?
	__asm mov eax, tmp
#else
	// rain - gcc-style inline asm
	// zinx - meh, gcc's lrint is sane, so use that. fixed inline asm too, though.
	/*
	asm(
		"fld %1\n\t"
		"fistp %0\n"
		: "=m" (tmp) // outputs
		: "f" (f) // inputs
	);
	return tmp;
	*/
	return lrint( f );
#endif
}

void Sys_SnapVector( float *v )
{
#if defined(__WIN64__)
	*v = (float)fastftol( *v );
	v++;
	*v = (float)fastftol( *v );
	v++;
	*v = (float)fastftol( *v );

#elif defined(_WIN32)
	int i;
	float f;

	f = *v;
	__asm fld f;
	__asm fistp i;
	*v = i;
	v++;
	f = *v;
	__asm fld f;
	__asm fistp i;
	*v = i;
	v++;
	f = *v;
	__asm fld f;
	__asm fistp i;
	*v = i;
	/*
	*v = fastftol(*v);
	v++;
	*v = fastftol(*v);
	v++;
	*v = fastftol(*v);
	*/
#else
	// rain - gcc has different inline asm, but I'm not going to emulate
	// that here for now...
	*v = (float)fastftol( *v );
	v++;
	*v = (float)fastftol( *v );
	v++;
	*v = (float)fastftol( *v );
#endif
}