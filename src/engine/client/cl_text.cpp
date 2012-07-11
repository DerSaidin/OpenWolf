/*
===========================================================================

OpenWolf GPL Source Code
Copyright (C) 2007 HermitWorks Entertainment Corporation
Copyright (C) 2012 Dusan Jocic <dusanjocic@msn.com>

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

#include "client.h"

int CL_RenderText_ExtractLines( fontInfo_t * font, float fontScale, float startColor[4], float baseColor[4], const char * text,
	int limit, int align, float width, lineInfo_t * lines, int line_offset, int line_limit )
{
	lineInfo_t dummyLine;

	int lineCount = 0;
	const char* s = text;
	float rgba[4];
	qboolean clipToWidth;

	Vec4_Cpy( rgba, startColor );

	if( align & TEXT_ALIGN_NOCLIP )
	{
		clipToWidth = qfalse;
		align &= ~TEXT_ALIGN_NOCLIP;
	}
	else
		clipToWidth = qtrue;

	for( ; ; )
	{
		int		spaceCount	= 0;
		int		lastSpace	= 0;
		int		widthAtLastSpace = 0;
		int		count		= 0;
		lineInfo_t* line = lines ? lines + lineCount : &dummyLine;

		line->text		= s;
		line->sa		= 0.0f;
		line->ox		= 0.0f;
		line->width		= 0.0f;
		line->height	= 0.0f;

		Vec4_Cpy( line->defaultColor, baseColor );

		Vec4_Cpy( line->startColor, rgba );
		Vec4_Cpy( line->endColor, rgba );

		if( lines && lineCount > 0 )
			Vec4_Cpy( lines[lineCount - 1].endColor, rgba );

		for( ; ; )
		{
			int				c = s[ count ];
			glyphInfo_t*	g = font->glyphs + c;

			// line has ended
			if ( c == '\n' || c == '\0' )
				break;

			if ( Q_IsColorString( s + count) )
			{
				count++;

				if( s[count] == '-' )
					Vec4_Cpy( rgba, baseColor );
				else
					Vec4_Cpy( rgba, g_color_table[ColorIndex( s[count] )] );
		
				count++;

				continue;
			}

			// record white space
			if ( c == ' ' )
			{
				lastSpace = count;
				widthAtLastSpace = line->width;
				spaceCount++;
			}

			// line is too long
			if( clipToWidth && (line->width + (g->xSkip*fontScale) > width) )
			{
				if ( limit == -2 )
					break;

				if ( spaceCount > 0 )
				{
					count		= lastSpace;
					line->width	= widthAtLastSpace;
					spaceCount--;

					if ( align == TEXT_ALIGN_JUSTIFY )
						line->sa = (width-line->width) / (float)spaceCount;
				}

				if ( count == 0 )	// width is less than 1 character?
				{
					line->width = g->xSkip*fontScale;
					count = 1;
				}

				break;
			}

			// record height
			if ( g->height > line->height )
				line->height = g->height;

			// move along
			if ( c == '\t')
			{
			} else
				line->width += ((float)g->xSkip*fontScale);

			count++;

			if ( limit>0 && count >= limit )
				break;
		}

		line->count	= count;

		if ( align == TEXT_ALIGN_RIGHT )
		{
			line->ox = width-line->width;

		} else if ( align == TEXT_ALIGN_CENTER )
		{
			line->ox = (width-line->width)/2;
		}

		if ( line_offset > 0 ) {
			line_offset--;
		} else
			lineCount++;

		if( line_limit > 0 && lineCount >= line_limit )
			break;

		s += count;

		if ( limit == -2 )
			break;

		if ( *s == '\0' )
			break;

		if ( limit>0 )
		{
			if ( count >= limit )
				break;

			limit -= count;
		}

		if ( *s == '\n' )
			s++;

		if ( *s == ' ' )
			s++;
	}

	return lineCount;
}
