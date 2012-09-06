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

// q_shared.c -- stateless support routines that are included in each code dll
#include "q_shared.h"

// os x game bundles have no standard library links, and the defines are not always defined!

#ifdef MACOS_X
int qmax( int x, int y ) {
	return ( ( ( x ) > ( y ) ) ? ( x ) : ( y ) );
}

int qmin( int x, int y ) {
	return ( ( ( x ) < ( y ) ) ? ( x ) : ( y ) );
}
#endif

/*
============================================================================

GROWLISTS

============================================================================
*/

// malloc / free all in one place for debugging
//extern          "C" void *Com_Allocate(int bytes);
//extern          "C" void Com_Dealloc(void *ptr);

void Com_InitGrowList(growList_t * list, int maxElements)
{
	list->maxElements = maxElements;
	list->currentElements = 0;
	list->elements = (void **)Com_Allocate(list->maxElements * sizeof(void *));
}

void Com_DestroyGrowList(growList_t * list)
{
	Com_Dealloc(list->elements);
	memset(list, 0, sizeof(*list));
}

int Com_AddToGrowList(growList_t * list, void *data)
{
	void          **old;

	if(list->currentElements != list->maxElements)
	{
		list->elements[list->currentElements] = data;
		return list->currentElements++;
	}

	// grow, reallocate and move
	old = list->elements;

	if(list->maxElements < 0)
	{
		Com_Error(ERR_FATAL, "Com_AddToGrowList: maxElements = %i", list->maxElements);
	}

	if(list->maxElements == 0)
	{
		// initialize the list to hold 100 elements
		Com_InitGrowList(list, 100);
		return Com_AddToGrowList(list, data);
	}

	list->maxElements *= 2;

//  Com_DPrintf("Resizing growlist to %i maxElements\n", list->maxElements);

	list->elements = (void **)Com_Allocate(list->maxElements * sizeof(void *));

	if(!list->elements)
	{
		Com_Error(ERR_DROP, "Growlist alloc failed");
	}

	Com_Memcpy(list->elements, old, list->currentElements * sizeof(void *));

	Com_Dealloc(old);

	return Com_AddToGrowList(list, data);
}

void           *Com_GrowListElement(const growList_t * list, int index)
{
	if(index < 0 || index >= list->currentElements)
	{
		Com_Error(ERR_DROP, "Com_GrowListElement: %i out of range of %i", index, list->currentElements);
	}
	return list->elements[index];
}

int Com_IndexForGrowListElement(const growList_t * list, const void *element)
{
	int             i;

	for(i = 0; i < list->currentElements; i++)
	{
		if(list->elements[i] == element)
		{
			return i;
		}
	}
	return -1;
}

//=============================================================================

memStream_t *AllocMemStream(byte *buffer, int bufSize)
{
	memStream_t		*s;

	if(buffer == NULL || bufSize <= 0)
		return NULL;

	s = (memStream_t*)Com_Allocate(sizeof(memStream_t));
	if(s == NULL)
		return NULL;

	Com_Memset(s, 0, sizeof(memStream_t));

	s->buffer 	= buffer;
	s->curPos 	= buffer;
	s->bufSize	= bufSize;
	s->flags	= 0;

	return s;
}

void FreeMemStream(memStream_t * s)
{
	Com_Dealloc(s);
}

int MemStreamRead(memStream_t *s, void *buffer, int len)
{
	int				ret = 1;

	if(s == NULL || buffer == NULL)
		return 0;

	if(s->curPos + len > s->buffer + s->bufSize)
	{
		s->flags |= MEMSTREAM_FLAGS_EOF;
		len = s->buffer + s->bufSize - s->curPos;
		ret = 0;

		Com_Error(ERR_FATAL, "MemStreamRead: EOF reached");
	}

	Com_Memcpy(buffer, s->curPos, len);
	s->curPos += len;

	return ret;
}

int MemStreamGetC(memStream_t *s)
{
	int				c = 0;

	if(s == NULL)
		return -1;

	if(MemStreamRead(s, &c, 1) == 0)
		return -1;

	return c;
}

int MemStreamGetLong(memStream_t * s)
{
	int				c = 0;

	if(s == NULL)
		return -1;

	if(MemStreamRead(s, &c, 4) == 0)
		return -1;

	return c;// LongSwap(c);
}

int MemStreamGetShort(memStream_t * s)
{
	int				c = 0;

	if(s == NULL)
		return -1;

	if(MemStreamRead(s, &c, 2) == 0)
		return -1;

	return c;// ShortSwap(c);
}

float MemStreamGetFloat(memStream_t * s)
{
	floatint_t		c;

	if(s == NULL)
		return -1;

	if(MemStreamRead(s, &c.i, 4) == 0)
		return -1;

	return c.f;// FloatSwap(c.f);
}

//=============================================================================

float Com_Clamp( float min, float max, float value ) {
	if ( value < min ) {
		return min;
	}
	if ( value > max ) {
		return max;
	}
	return value;
}


/*
COM_FixPath()
unixifies a pathname
*/

void COM_FixPath( char *pathname ) {
	while ( *pathname )
	{
		if ( *pathname == '\\' ) {
			*pathname = '/';
		}
		pathname++;
	}
}



/*
============
COM_SkipPath
============
*/
char *COM_SkipPath( char *pathname ) {
	char    *last;

	last = pathname;
	while ( *pathname )
	{
		if ( *pathname == '/' ) {
			last = pathname + 1;
		}
		pathname++;
	}
	return last;
}

/*
==================
Com_CharIsOneOfCharset
==================
*/
static qboolean Com_CharIsOneOfCharset( char c, char *set )
{
	int i;

	for( i = 0; i < strlen( set ); i++ )
	{
		if( set[ i ] == c )
			return qtrue;
	}

	return qfalse;
}

/*
==================
Com_SkipCharset
==================
*/
char *Com_SkipCharset( char *s, char *sep )
{
	char	*p = s;

	while( p )
	{
		if( Com_CharIsOneOfCharset( *p, sep ) )
			p++;
		else
			break;
	}

	return p;
}


/*
==================
Com_SkipTokens
==================
*/
char *Com_SkipTokens( char *s, int numTokens, char *sep )
{
	int		sepCount = 0;
	char	*p = s;

	while( sepCount < numTokens )
	{
		if( Com_CharIsOneOfCharset( *p++, sep ) )
		{
			sepCount++;
			while( Com_CharIsOneOfCharset( *p, sep ) )
				p++;
		}
		else if( *p == '\0' )
			break;
	}

	if( sepCount == numTokens )
		return p;
	else
		return s;
}


/*
============
COM_GetExtension
============
*/
const char     *COM_GetExtension(const char *name)
{
    int             length, i;

    length = strlen(name) - 1;
    i = length;

    while(name[i] != '.')
    {
        i--;
        if(name[i] == '/' || i == 0)
                return "";                      // no extension
    }

    return &name[i + 1];
}

/*
============
COM_StripExtension
============
*/
void COM_StripExtension( const char *in, char *out ) {
	while ( *in && *in != '.' ) {
		*out++ = *in++;
	}
	*out = 0;
}

/*
============
COM_StripExtension2
a safer version
============
*/
void COM_StripExtension2( const char *in, char *out, int destsize ) {
	int len = 0;
	while ( len < destsize - 1 && *in && *in != '.' ) {
		*out++ = *in++;
		len++;
	}
	*out = 0;
}

void COM_StripFilename( char *in, char *out ) {
	char *end;
	Q_strncpyz( out, in, strlen( in ) + 1 );
	end = COM_SkipPath( out );
	*end = 0;
}


/*
============
COM_StripExtension3

RB: ioquake3 version
============
*/
void COM_StripExtension3(const char *src, char *dest, int destsize)
{
	int             length;

	Q_strncpyz(dest, src, destsize);

	length = strlen(dest) - 1;

	while(length > 0 && dest[length] != '.')
	{
		length--;

		if(dest[length] == '/')
			return;				// no extension
	}

	if(length)
	{
		dest[length] = 0;
	}
}


/*
==================
COM_DefaultExtension
==================
*/
void COM_DefaultExtension( char *path, int maxSize, const char *extension ) {
	char oldPath[MAX_QPATH];
	char    *src;

//
// if path doesn't have a .EXT, append extension
// (extension should include the .)
//
	src = path + strlen( path ) - 1;

	while ( *src != '/' && src != path ) {
		if ( *src == '.' ) {
			return;                 // it has an extension
		}
		src--;
	}

	Q_strncpyz( oldPath, path, sizeof( oldPath ) );
	Com_sprintf( path, maxSize, "%s%s", oldPath, extension );
}

//============================================================================

/*
============
Com_HashKey
============
*/
int Com_HashKey(char *string, int maxlen)
{
       register int    hash, i;

       hash = 0;
       for(i = 0; i < maxlen && string[i] != '\0'; i++)
       {
               hash += string[i] * (119 + i);
       }
       hash = (hash ^ (hash >> 10) ^ (hash >> 20));
       return hash;
}

//============================================================================


/*
==================
COM_BitCheck

  Allows bit-wise checks on arrays with more than one item (> 32 bits)
==================
*/
qboolean COM_BitCheck( const int array[], int bitNum ) {
	int i;

	i = 0;
	while ( bitNum > 31 ) {
		i++;
		bitNum -= 32;
	}

	return (qboolean)( ( array[i] & ( 1 << bitNum ) ) != 0 );  // (SA) heh, whoops. :)
}

/*
==================
COM_BitSet

  Allows bit-wise SETS on arrays with more than one item (> 32 bits)
==================
*/
void COM_BitSet( int array[], int bitNum ) {
	int i;

	i = 0;
	while ( bitNum > 31 ) {
		i++;
		bitNum -= 32;
	}

	array[i] |= ( 1 << bitNum );
}

/*
==================
COM_BitClear

  Allows bit-wise CLEAR on arrays with more than one item (> 32 bits)
==================
*/
void COM_BitClear( int array[], int bitNum ) {
	int i;

	i = 0;
	while ( bitNum > 31 ) {
		i++;
		bitNum -= 32;
	}

	array[i] &= ~( 1 << bitNum );
}

/*
============================================================================

PARSING

============================================================================
*/

// multiple character punctuation tokens
const char     *punctuation[] = {
	"+=", "-=", "*=", "/=", "&=", "|=", "++", "--",
	"&&", "||", "<=", ">=", "==", "!=",
	NULL
};

static char com_token[MAX_TOKEN_CHARS];
static char com_parsename[MAX_TOKEN_CHARS];
static int com_lines;

static int backup_lines;
static char    *backup_text;

void COM_BeginParseSession( const char *name ) {
	com_lines = 0;
	Com_sprintf( com_parsename, sizeof( com_parsename ), "%s", name );
}

void COM_BackupParseSession( char **data_p ) {
	backup_lines = com_lines;
	backup_text = *data_p;
}

void COM_RestoreParseSession( char **data_p ) {
	com_lines = backup_lines;
	*data_p = backup_text;
}

void COM_SetCurrentParseLine( int line ) {
	com_lines = line;
}

int COM_GetCurrentParseLine( void ) {
	return com_lines;
}

char *COM_Parse( char **data_p ) {
	return COM_ParseExt( data_p, qtrue );
}

void COM_ParseError( char *format, ... ) {
	va_list argptr;
	static char string[4096];

	va_start( argptr, format );
	vsnprintf( string, sizeof( string ), format, argptr );
	va_end( argptr );

	Com_Printf(S_COLOR_RED "ERROR: %s, line %d: %s\n", com_parsename, com_lines, string);
}

void COM_ParseWarning( char *format, ... ) {
	va_list argptr;
	static char string[4096];

	va_start( argptr, format );
	vsnprintf( string, sizeof( string ), format, argptr );
	va_end( argptr );

	Com_Printf( "WARNING: %s, line %d: %s\n", com_parsename, com_lines, string );
}



/*
==============
COM_Parse

Parse a token out of a string
Will never return NULL, just empty strings

If "allowLineBreaks" is qtrue then an empty
string will be returned if the next token is
a newline.
==============
*/
static char *SkipWhitespace( char *data, qboolean *hasNewLines ) {
	int c;

	while ( ( c = *data ) <= ' ' ) {
		if ( !c ) {
			return NULL;
		}
		if ( c == '\n' ) {
			com_lines++;
			*hasNewLines = qtrue;
		}
		data++;
	}

	return data;
}

int COM_Compress( char *data_p ) {
	char *datai, *datao;
	int c, size;
	qboolean ws = qfalse;

	size = 0;
	datai = datao = data_p;
	if ( datai ) {
		while ( ( c = *datai ) != 0 ) {
			if ( c == 13 || c == 10 ) {
				*datao = c;
				datao++;
				ws = qfalse;
				datai++;
				size++;
				// skip double slash comments
			} else if ( c == '/' && datai[1] == '/' ) {
				while ( *datai && *datai != '\n' ) {
					datai++;
				}
				ws = qfalse;
				// skip /* */ comments
			} else if ( c == '/' && datai[1] == '*' ) {
				datai += 2; // Arnout: skip over '/*'
				while ( *datai && ( *datai != '*' || datai[1] != '/' ) )
				{
					datai++;
				}
				if ( *datai ) {
					datai += 2;
				}
				ws = qfalse;
			} else {
				if ( ws ) {
					*datao = ' ';
					datao++;
				}
				*datao = c;
				datao++;
				datai++;
				ws = qfalse;
				size++;
			}
		}
	}
	*datao = 0;
	return size;
}

char *COM_ParseExt( char **data_p, qboolean allowLineBreaks ) {
	int c = 0, len;
	qboolean hasNewLines = qfalse;
	char *data;

	data = *data_p;
	len = 0;
	com_token[0] = 0;

	// make sure incoming data is valid
	if ( !data ) {
		*data_p = NULL;
		return com_token;
	}

	// RF, backup the session data so we can unget easily
	COM_BackupParseSession( data_p );

	while ( 1 )
	{
		// skip whitespace
		data = SkipWhitespace( data, &hasNewLines );
		if ( !data ) {
			*data_p = NULL;
			return com_token;
		}
		if ( hasNewLines && !allowLineBreaks ) {
			*data_p = data;
			return com_token;
		}

		c = *data;

		// skip double slash comments
		if ( c == '/' && data[1] == '/' ) {
			data += 2;
			while ( *data && *data != '\n' ) {
				data++;
			}
//			com_lines++;
		}
		// skip /* */ comments
		else if ( c == '/' && data[1] == '*' ) {
			data += 2;
			while ( *data && ( *data != '*' || data[1] != '/' ) )
			{
				data++;
				if ( *data == '\n' ) {
//					com_lines++;
				}
			}
			if ( *data ) {
				data += 2;
			}
		} else
		{
			break;
		}
	}

	// handle quoted strings
	if ( c == '\"' ) {
		data++;
		while ( 1 )
		{
			c = *data++;
			if ( c == '\\' && *( data ) == '\"' ) {
				// Arnout: string-in-string
				if ( len < MAX_TOKEN_CHARS ) {
					com_token[len] = '\"';
					len++;
				}
				data++;

				while ( 1 ) {
					c = *data++;

					if ( !c ) {
						com_token[len] = 0;
						*data_p = ( char * ) data;
						break;
					}
					if ( ( c == '\\' && *( data ) == '\"' ) ) {
						if ( len < MAX_TOKEN_CHARS ) {
							com_token[len] = '\"';
							len++;
						}
						data++;
						c = *data++;
						break;
					}
					if ( len < MAX_TOKEN_CHARS ) {
						com_token[len] = c;
						len++;
					}
				}
			}
			if ( c == '\"' || !c ) {
				com_token[len] = 0;
				*data_p = ( char * ) data;
				return com_token;
			}
			if ( len < MAX_TOKEN_CHARS ) {
				com_token[len] = c;
				len++;
			}
		}
	}

	// parse a regular word
	do
	{
		if ( len < MAX_TOKEN_CHARS ) {
			com_token[len] = c;
			len++;
		}
		data++;
		c = *data;
		if ( c == '\n' ) {
			com_lines++;
		}
	} while ( c > 32 );

	if ( len == MAX_TOKEN_CHARS ) {
//		Com_Printf ("Token exceeded %i chars, discarded.\n", MAX_TOKEN_CHARS);
		len = 0;
	}
	com_token[len] = 0;

	*data_p = ( char * ) data;
	return com_token;
}


char           *COM_Parse2(char **data_p)
{
	return COM_ParseExt2(data_p, qtrue);
}


// *INDENT-OFF*
char           *COM_ParseExt2(char **data_p, qboolean allowLineBreaks)
{
	int             c = 0, len;
	qboolean        hasNewLines = qfalse;
	char           *data;
	const char    **punc;

	if(!data_p)
	{
		Com_Error(ERR_FATAL, "COM_ParseExt: NULL data_p");
	}

	data = *data_p;
	len = 0;
	com_token[0] = 0;

	// make sure incoming data is valid
	if(!data)
	{
		*data_p = NULL;
		return com_token;
	}

	// RF, backup the session data so we can unget easily
	COM_BackupParseSession(data_p);

	// skip whitespace
	while(1)
	{
		data = SkipWhitespace(data, &hasNewLines);
		if(!data)
		{
			*data_p = NULL;
			return com_token;
		}
		if(hasNewLines && !allowLineBreaks)
		{
			*data_p = data;
			return com_token;
		}

		c = *data;

		// skip double slash comments
		if(c == '/' && data[1] == '/')
		{
			data += 2;
			while(*data && *data != '\n')
			{
				data++;
			}
		}
		// skip /* */ comments
		else if(c == '/' && data[1] == '*')
		{
			data += 2;
			while(*data && (*data != '*' || data[1] != '/'))
			{
				data++;
			}
			if(*data)
			{
				data += 2;
			}
		}
		else
		{
			// a real token to parse
			break;
		}
	}

	// handle quoted strings
	if(c == '\"')
	{
		data++;
		while(1)
		{
			c = *data++;

			if((c == '\\') && (*data == '\"'))
			{
				// allow quoted strings to use \" to indicate the " character
				data++;
			}
			else if(c == '\"' || !c)
			{
				com_token[len] = 0;
				*data_p = (char *)data;
				return com_token;
			}
			else if(*data == '\n')
			{
				com_lines++;
			}

			if(len < MAX_TOKEN_CHARS - 1)
			{
				com_token[len] = c;
				len++;
			}
		}
	}

	// check for a number
	// is this parsing of negative numbers going to cause expression problems
	if(	(c >= '0' && c <= '9') ||
		(c == '-' && data[1] >= '0' && data[1] <= '9') ||
		(c == '.' && data[1] >= '0' && data[1] <= '9') ||
		(c == '-' && data[1] == '.' && data[2] >= '0' && data[2] <= '9'))
	{
		do
		{
			if(len < MAX_TOKEN_CHARS - 1)
			{
				com_token[len] = c;
				len++;
			}
			data++;

			c = *data;
		} while((c >= '0' && c <= '9') || c == '.');

		// parse the exponent
		if(c == 'e' || c == 'E')
		{
			if(len < MAX_TOKEN_CHARS - 1)
			{
				com_token[len] = c;
				len++;
			}
			data++;
			c = *data;

			if(c == '-' || c == '+')
			{
				if(len < MAX_TOKEN_CHARS - 1)
				{
					com_token[len] = c;
					len++;
				}
				data++;
				c = *data;
			}

			do
			{
				if(len < MAX_TOKEN_CHARS - 1)
				{
					com_token[len] = c;
					len++;
				}
				data++;

				c = *data;
			} while(c >= '0' && c <= '9');
		}

		if(len == MAX_TOKEN_CHARS)
		{
			len = 0;
		}
		com_token[len] = 0;

		*data_p = (char *)data;
		return com_token;
	}

	// check for a regular word
	// we still allow forward and back slashes in name tokens for pathnames
	// and also colons for drive letters
	if(	(c >= 'a' && c <= 'z') ||
		(c >= 'A' && c <= 'Z') ||
		(c == '_') ||
		(c == '/') ||
		(c == '\\') ||
		(c == '$') || (c == '*')) // Tr3B - for bad shader strings
	{
		do
		{
			if(len < MAX_TOKEN_CHARS - 1)
			{
				com_token[len] = c;
				len++;
			}
			data++;

			c = *data;
		}
		while
			((c >= 'a' && c <= 'z') ||
			 (c >= 'A' && c <= 'Z') ||
			 (c == '_') ||
			 (c == '-') ||
			 (c >= '0' && c <= '9') ||
			 (c == '/') ||
			 (c == '\\') ||
			 (c == ':') ||
			 (c == '.') ||
			 (c == '$') ||
			 (c == '*') ||
			 (c == '@'));

		if(len == MAX_TOKEN_CHARS)
		{
			len = 0;
		}
		com_token[len] = 0;

		*data_p = (char *)data;
		return com_token;
	}

	// check for multi-character punctuation token
	for(punc = punctuation; *punc; punc++)
	{
		int             l;
		int             j;

		l = strlen(*punc);
		for(j = 0; j < l; j++)
		{
			if(data[j] != (*punc)[j])
			{
				break;
			}
		}
		if(j == l)
		{
			// a valid multi-character punctuation
			Com_Memcpy(com_token, *punc, l);
			com_token[l] = 0;
			data += l;
			*data_p = (char *)data;
			return com_token;
		}
	}

	// single character punctuation
	com_token[0] = *data;
	com_token[1] = 0;
	data++;
	*data_p = (char *)data;

	return com_token;
}
// *INDENT-ON*

/*
=================
SkipBracedSection_Depth

=================
*/
qboolean SkipBracedSection_Depth( char **program, int depth ) {
	char *token;

	do {
		token = COM_ParseExt( program, qtrue );
		if ( token[1] == 0 ) {
			if ( token[0] == '{' ) {
				depth++;
			} else if ( token[0] == '}' )     {
				depth--;
			}
		}
	} while ( depth && *program );

	 return (qboolean) (depth == 0);
}

/*
=================
SkipBracedSection

The next token should be an open brace.
Skips until a matching close brace is found.
Internal brace depths are properly skipped.
=================
*/
qboolean SkipBracedSection( char **program ) {
	char            *token;
	int depth;

	depth = 0;
	do {
		token = COM_ParseExt( program, qtrue );
		if ( token[1] == 0 ) {
			if ( token[0] == '{' ) {
				depth++;
			} else if ( token[0] == '}' )     {
				depth--;
			}
		}
	} while ( depth && *program );

	return (qboolean) (depth == 0);
}

/*
=================
SkipRestOfLine
=================
*/
void SkipRestOfLine( char **data ) {
	char    *p;
	int c;

	p = *data;
	while ( ( c = *p++ ) != 0 ) {
		if ( c == '\n' ) {
			com_lines++;
			break;
		}
	}

	*data = p;
}

/*
===================
Com_HexStrToInt
===================
*/
int Com_HexStrToInt( const char *str )
{
	if ( !str || !str[ 0 ] )
		return -1;

	// check for hex code
	if( str[ 0 ] == '0' && str[ 1 ] == 'x' )
	{
		int i, n = 0;

		for( i = 2; i < strlen( str ); i++ )
		{
			char digit;

			n *= 16;

			digit = tolower( str[ i ] );

			if( digit >= '0' && digit <= '9' )
				digit -= '0';
			else if( digit >= 'a' && digit <= 'f' )
				digit = digit - 'a' + 10;
			else
				return -1;

			n += digit;
		}

		return n;
	}

	return -1;
}

/*
===================
Com_QuoteStr
===================
*/
const char *Com_QuoteStr (const char *str)
{
	static char *buf = NULL;
	static size_t buflen = 0;

	size_t length;
	char *ptr;

	// quick exit if no quoting is needed
//	if (!strpbrk (str, "\";"))
//		return str;

	length = strlen (str);
	if (buflen < 2 * length + 3)
	{
		free (buf);
		buflen = 2 * length + 3;
		buf = (char*)malloc (buflen);
	}
	ptr = buf;
	*ptr++ = '"';
	--str;
	while (*++str)
	{
		if (*str == '"')
			*ptr++ = '\\';
		*ptr++ = *str;
	}
	ptr[0] = '"';
	ptr[1] = 0;

	return buf;
}

/*
===================
Com_UnquoteStr
===================
*/
const char *Com_UnquoteStr (const char *str)
{
	static char *buf = NULL;

	size_t length;
	char *ptr;
	const char *end;

	end = str + strlen (str);

	// Strip trailing spaces
	while (--end >= str)
		if (*end != ' ')
			break;
	// end points at the last non-space character

	// If it doesn't begin with '"', return quickly
	if (*str != '"')
	{
		length = end + 1 - str;
		free (buf);
		buf = (char*)malloc (length + 1);
		strncpy (buf, str, length);
		buf[length] = 0;
		return buf;
	}

	// It begins with '"'; if it ends with '"', lose that '"'
	if (end > str && *end == '"')
		--end;

	free (buf);
	buf = (char*)malloc (end + 1 - str);
	ptr = buf;

	// Copy, unquoting as we go
	// str[0] == '"', so that gets skipped
	while (++str <= end)
	{
		if (str[0] == '\\' && str[1] == '"' && str < end) // FIXME: \ semantics are broken
			++str;
		*ptr++ = *str;
	}
	*ptr = 0;

	return buf;
}


/*
============================================================================

					LIBRARY REPLACEMENT FUNCTIONS

============================================================================
*/

/*
=============
Q_strtoi/l

Takes a null-terminated string (which represents either a float or integer
conforming to strtod) and an integer to assign to (if successful).

Returns true on success and vice versa.
Demonstration of behavior of strtod and conversions: http://codepad.org/YQKxV94R
-============
*/
qboolean Q_strtoi(const char* s, int * outNum) {
	char *p;
	if ( *s== '\0' ) {
		return qfalse;
	}
	
	*outNum = strtod( s, &p );
	
	return (qboolean) (*p == '\0');
}

char * Q_strcpy_ringbuffer( char * buffer, int size, char * first, char * last, const char * s ) {

	int i;

	if ( !first ) {
		first = buffer + size;
	} else {
		while ( *first ) first++;
		first++;
	}

	if ( !last ) {
		last = buffer;
	} else {
		while ( *last ) last++;
		last++;
	}

	if ( last == first ) {
		first = buffer + ((last-buffer)-1)%size;
	}

	for ( i=0; ; i++ ) {

		if ( last + i >= buffer + size ) {
			last = buffer;
			i = 0;
		}

		if ( last + i == first ) {
			return 0;
		}

		last[ i ] = s[ i ];

		if ( s[ i ] == '\0' )
			break;
	}

	return last;
}

/*
=============
Q_strncpyz

Safe strncpy that ensures a trailing zero
=============
*/

// Dushan
#if defined(_DEBUG)
void Q_strncpyzDebug (char *dest, const char *src, size_t destsize, const char *file, int line)
#else
void Q_strncpyz( char *dest, const char *src, int destsize )
#endif
{
#ifdef _DEBUG
	if (!dest) {
		Com_Error(ERR_DROP, "Q_strncpyz: NULL dest (%s, %i)", file, line);
	}
	if (!src) {
		Com_Error(ERR_DROP, "Q_strncpyz: NULL src (%s, %i)", file, line);
	}
	if (destsize < 1) {
		Com_Error(ERR_DROP, "Q_strncpyz: destsize < 1 (%s, %i)", file, line);
	}
#else

	if ( !dest ) {
		Com_Error( ERR_FATAL, "Q_strncpyz: NULL dest" );
	}

	if ( !src ) {
		Com_Error( ERR_FATAL, "Q_strncpyz: NULL src" );
	}
	if ( destsize < 1 ) {
		Com_Error( ERR_FATAL,"Q_strncpyz: destsize < 1" );
	}
#endif

	strncpy( dest, src, destsize - 1 );
	dest[destsize - 1] = 0;
}

int Q_stricmpn( const char *s1, const char *s2, int n ) {
	int c1, c2;

	do {
		c1 = *s1++;
		c2 = *s2++;

		if ( !n-- ) {
			return 0;       // strings are equal until end point
		}

		if ( c1 != c2 ) {
			if ( c1 >= 'a' && c1 <= 'z' ) {
				c1 -= ( 'a' - 'A' );
			}
			if ( c2 >= 'a' && c2 <= 'z' ) {
				c2 -= ( 'a' - 'A' );
			}
			if ( c1 != c2 ) {
				return c1 < c2 ? -1 : 1;
			}
		}
	} while ( c1 );

	return 0;       // strings are equal
}

int Q_strncmp( const char *s1, const char *s2, int n ) {
	int c1, c2;

	do {
		c1 = *s1++;
		c2 = *s2++;

		if ( !n-- ) {
			return 0;       // strings are equal until end point
		}

		if ( c1 != c2 ) {
			return c1 < c2 ? -1 : 1;
		}
	} while ( c1 );

	return 0;       // strings are equal
}

int Q_stricmp( const char *s1, const char *s2 ) {
	return ( s1 && s2 ) ? Q_stricmpn( s1, s2, 99999 ) : -1;
}

char *Q_strlwr( char *s1 ) {
	char*   s;

	for ( s = s1; *s; ++s ) {
		if ( ( 'A' <= *s ) && ( *s <= 'Z' ) ) {
			*s -= 'A' - 'a';
		}
	}

	return s1;
}

char *Q_strupr( char *s1 ) {
	char* cp;

	for ( cp = s1 ; *cp ; ++cp ) {
		if ( ( 'a' <= *cp ) && ( *cp <= 'z' ) ) {
			*cp += 'A' - 'a';
		}
	}

	return s1;
}


// never goes past bounds or leaves without a terminating 0
void Q_strcat( char *dest, int size, const char *src ) {
	int l1;

	l1 = strlen( dest );
	if ( l1 >= size ) {
		Com_Error( ERR_FATAL, "Q_strcat: already overflowed" );
	}
	Q_strncpyz( dest + l1, src, size - l1 );
}


int Q_strnicmp (const char *string1, const char *string2, int n) {
	int c1, c2;

	if (string1 == NULL) {
		if (string2 == NULL)
			return 0;
		else
			return -1;
	}
	else if (string2 == NULL)
		return 1;

	do {
		c1 = *string1++;
		c2 = *string2++;

		if (!n--)
			return 0;// Strings are equal until end point

		if (c1 != c2) {
			if (c1 >= 'a' && c1 <= 'z')
				c1 -= ('a' - 'A');
			if (c2 >= 'a' && c2 <= 'z')
				c2 -= ('a' - 'A');

			if (c1 != c2)
				return c1 < c2 ? -1 : 1;
		}
	} while (c1);

	return 0;// Strings are equal
}

void Q_strncpyz2 (char *dst, const char *src, int dstSize)
{
	if (!dst)
		Com_Error(ERR_FATAL, "Q_strncpyz: NULL dst");

	if (!src)
		Com_Error(ERR_FATAL, "Q_strncpyz: NULL src");

	if (dstSize < 1)
		Com_Error(ERR_FATAL, "Q_strncpyz: dstSize < 1");

	strncpy(dst, src, dstSize-1);
	dst[dstSize-1] = 0;
}

int Q_strncasecmp (const char *s1, const char *s2, int n)
{
	int		c1, c2;

	do
	{
		c1 = *s1++;
		c2 = *s2++;

		if (!n--)
			return 0;		// strings are equal until end point

		if (c1 != c2)
		{
			if (c1 >= 'a' && c1 <= 'z')
				c1 -= ('a' - 'A');
			if (c2 >= 'a' && c2 <= 'z')
				c2 -= ('a' - 'A');
			if (c1 != c2)
				return -1;		// strings not equal
		}
	} while (c1);

	return 0;		// strings are equal
}

int Q_strcasecmp (const char *s1, const char *s2)
{
	return Q_strncasecmp (s1, s2, 99999);
}

/*
* Find the first occurrence of find in s.
*/
const char *Q_stristr( const char *s, const char *find)
{
  char c, sc;
  size_t len;

  if ((c = *find++) != 0)
  {
    if (c >= 'a' && c <= 'z')
    {
      c -= ('a' - 'A');
    }
    len = strlen(find);
    do
    {
      do
      {
        if ((sc = *s++) == 0)
          return NULL;
        if (sc >= 'a' && sc <= 'z')
        {
          sc -= ('a' - 'A');
        }
      } while (sc != c);
    } while (Q_stricmpn(s, find, len) != 0);
    s--;
  }
  return s;
}


/*
=============
Q_strreplace

replaces content of find by replace in dest
=============
*/
qboolean Q_strreplace(char *dest, int destsize, const char *find, const char *replace)
{
	int             lstart, lfind, lreplace, lend;
	char           *s;
	char            backup[32000];	// big, but small enough to fit in PPC stack

	lend = strlen(dest);
	if(lend >= destsize)
	{
		Com_Error(ERR_FATAL, "Q_strreplace: already overflowed");
	}

	s = strstr(dest, find);
	if(!s)
	{
		return qfalse;
	}
	else
	{
		Q_strncpyz(backup, dest, lend + 1);
		lstart = s - dest;
		lfind = strlen(find);
		lreplace = strlen(replace);

		strncpy(s, replace, destsize - lstart - 1);
		strncpy(s + lreplace, backup + lstart + lfind, destsize - lstart - lreplace - 1);

		return qtrue;
	}
}


int Q_PrintStrlen( const char *string ) {
	int len;
	const char  *p;

	if ( !string ) {
		return 0;
	}

	len = 0;
	p = string;
	while ( *p ) {
		if ( Q_IsColorString( p ) ) {
			p += 2;
			continue;
		}
		p++;
		len++;
	}

	return len;
}


char *Q_CleanStr( char *string ) {
	char*   d;
	char*   s;
	int c;

	s = string;
	d = string;
	while ( ( c = *s ) != 0 ) {
		if ( Q_IsColorString( s ) ) {
			s++;
		} else if ( c >= 0x20 && c <= 0x7E )   {
			*d++ = c;
		}
		s++;
	}
	*d = '\0';

	return string;
}

// strips whitespaces and bad characters
qboolean Q_isBadDirChar( char c ) {
	char badchars[] = { ';', '&', '(', ')', '|', '<', '>', '*', '?', '[', ']', '~', '+', '@', '!', '\\', '/', ' ', '\'', '\"', '\0' };
	int i;

	for ( i = 0; badchars[i] != '\0'; i++ ) {
		if ( c == badchars[i] ) {
			return qtrue;
		}
	}

	return qfalse;
}

char *Q_CleanDirName( char *dirname ) {
	char*   d;
	char*   s;

	s = dirname;
	d = dirname;

	// clear trailing .'s
	while ( *s == '.' ) {
		s++;
	}

	while ( *s != '\0' ) {
		if ( !Q_isBadDirChar( *s ) ) {
			*d++ = *s;
		}
		s++;
	}
	*d = '\0';

	return dirname;
}

int Q_CountChar(const char *string, char tocount)
{
	int count;
	
	for(count = 0; *string; string++)
	{
		if(*string == tocount)
			count++;
	}
	
	return count;
}

int QDECL Com_sprintf(char *dest, int size, const char *fmt, ...) {
	int len;
	va_list argptr;

	va_start( argptr,fmt );
	len = vsnprintf( dest, size, fmt, argptr );
	va_end( argptr );

	// Dushan
	if(len >= size) {
		Com_Printf("Com_sprintf: Output length %d too short, require %d bytes.\n", size, len + 1);
	}

	if ( len == -1 ) {
		Com_Printf( "Com_sprintf: overflow of %i bytes buffer\n", size );
	}

	return len;
}

/*
============
va

does a varargs printf into a temp buffer, so I don't need to have
varargs versions of all text functions.
FIXME: make this buffer size safe someday

Ridah, modified this into a circular list, to further prevent stepping on
previous strings
============
*/
char    * QDECL va( const char *format, ... ) {
	va_list argptr;
	#define MAX_VA_STRING   32000
	static char temp_buffer[MAX_VA_STRING];
	static char string[MAX_VA_STRING];      // in case va is called by nested functions
	static int index = 0;
	char    *buf;
	int len;


	va_start( argptr, format );
	vsprintf( temp_buffer, format,argptr );
	va_end( argptr );

	if ( ( len = strlen( temp_buffer ) ) >= MAX_VA_STRING ) {
		Com_Error( ERR_DROP, "Attempted to overrun string in call to va()\n" );
	}

	if ( len + index >= MAX_VA_STRING - 1 ) {
		index = 0;
	}

	buf = &string[index];
	memcpy( buf, temp_buffer, len + 1 );

	index += len + 1;

	return buf;
}

/*
=============
TempVector

(SA) this is straight out of g_utils.c around line 210

This is just a convenience function
for making temporary vectors for function calls
=============
*/
float   *tv( float x, float y, float z ) {
	static int index;
	static vec3_t vecs[8];
	float   *v;

	// use an array so that multiple tempvectors won't collide
	// for a while
	v = vecs[index];
	index = ( index + 1 ) & 7;

	v[0] = x;
	v[1] = y;
	v[2] = z;

	return v;
}

/*
=====================================================================

  INFO STRINGS

=====================================================================
*/

/*
===============
Info_ValueForKey

Searches the string for the given
key and returns the associated value, or an empty string.
FIXME: overflow check?
===============
*/
char *Info_ValueForKey( const char *s, const char *key ) {
	char pkey[BIG_INFO_KEY];
	static char value[2][BIG_INFO_VALUE];   // use two buffers so compares
											// work without stomping on each other
	static int valueindex = 0;
	char    *o;

	if ( !s || !key ) {
		return "";
	}

	if ( strlen( s ) >= BIG_INFO_STRING ) {
		Com_Error( ERR_DROP, "Info_ValueForKey: oversize infostring [%s] [%s]", s, key );
	}

	valueindex ^= 1;
	if ( *s == '\\' ) {
		s++;
	}
	while ( 1 )
	{
		o = pkey;
		while ( *s != '\\' )
		{
			if ( !*s ) {
				return "";
			}
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value[valueindex];

		while ( *s != '\\' && *s )
		{
			*o++ = *s++;
		}
		*o = 0;

		if ( !Q_stricmp( key, pkey ) ) {
			return value[valueindex];
		}

		if ( !*s ) {
			break;
		}
		s++;
	}

	return "";
}


/*
===================
Info_NextPair

Used to itterate through all the key/value pairs in an info string
===================
*/
void Info_NextPair( const char **head, char *key, char *value ) {
	char    *o;
	const char  *s;

	s = *head;

	if ( *s == '\\' ) {
		s++;
	}
	key[0] = 0;
	value[0] = 0;

	o = key;
	while ( *s != '\\' ) {
		if ( !*s ) {
			*o = 0;
			*head = s;
			return;
		}
		*o++ = *s++;
	}
	*o = 0;
	s++;

	o = value;
	while ( *s != '\\' && *s ) {
		*o++ = *s++;
	}
	*o = 0;

	*head = s;
}


/*
===================
Info_RemoveKey
===================
*/
void Info_RemoveKey( char *s, const char *key ) {
	char    *start;
	char pkey[MAX_INFO_KEY];
	char value[MAX_INFO_VALUE];
	char    *o;

	if ( strlen( s ) >= MAX_INFO_STRING ) {
		Com_Error( ERR_DROP, "Info_RemoveKey: oversize infostring [%s] [%s]", s, key );
	}

	if ( strchr( key, '\\' ) ) {
		return;
	}

	while ( 1 )
	{
		start = s;
		if ( *s == '\\' ) {
			s++;
		}
		o = pkey;
		while ( *s != '\\' )
		{
			if ( !*s ) {
				return;
			}
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;
		while ( *s != '\\' && *s )
		{
			if ( !*s ) {
				return;
			}
			*o++ = *s++;
		}
		*o = 0;

		if ( !Q_stricmp( key, pkey ) ) {
			// rain - arguments to strcpy must not overlap
			//strcpy (start, s);	// remove this part
			memmove( start, s, strlen( s ) + 1 ); // remove this part
			return;
		}

		if ( !*s ) {
			return;
		}
	}

}

/*
===================
Info_RemoveKey_Big
===================
*/
void Info_RemoveKey_Big( char *s, const char *key ) {
	char    *start;
	char pkey[BIG_INFO_KEY];
	char value[BIG_INFO_VALUE];
	char    *o;

	if ( strlen( s ) >= BIG_INFO_STRING ) {
		Com_Error( ERR_DROP, "Info_RemoveKey_Big: oversize infostring [%s] [%s]", s, key );
	}

	if ( strchr( key, '\\' ) ) {
		return;
	}

	while ( 1 )
	{
		start = s;
		if ( *s == '\\' ) {
			s++;
		}
		o = pkey;
		while ( *s != '\\' )
		{
			if ( !*s ) {
				return;
			}
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;
		while ( *s != '\\' && *s )
		{
			if ( !*s ) {
				return;
			}
			*o++ = *s++;
		}
		*o = 0;

		if ( !Q_stricmp( key, pkey ) ) {
			strcpy( start, s );  // remove this part
			return;
		}

		if ( !*s ) {
			return;
		}
	}

}




/*
==================
Info_Validate

Some characters are illegal in info strings because they
can mess up the server's parsing
==================
*/
qboolean Info_Validate( const char *s ) {
	if ( strchr( s, '\"' ) ) {
		return qfalse;
	}
	if ( strchr( s, ';' ) ) {
		return qfalse;
	}
	return qtrue;
}

/*
==================
Info_SetValueForKey

Changes or adds a key/value pair
==================
*/
qboolean Info_SetValueForKey( char *s, const char *key, const char *value ) {
	char	newi[MAX_INFO_STRING], *v;
	int		c, maxsize = MAX_INFO_STRING;

	if ( strlen( s ) >= MAX_INFO_STRING ) {
		Com_Error( ERR_DROP, "SetValueForKey: oversize infostring [%s] [%s] [%s]", s, key, value );
	}

	if ( strchr( key, '\\' ) || strchr( value, '\\' ) ) {
		Com_Printf( "SetValueForKey: Can't use keys or values with a \\\n" );
		return qfalse;
	}

	if ( strchr( key, ';' ) || strchr( value, ';' ) ) {
		Com_Printf( "SetValueForKey: Can't use keys or values with a semicolon\n" );
		return qfalse;
	}

	if ( strchr( key, '\"' ) || strchr( value, '\"' ) ) {
		Com_Printf( "SetValueForKey: Can't use keys or values with a \"\n" );
		return qfalse;
	}

	if( strlen( key ) > MAX_INFO_KEY - 1 || strlen( value ) > MAX_INFO_KEY - 1 ) {
		Com_Error( ERR_DROP, "SetValueForKey: keys and values must be < %i characters.\n", MAX_INFO_KEY );
		return qfalse;
	}

	Info_RemoveKey( s, key );
	if ( !value || !strlen( value ) ) {
		return qtrue; // just clear variable
	}

	Com_sprintf( newi, sizeof( newi ), "\\%s\\%s", key, value );

	if ( strlen( newi ) + strlen( s ) > maxsize ) {
		Com_Printf( "SetValueForKey: Info string length exceeded\n" );
		return qtrue;
	}

	// only copy ascii values
	s += strlen( s );
	v = newi;

	while( *v ) {
		c = *v++;
		c &= 255;	// strip high bits
		if( c >= 32 && c <= 255 )
			*s++ = c;
	}
	*s = 0;

	// all done
	return qtrue;
}

/*
==================
Info_SetValueForKey_Big

Changes or adds a key/value pair
==================
*/
void Info_SetValueForKey_Big( char *s, const char *key, const char *value ) {
	char newi[BIG_INFO_STRING];

	if ( strlen( s ) >= BIG_INFO_STRING ) {
		Com_Error( ERR_DROP, "Info_SetValueForKey: oversize infostring [%s] [%s] [%s]", s, key, value );
	}

	if ( strchr( key, '\\' ) || strchr( value, '\\' ) ) {
		Com_Printf( "Can't use keys or values with a \\\n" );
		return;
	}

	if ( strchr( key, ';' ) || strchr( value, ';' ) ) {
		Com_Printf( "Can't use keys or values with a semicolon\n" );
		return;
	}

	if ( strchr( key, '\"' ) || strchr( value, '\"' ) ) {
		Com_Printf( "Can't use keys or values with a \"\n" );
		return;
	}

	Info_RemoveKey_Big( s, key );
	if ( !value || !strlen( value ) ) {
		return;
	}

	Com_sprintf( newi, sizeof( newi ), "\\%s\\%s", key, value );

	if ( strlen( newi ) + strlen( s ) > BIG_INFO_STRING ) {
		Com_Printf( "BIG Info string length exceeded\n" );
		return;
	}

	strcat( s, newi );
}

/*
============
Com_ClientListContains
============
*/
qboolean Com_ClientListContains( const clientList_t *list, int clientNum )
{
  if( clientNum < 0 || clientNum >= MAX_CLIENTS || !list )
    return qfalse;
  if( clientNum < 32 )
    return (qboolean)( ( list->lo & ( 1 << clientNum ) ) != 0 );
  else
    return (qboolean)( ( list->hi & ( 1 << ( clientNum - 32 ) ) ) != 0 );
}

/*
================
VectorMatrixMultiply
================
*/
void VectorMatrixMultiply( const vec3_t p, vec3_t m[ 3 ], vec3_t out )
{
	out[ 0 ] = m[ 0 ][ 0 ] * p[ 0 ] + m[ 1 ][ 0 ] * p[ 1 ] + m[ 2 ][ 0 ] * p[ 2 ];
	out[ 1 ] = m[ 0 ][ 1 ] * p[ 0 ] + m[ 1 ][ 1 ] * p[ 1 ] + m[ 2 ][ 1 ] * p[ 2 ];
	out[ 2 ] = m[ 0 ][ 2 ] * p[ 0 ] + m[ 1 ][ 2 ] * p[ 1 ] + m[ 2 ][ 2 ] * p[ 2 ];
}

#ifdef _MSC_VER
float rint( float v ) {
	if( v >= 0.5f ) return ceilf( v );
	else return floorf( v );
}
#endif

void QDECL Com_FatalError( const char *error, ... ) {
	va_list argptr;
	char msg[8192];

	va_start( argptr,error );
	vsprintf( msg,error,argptr );
	va_end( argptr );

	Com_Error(ERR_FATAL, msg);
}

void QDECL Com_DropError( const char *error, ... ) {
	va_list argptr;
	char msg[8192];

	va_start( argptr,error );
	vsprintf( msg,error,argptr );
	va_end( argptr );

	Com_Error(ERR_DROP, msg);
}

void QDECL Com_Warning( const char *error, ... ) {
	va_list argptr;
	char msg[8192];

	va_start( argptr,error );
	vsprintf( msg,error,argptr );
	va_end( argptr );

	Com_Printf(msg);
}


//====================================================================
