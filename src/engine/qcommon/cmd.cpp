/*
===========================================================================

OpenWolf GPL Source Code
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company. 
Copyright (C) 2012 Dusan Jocic <dusanjocic@msn.com>

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

// cmd_c -- Quake script command processing module

#include "../qcommon/q_shared.h"
#include "qcommon.h"
//#include "../client/keys.h"
#include "../idLib/precompiled.h"

#define MAX_CMD_BUFFER  131072
#define MAX_CMD_LINE    1024

typedef struct
{
	byte           *data;
	int             maxsize;
	int             cursize;
} cmd_t;

int             cmd_wait;
cmd_t           cmd_text;
byte            cmd_text_buf[MAX_CMD_BUFFER];

// Delay stuff 

#define MAX_DELAYED_COMMANDS    64 
#define CMD_DELAY_FRAME_FIRE 1
#define CMD_DELAY_UNUSED 0

typedef enum
{
  CMD_DELAY_MSEC,
  CMD_DELAY_FRAME
} cmdDelayType_t;

typedef struct
{
        char    name[MAX_CMD_LINE];
        char    text[MAX_CMD_LINE];
        int     delay;
        cmdDelayType_t 	type;
} delayed_cmd_s; 
 
delayed_cmd_s delayed_cmd[MAX_DELAYED_COMMANDS]; 

typedef struct cmd_function_s
{
	struct cmd_function_s *next;
	char                  *name;
	xcommand_t             function;
	completionFunc_t	   complete;
	char                  *desc;
} cmd_function_t;

typedef struct cmdContext_s
{
	int		argc;
	char	*argv[ MAX_STRING_TOKENS ];	// points into cmd.tokenized
	char	tokenized[ BIG_INFO_STRING + MAX_STRING_TOKENS ];	// will have 0 bytes inserted
	char	cmd[ BIG_INFO_STRING ]; // the original command we received (no token processing)
} cmdContext_t;

static cmdContext_t		cmd;
static cmdContext_t		savedCmd;

static cmd_function_t *cmd_functions;	// possible commands to execute



//=============================================================================

/*
============
Cmd_FindCommand
============
*/
cmd_function_t *Cmd_FindCommand( const char *cmd_name )
{
	cmd_function_t *cmd;
	for( cmd = cmd_functions; cmd; cmd = cmd->next )
	{
		if( !Q_stricmp( cmd_name, cmd->name ) ) 
		{
			return cmd;
		}
	}
	return NULL;
}

/*
============
Cmd_Wait_f

Causes execution of the remainder of the command buffer to be delayed until
next frame.  This allows commands like:
bind g "cmd use rocket ; +attack ; wait ; -attack ; cmd use blaster"
============
*/
void Cmd_Wait_f(void)
{
	if(Cmd_Argc() == 2)
	{
		cmd_wait = atoi(Cmd_Argv(1));
		if ( cmd_wait < 0 ) {
			cmd_wait = 1; // ignore the argument
		}
	}
	else
	{
		cmd_wait = 1;
	}
}


/*
=============================================================================

						COMMAND BUFFER

=============================================================================
*/

/*
============
Cbuf_Init
============
*/
void Cbuf_Init(void)
{
	cmd_text.data = cmd_text_buf;
	cmd_text.maxsize = MAX_CMD_BUFFER;
	cmd_text.cursize = 0;
}

/*
============
Cbuf_AddText

Adds command text at the end of the buffer, does NOT add a final \n
============
*/
void Cbuf_AddText(const char *text)
{
	int             l;

	l = strlen(text);

	if(cmd_text.cursize + l >= cmd_text.maxsize)
	{
		Com_Printf("Cbuf_AddText: overflow\n");
		return;
	}
	Com_Memcpy(&cmd_text.data[cmd_text.cursize], text, l);
	cmd_text.cursize += l;
}


/*
============
Cbuf_InsertText

Adds command text immediately after the current command
Adds a \n to the text
============
*/
void Cbuf_InsertText(const char *text)
{
	int             len;
	int             i;

	len = strlen(text) + 1;
	if(len + cmd_text.cursize > cmd_text.maxsize)
	{
		Com_Printf("Cbuf_InsertText overflowed\n");
		return;
	}

	// move the existing command text
	for(i = cmd_text.cursize - 1; i >= 0; i--)
	{
		cmd_text.data[i + len] = cmd_text.data[i];
	}

	// copy the new text in
	Com_Memcpy(cmd_text.data, text, len - 1);

	// add a \n
	cmd_text.data[len - 1] = '\n';

	cmd_text.cursize += len;
}


/*
============
Cbuf_ExecuteText
============
*/
void Cbuf_ExecuteText(int exec_when, const char *text)
{
	switch (exec_when)
	{
		case EXEC_NOW:
			if(text && strlen(text) > 0)
			{
				Com_DPrintf(S_COLOR_YELLOW "EXEC_NOW %s\n", text);
				Cmd_ExecuteString(text);
			}
			else
			{
				Cbuf_Execute();
				Com_DPrintf(S_COLOR_YELLOW "EXEC_NOW %s\n", cmd_text.data);
			}
			break;
		case EXEC_INSERT:
			Cbuf_InsertText(text);
			break;
		case EXEC_APPEND:
			Cbuf_AddText(text);
			break;
		default:
			Com_Error(ERR_FATAL, "Cbuf_ExecuteText: bad exec_when");
	}
}

/*
============
Cbuf_Execute
============
*/
void Cbuf_Execute(void)
{
	int             i;
	char           *text;
	char            line[MAX_CMD_LINE];
	int             quotes;

	while(cmd_text.cursize)
	{
		if(cmd_wait)
		{
			// skip out while text still remains in buffer, leaving it
			// for next frame
			cmd_wait--;
			break;
		}

		// find a \n or ; line break
		text = (char *)cmd_text.data;

		quotes = 0;
		for(i = 0; i < cmd_text.cursize; i++)
		{
			if(text[i] == '\\' && text[i+1] == '"') {
				i++;
				continue;
			}

			if(text[i] == '"')
			{
				quotes++;
			}
			if(!(quotes & 1) && text[i] == ';')
			{
				break;			// don't break if inside a quoted string
			}
			if(text[i] == '\n' || text[i] == '\r')
			{
				break;
			}
		}

		if(i >= (MAX_CMD_LINE - 1))
		{
			i = MAX_CMD_LINE - 1;
		}

		memcpy(line, text, i);
		line[i] = 0;

// delete the text from the command buffer and move remaining commands down
// this is necessary because commands (exec) can insert data at the
// beginning of the text buffer

		if(i == cmd_text.cursize)
		{
			cmd_text.cursize = 0;
		}
		else
		{
			i++;
			cmd_text.cursize -= i;
			memmove(text, text + i, cmd_text.cursize);
		}

// execute the command line

		Cmd_ExecuteString(line);
	}
}

/*
==============================================================================

						COMMANDS DELAYING

==============================================================================
*/


/*
===============
Cdelay_Frame
===============
*/

void Cdelay_Frame( void ) {
	int i;
	qboolean run_it;
	
	for(i=0; (i<MAX_DELAYED_COMMANDS); i++)
	{
		run_it = qfalse;
		
		if(delayed_cmd[i].delay == CMD_DELAY_UNUSED)
			continue;
		
		//check if we should run the command (both type)
		if(delayed_cmd[i].type == CMD_DELAY_MSEC && delayed_cmd[i].delay < Sys_Milliseconds())
		{			
			run_it = qtrue;
		} else if(delayed_cmd[i].type == CMD_DELAY_FRAME)
		{
			delayed_cmd[i].delay -= 1;
			if(delayed_cmd[i].delay == CMD_DELAY_FRAME_FIRE)
				run_it = qtrue;
		}
		
		if(run_it)
		{
			delayed_cmd[i].delay = CMD_DELAY_UNUSED;
			Cbuf_ExecuteText(EXEC_NOW, delayed_cmd[i].text);
		}
	}
}





/*
==============================================================================

						SCRIPT COMMANDS

==============================================================================
*/


/*
===============
Cmd_ExecFile
===============
*/
static void Cmd_ExecFile( char *f )
{
	int i;

	COM_Compress (f);
	
	Cvar_Get( "arg_all", Cmd_ArgsFrom(2), CVAR_TEMP | CVAR_ROM | CVAR_USER_CREATED, "" );
	Cvar_Set( "arg_all", Cmd_ArgsFrom(2) );
	Cvar_Get( "arg_count", va( "%i", Cmd_Argc() - 2 ), CVAR_TEMP | CVAR_ROM | CVAR_USER_CREATED, "" );
	Cvar_Set( "arg_count", va( "%i", Cmd_Argc() - 2 ) );

	for (i = Cmd_Argc() - 2; i; i--)
	{
		Cvar_Get( va("arg_%i", i), Cmd_Argv( i + 1 ), CVAR_TEMP | CVAR_ROM | CVAR_USER_CREATED, "" );
		Cvar_Set( va("arg_%i", i), Cmd_Argv( i + 1 ) );
	}

	Cbuf_InsertText (f);
}

/*
===============
Cmd_Exec_f
===============
*/
void Cmd_Exec_f(void)
{
	union {
		char	*c;
		void	*v;
	} f;
	int		len;
	char	filename[MAX_QPATH];
	fileHandle_t h;
	qboolean success = qfalse;

	if (Cmd_Argc () < 2) {
		Com_Printf ("exec <filename> (args) : execute a script file\n");
		return;
	}

	Com_Printf ("execing %s\n", Cmd_Argv(1));

	Q_strncpyz( filename, Cmd_Argv(1), sizeof( filename ) );
	COM_DefaultExtension( filename, sizeof( filename ), ".cfg" );

	len = FS_SV_FOpenFileRead(filename, &h);
	if (h)
	{
		success = qtrue;
		f.v = Hunk_AllocateTempMemory(len + 1);
		FS_Read(f.v, len, h);
		f.c[len] = 0;
		FS_FCloseFile(h);
		Cmd_ExecFile(f.c);
		Hunk_FreeTempMemory(f.v);
	}

	FS_ReadFile( filename, &f.v);
	if (f.c) {
		success = qtrue;
		Cmd_ExecFile(f.c);
		FS_FreeFile (f.v);
	}

	if (!success)
		Com_Printf ("couldn't exec %s\n",Cmd_Argv(1));
}

/*
=============================================================================

					ALIASES

=============================================================================
*/

typedef struct cmd_alias_s
{
	struct cmd_alias_s	*next;
	char				*name;
	char				*exec;
} cmd_alias_t;

static cmd_alias_t	*cmd_aliases = NULL;

/*
============
Cmd_RunAlias_f
============
*/
void Cmd_RunAlias_f(void)
{
	cmd_alias_t	*alias;
	char 		*name = Cmd_Argv(0);
	char 		*args = Cmd_ArgsFrom(1);

	// Find existing alias
	for (alias = cmd_aliases; alias; alias=alias->next)
	{
		if (!Q_stricmp( name, alias->name ))
			break;
	}

	if (!alias)
		Com_Error(ERR_FATAL, "Alias: Alias %s doesn't exist", name);

	Cbuf_InsertText(va("%s %s", alias->exec, args));
}

/*
============
Cmd_WriteAliases
============
*/
void Cmd_WriteAliases(fileHandle_t f)
{
	char buffer[1024] = "clearaliases\n";
	cmd_alias_t *alias = cmd_aliases;
	FS_Write(buffer, strlen(buffer), f);
	while (alias)
	{
		Com_sprintf(buffer, sizeof(buffer), "alias %s \"%s\"\n", alias->name, Cmd_EscapeString(alias->exec));
		FS_Write(buffer, strlen(buffer), f);
		alias = alias->next;
	}
}

/*
============
Cmd_AliasList_f
============
*/
void Cmd_AliasList_f (void)
{
	cmd_alias_t	*alias;
	int			i;
	char		*match;

	if (Cmd_Argc() > 1)
		match = Cmd_Argv( 1 );
	else
		match = NULL;

	i = 0;
	for (alias = cmd_aliases; alias; alias = alias->next)
	{
		if (match && !Com_Filter(match, alias->name, qfalse))
			continue;
		Com_Printf ("%s ==> %s\n", alias->name, alias->exec);
		i++;
	}
	Com_Printf ("%i aliases\n", i);
}

/*
============
Cmd_ClearAliases_f
============
*/
void Cmd_ClearAliases_f(void)
{
	cmd_alias_t *alias = cmd_aliases;
	cmd_alias_t *next;
	while (alias)
	{
		next = alias->next;
		Cmd_RemoveCommand(alias->name);
		Z_Free(alias->name);
		Z_Free(alias->exec);
		Z_Free(alias);
		alias = next;
	}
	cmd_aliases = NULL;
	
	// update autogen.cfg
	cvar_modifiedFlags |= CVAR_ARCHIVE;
}

/*
============
Cmd_UnAlias_f
============
*/
void Cmd_UnAlias_f(void)
{
	cmd_alias_t *alias, **back;
	const char	*name;

	// Get args
	if (Cmd_Argc() < 2)
	{
		Com_Printf("unalias <name> : delete an alias\n");
		return;
	}
	name = Cmd_Argv(1);

	back = &cmd_aliases;
	while(1)
	{
		alias = *back;
		if (!alias)
			return;
		if (!Q_stricmp(name, alias->name))
		{
			*back = alias->next;
			Z_Free(alias->name);
			Z_Free(alias->exec);
			Z_Free(alias);
			Cmd_RemoveCommand(name);
	
			// update autogen.cfg
			cvar_modifiedFlags |= CVAR_ARCHIVE;
			return;
		}
		back = &alias->next;
	}
}

/*
============
Cmd_Alias_f
============
*/
void Cmd_Alias_f(void)
{
	cmd_alias_t	*alias;
	const char	*name;

	// Get args
	if (Cmd_Argc() < 2)
	{
		Com_Printf("alias <name> : show an alias\n");
		Com_Printf("alias <name> <exec> : create an alias\n");
		return;
	}
	name = Cmd_Argv(1);

	// Find existing alias
	for (alias = cmd_aliases; alias; alias = alias->next)
	{
		if (!Q_stricmp(name, alias->name))
			break;
	}

	// Modify/create an alias
	if (Cmd_Argc() > 2)
	{
		cmd_function_t	*cmd;

		// Crude protection from infinite loops
		if (!Q_stricmp(Cmd_Argv(2), name))
		{
			Com_Printf("Can't make an alias to itself\n");
			return;
		}

		// Don't allow overriding builtin commands
 		cmd = Cmd_FindCommand( name );
		if (cmd && cmd->function != Cmd_RunAlias_f)
		{
			Com_Printf("Can't override a builtin function with an alias\n");
			return;
		}

		// Create/update an alias
		if (!alias)
		{
			alias = (cmd_alias_t*)S_Malloc(sizeof(cmd_alias_t));
			alias->name = CopyString(name);
			alias->exec = CopyString(Cmd_ArgsFrom(2));
			alias->next = cmd_aliases;
			cmd_aliases = alias;
			Cmd_AddCommand(name, Cmd_RunAlias_f, "");
		}
		else
		{
			// Reallocate the exec string
			Z_Free(alias->exec);
			alias->exec = CopyString(Cmd_ArgsFrom(2));
			Cmd_AddCommand(name, Cmd_RunAlias_f, "");
		}
	}
	
	// Show the alias
	if (!alias)
		Com_Printf("Alias %s does not exist\n", name);
	else if (Cmd_Argc() == 2)
		Com_Printf("%s ==> %s\n", alias->name, alias->exec);
	
	// update autogen.cfg
	cvar_modifiedFlags |= CVAR_ARCHIVE;
}

/*
============
Cmd_AliasCompletion
============
*/
void	Cmd_AliasCompletion( void(*callback)(const char *s) ) {
	cmd_alias_t	*alias;
	
	for (alias=cmd_aliases ; alias ; alias=alias->next) {
		callback( alias->name );
	}
}

/*
============
Cmd_DelayCompletion
============
*/
void	Cmd_DelayCompletion( void(*callback)(const char *s) ) {
	int i;
	
	for (i = 0; i < MAX_DELAYED_COMMANDS; i++) {
		if (delayed_cmd[i].delay != CMD_DELAY_UNUSED)
			callback(delayed_cmd[i].name);
	}
}

/*
=============================================================================

					COMMAND EXECUTION

=============================================================================
*/


/*
============
Cmd_SaveCmdContext
============
*/
void Cmd_SaveCmdContext( void )
{
	Com_Memcpy( &savedCmd, &cmd, sizeof( cmdContext_t ) );
}

/*
============
Cmd_RestoreCmdContext
============
*/
void Cmd_RestoreCmdContext( void )
{
	Com_Memcpy( &cmd, &savedCmd, sizeof( cmdContext_t ) );
}

/*
============
Cmd_Argc
============
*/
int Cmd_Argc(void)
{
	return cmd.argc;
}

/*
============
Cmd_Argv
============
*/
char           *Cmd_Argv(int arg)
{
	if((unsigned)arg >= cmd.argc)
	{
		return "";
	}
	return cmd.argv[arg];
}

/*
============
Cmd_ArgvBuffer

The interpreted versions use this because
they can't have pointers returned to them
============
*/
void Cmd_ArgvBuffer(int arg, char *buffer, int bufferLength)
{
	Q_strncpyz(buffer, Cmd_Argv(arg), bufferLength);
}


/*
============
Cmd_Args

Returns a single string containing argv(1) to argv(argc()-1)
============
*/
char           *Cmd_Args(void)
{
	static char     cmd_args[MAX_STRING_CHARS];
	int             i;

	cmd_args[0] = 0;
	for(i = 1; i < cmd.argc; i++)
	{
		strcat(cmd_args, cmd.argv[i]);
		if(i != cmd.argc - 1)
		{
			strcat(cmd_args, " ");
		}
	}

	return cmd_args;
}

/*
============
Cmd_Args

Returns a single string containing argv(arg) to argv(argc()-1)
============
*/
char           *Cmd_ArgsFrom(int arg)
{
	static char     cmd_args[BIG_INFO_STRING];
	int             i;

	cmd_args[0] = 0;
	if(arg < 0)
	{
		arg = 0;
	}
	for(i = arg; i < cmd.argc; i++)
	{
		strcat(cmd_args, cmd.argv[i]);
		if(i != cmd.argc - 1)
		{
			strcat(cmd_args, " ");
		}
	}

	return cmd_args;
}

/*
============
Cmd_ArgsBuffer

The interpreted versions use this because
they can't have pointers returned to them
============
*/
void Cmd_ArgsBuffer(char *buffer, int bufferLength)
{
	Q_strncpyz(buffer, Cmd_Args(), bufferLength);
}

/*
============
Cmd_LiteralArgsBuffer

The interpreted versions use this because
they can't have pointers returned to them
============
*/
void	Cmd_LiteralArgsBuffer( char *buffer, int bufferLength ) {
	Q_strncpyz( buffer, cmd.cmd, bufferLength );
}


/*
============
Cmd_Cmd

Retrieve the unmodified command string
For rcon use when you want to transmit without altering quoting
ATVI Wolfenstein Misc #284
============
*/
char           *Cmd_Cmd()
{
	return cmd.cmd;
}

/*
============
Cmd_Cmd_FromNth

Retrieve the unmodified command string
For rcon use when you want to transmit without altering quoting
ATVI Wolfenstein Misc #284
============
*/
char *Cmd_Cmd_FromNth(int count)
{
	char *ret = cmd.cmd - 1;
	int i = 0, q = 0;

	while (count && *++ret)
	{
		if (!q && *ret == ' ')
			i = 1; // space found outside quotation marks
		if (i && *ret != ' ')
		{
			i = 0; // non-space found after space outside quotation marks
			--count; // one word fewer to scan
		}
		if (*ret == '"')
			q = !q; // found a quotation mark
		else if (*ret == '\\' && ret[1] == '"')
			++ret;
	}

	return ret;
}


/*
============
Cmd_EscapeString

Escape all \$ in a string into \$$
============
*/
char *Cmd_EscapeString(const char *in)
{
	static char buffer[MAX_STRING_CHARS];
	char *out = buffer;
	while (*in) {
		if (out + 3 - buffer >= sizeof(buffer)) {
			break;
		}
		if (in[0] == '\\' && in[1] == '$') {
			out[0] = '\\';
			out[1] = '$';
			out[2] = '$';
			in += 2;
			out += 3;
		} else {
			*out++ = *in++;
		}
	}
	*out = '\0';
	return buffer;
}
/*
============
Cmd_TokenizeString

Parses the given string into command line tokens.
The text is copied to a seperate buffer and 0 characters
are inserted in the apropriate place, The argv array
will point into this temporary buffer.
============
*/
// NOTE TTimo define that to track tokenization issues
//#define TKN_DBG
static void Cmd_TokenizeString2( const char *text_in, qboolean ignoreQuotes, qboolean parseCvar ) {
	char	*text;
	char	*textOut;
	const char *cvarName;
	char buffer[ BIG_INFO_STRING ];

#ifdef TKN_DBG
  // FIXME TTimo blunt hook to try to find the tokenization of userinfo
  Com_DPrintf("Cmd_TokenizeString: %s\n", text_in);
#endif

	// clear previous args
	cmd.argc = 0;
	cmd.cmd[ 0 ] = '\0';

	if ( !text_in ) {
		return;
	}
	
	// parse for cvar substitution
	if ( parseCvar )
	{
		Q_strncpyz( buffer, text_in, sizeof( buffer ) );
		text = buffer;
		textOut = cmd.cmd;
		while ( *text )
		{
			if ( text[0] != '\\' || text[1] != '$' )
			{
				if ( textOut == sizeof(cmd.cmd) + cmd.cmd - 1 )
					break;
				*textOut++ = *text++;
				continue;
			}
			text += 2;
			cvarName = text;
			while ( *text && *text != '\\' )
				text++;
			if ( *text == '\\' )
			{
				*text = 0;
				if ( Cvar_Flags( cvarName ) != CVAR_NONEXISTENT )
				{
					char cvarValue[ MAX_CVAR_VALUE_STRING ];
					char *badchar;
					Cvar_VariableStringBuffer( cvarName, cvarValue, sizeof( cvarValue ) );
					do {
						badchar = strchr( cvarValue, ';' );
						if ( badchar )
							*badchar = '.';
						else
						{
							badchar = strchr( cvarValue, '\n' );
							if ( badchar )
								*badchar = '.';
						}
					} while ( badchar );
					Q_strncpyz( textOut, cvarValue, sizeof(cmd.cmd) - ( textOut - cmd.cmd ) );
					while ( *textOut )
						textOut++;
					if ( textOut == sizeof(cmd.cmd) + cmd.cmd - 1 )
						break;
				}
				else
				{
					cvarName -= 2;
					while ( *cvarName && textOut < sizeof(cmd.cmd) + cmd.cmd - 1 )
						*textOut++ = *cvarName++;
					if ( textOut == sizeof(cmd.cmd) + cmd.cmd - 1 )
						break;
					*textOut++ = '\\';
				}
				text++;
			}
			else
			{
				cvarName -= 2;
				while ( *cvarName && textOut < sizeof(cmd.cmd) + cmd.cmd - 1 )
					*textOut++ = *cvarName++;
				if ( textOut == sizeof(cmd.cmd) + cmd.cmd - 1 )
					break;
			}
		}
		*textOut = 0;

		// "\$$" --> "\$"
		text = textOut = cmd.cmd;
		while (text[0])
		{
			if ( text[0] == '\\'  && text[1]  && text[1] == '$'&& text[2] && text[2] == '$' )
			{
				textOut[0] = '\\';
				textOut[1] = '$';
				textOut += 2;
				text += 3;
			}
			else
				*textOut++ = *text++;
		}
		*textOut = '\0';
	}
	else
		Q_strncpyz( cmd.cmd, text_in, sizeof(cmd.cmd) );

	text = cmd.cmd;
	textOut = cmd.tokenized;

	while ( 1 ) {
		if ( cmd.argc == MAX_STRING_TOKENS ) {
			return;			// this is usually something malicious
		}

		while ( 1 ) {
			// skip whitespace
			while ( *text > '\0' && *text <= ' ' ) {
				text++;
			}
			if ( !*text ) {
				return;			// all tokens parsed
			}

			// skip // comments
			if ( text[0] == '/' && text[1] == '/' ) {
				return;			// all tokens parsed
			}

			// skip /* */ comments
			if ( text[0] == '/' && text[1] =='*' ) {
				while ( *text && ( text[0] != '*' || text[1] != '/' ) ) {
					text++;
				}
				if ( !*text ) {
					return;		// all tokens parsed
				}
				text += 2;
			} else {
				break;			// we are ready to parse a token
			}
		}

		// handle quote escaping
		if ( !ignoreQuotes && text[0] == '\\' && text[1] == '"' ) {
			*textOut++ = '"';
			text += 2;
			continue;
		}

		// handle quoted strings
		if ( !ignoreQuotes && *text == '"' ) {
			cmd.argv[cmd.argc] = textOut;
			cmd.argc++;
			text++;
			while ( *text && *text != '"' ) {
				if ( text[0] == '\\' && text[1] == '"' ) {
					*textOut++ = '"';
					text += 2;
					continue;
				}
				*textOut++ = *text++;
			}
			*textOut++ = 0;
			if ( !*text ) {
				return;		// all tokens parsed
			}
			text++;
			continue;
		}

		// regular token
		cmd.argv[cmd.argc] = textOut;
		cmd.argc++;

		// skip until whitespace, quote, or command
		while ( *text > ' ' || *text < '\0' ) {
			if ( !ignoreQuotes && text[0] == '\\' && text[1] == '"' ) {
				*textOut++ = '"';
				text += 2;
				continue;
			}

			if ( !ignoreQuotes && text[0] == '"' ) {
				break;
			}

			if ( text[0] == '/' && text[1] == '/' ) {
				break;
			}

			// skip /* */ comments
			if ( text[0] == '/' && text[1] =='*' ) {
				break;
			}

			*textOut++ = *text++;
		}

		*textOut++ = 0;

		if ( !*text ) {
			return;		// all tokens parsed
		}
	}
	
}

/*
============
Cmd_TokenizeString
============
*/
void Cmd_TokenizeString( const char *text_in ) {
	Cmd_TokenizeString2( text_in, qfalse, qfalse );
}

/*
============
Cmd_TokenizeStringIgnoreQuotes
============
*/
void Cmd_TokenizeStringIgnoreQuotes( const char *text_in ) {
	Cmd_TokenizeString2( text_in, qtrue, qfalse );
}

/*
============
Cmd_TokenizeStringParseCvar
============
*/
void Cmd_TokenizeStringParseCvar( const char *text_in ) {
	Cmd_TokenizeString2( text_in, qfalse, qtrue );
}


/*
============
Cmd_AddCommand
============
*/
void Cmd_AddCommand(const char *cmd_name, xcommand_t function, const char *cmd_desc)
{
	cmd_function_t *cmd;

	// fail if the command is a variable name
	if( Cvar_FindVar( cmd_name ))
	{
		Com_Printf( "Cmd_AddCommand: %s already defined as a var\n", cmd_name );
		return;
	}
	
	// fail if the command already exists
	if( Cmd_Exists( cmd_name ))
	{
		Com_Printf( "Cmd_AddCommand: %s already defined\n", cmd_name );
		return;
	}

	// use a small malloc to avoid zone fragmentation
	cmd = (cmd_function_t*)S_Malloc(sizeof(cmd_function_t));
	cmd->name = CopyString(cmd_name);
	cmd->desc = CopyString(cmd_desc);
	cmd->function = function;
	cmd->next = cmd_functions;
	cmd->complete = NULL;
	cmd_functions = cmd;
}

/*
============
Cmd_SetCommandCompletionFunc
============
*/
void Cmd_SetCommandCompletionFunc( const char *command, completionFunc_t complete ) {
	cmd_function_t	*cmd;

	for( cmd = cmd_functions; cmd; cmd = cmd->next ) {
		if( !Q_stricmp( command, cmd->name ) ) {
			cmd->complete = complete;
		}
	}
}


/*
============
Cmd_RemoveCommand
============
*/
void Cmd_RemoveCommand(const char *cmd_name)
{
	cmd_function_t *cmd, **back;

	back = &cmd_functions;
	while(1)
	{
		cmd = *back;
		if(!cmd)
		{
			// command wasn't active
			return;
		}
		if(!strcmp(cmd_name, cmd->name))
		{
			*back = cmd->next;
			if(cmd->name)
			{
				Z_Free(cmd->name);
			}
			if(cmd->desc)
			{
				Z_Free(cmd->desc);
			}
			Z_Free(cmd);
			return;
		}
		back = &cmd->next;
	}
}


/*
============
Cmd_CommandCompletion
============
*/
void Cmd_CommandCompletion(void (*callback) (const char *s))
{
	cmd_function_t *cmd;

	for(cmd = cmd_functions; cmd; cmd = cmd->next)
	{
		callback(cmd->name);
	}
}

/*
============
Cmd_CompleteArgument
============
*/
void Cmd_CompleteArgument( const char *command, char *args, int argNum ) {
	cmd_function_t	*cmd;

	for( cmd = cmd_functions; cmd; cmd = cmd->next ) {
		if( !Q_stricmp( command, cmd->name ) && cmd->complete ) {
			cmd->complete( args, argNum );
		}
	}
}


/*
============
Cmd_ExecuteString

A complete command line has been parsed, so try to execute it
============
*/
void Cmd_ExecuteString(const char *text)
{
	cmd_function_t *cmdFunc, **prev;

	// execute the command line
	Cmd_TokenizeStringParseCvar( text );
	if(!Cmd_Argc())
	{
		return;					// no tokens
	}

	// check registered command functions
	for(prev = &cmd_functions; *prev; prev = &cmdFunc->next)
	{
		cmdFunc = *prev;
		if(!Q_stricmp(cmd.argv[0], cmdFunc->name))
		{
			// rearrange the links so that the command will be
			// near the head of the list next time it is used
			*prev = cmdFunc->next;
			cmdFunc->next = cmd_functions;
			cmd_functions = cmdFunc;

			// perform the action
			if(!cmdFunc->function)
			{
				// let the cgame or game handle it
				break;
			}
			else
			{
				cmdFunc->function();
			}
			return;
		}
	}

	// check cvars
	if(Cvar_Command())
	{
		return;
	}

	// check client game commands
	if(com_cl_running && com_cl_running->integer && CL_GameCommand())
	{
		return;
	}

	// check server game commands
	if(com_sv_running && com_sv_running->integer && SV_GameCommand())
	{
		return;
	}

	// check ui commands
	if(com_cl_running && com_cl_running->integer && UI_GameCommand())
	{
		return;
	}

	// send it as a server command if we are connected
	// this will usually result in a chat message
	CL_ForwardCommandToServer(text);
}

/*
============
Cmd_List_f
============
*/
void Cmd_List_f(void)
{
	cmd_function_t *cmd;
	int             i = 0;
	char           *match;

	if(Cmd_Argc() > 1)
	{
		match = Cmd_Argv(1);
	}
	else
	{
		match = NULL;
	}

	for(cmd = cmd_functions; cmd; cmd = cmd->next)
	{
		if(match && !Com_Filter(match, cmd->name, qfalse))
		{
			continue;
		}

		Com_Printf("    %-32s ^7\"%s^7\"\n", cmd->name, cmd->desc);
		i++;
	}
	Com_Printf("%i commands\n", i);
}

/*
==================
Cmd_CompleteCfgName
==================
*/
void Cmd_CompleteCfgName( char *args, int argNum ) {
	if( argNum == 2 ) {
		Field_CompleteFilename( "", "cfg", qfalse );
	}
}

/*
==================
Cmd_CompleteAliasName
==================
*/
void Cmd_CompleteAliasName( char *args, int argNum ) {
	if( argNum == 2 ) {
		Field_CompleteAlias( );
	}
}

/*
==================
Cmd_CompleteConcat
==================
*/
void Cmd_CompleteConcat( char *args, int argNum )
{
	// Skip
	char *p = Com_SkipTokens( args, argNum - 1, " " );

	if( p > args )
		Field_CompleteCommand( p, qfalse, qtrue );
}

/*
==================
Cmd_CompleteIf
==================
*/
void Cmd_CompleteIf( char *args, int argNum )
{
	if( argNum == 5 || argNum == 6 )
	{
		// Skip
		char *p = Com_SkipTokens( args, argNum - 1, " " );

		if( p > args )
			Field_CompleteCommand( p, qfalse, qtrue );
	}
}

/*
==================
Cmd_CompleteDelay
==================
*/
void Cmd_CompleteDelay( char *args, int argNum )
{
	if( argNum == 3 || argNum == 4 )
	{
		// Skip "delay "
		char *p = Com_SkipTokens( args, 1, " " );

		if( p > args )
			Field_CompleteCommand( p, qtrue, qtrue );
	}
}

/*
==================
Cmd_CompleteUnDelay
==================
*/
void Cmd_CompleteUnDelay( char *args, int argNum ) {
	if( argNum == 2 ) {
		Field_CompleteDelay( );
	}
}


/*
============
Cmd_Init
============
*/
void Cmd_Init(void)
{
	Cmd_AddCommand("cmdlist", Cmd_List_f, "^1Display all console commands beginning with the specified prefix.");
	Cmd_AddCommand("exec", Cmd_Exec_f, "^1Execute a script file.");
	Cmd_SetCommandCompletionFunc( "exec", Cmd_CompleteCfgName );
	Cmd_AddCommand("wait", Cmd_Wait_f,"^1Make script execution wait for some rendered frames.");
	Cmd_SetCommandCompletionFunc( "math", Cvar_CompleteCvarName );
	Cmd_AddCommand ("alias", Cmd_Alias_f, "^1Create a script function. Without arguments show the list of all alias.");
	Cmd_SetCommandCompletionFunc( "alias", Cmd_CompleteAliasName );
	Cmd_AddCommand ("unalias", Cmd_UnAlias_f, "^1Removes the alias.");
	Cmd_SetCommandCompletionFunc( "unalias", Cmd_CompleteAliasName );
	Cmd_AddCommand ("aliaslist", Cmd_AliasList_f, "^1Lists all existing aliases.");
	Cmd_AddCommand ("clearaliases", Cmd_ClearAliases_f, "^1Removes all existing aliases.");
}


/*
============
Cmd_Exists
============
*/
qboolean Cmd_Exists( const char *cmd_name )
{
	cmd_function_t	*cmd;

	for( cmd = cmd_functions; cmd; cmd = cmd->next )
	{
		if( !idStr::Cmp( cmd_name, cmd->name ))
			return qtrue;
	}
	return qfalse;
}