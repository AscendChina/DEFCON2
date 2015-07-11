#include "lib/universal_include.h"

#ifdef WIN32
#include <io.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#endif

#include <stdio.h>
#include <string.h>

#include "lib/render/renderer.h"
#include "lib/debug_utils.h"

#include "app/globals.h"
#include "app/app.h"
#include "lib/multiline_text.h"
#include "lib/string_utils.h"

static char *Substring( const char *string, unsigned int length )
{
	char *substring = new char[ length + 1 ];
	memcpy( substring, string, length );
	substring[ length ] = '\0';
	
	return substring;
}

// Goes through the string and divides it up into several smaller strings,
// taking into account newline characters, and the width of the text area.

MultiLineText::MultiLineText( const char *string, float lineSize, float fontSize, bool wrapToWindow ) 
{
	char *startOfLine, *endOfLine;
	unsigned int fullLineLen;
	char *fullLineEnd;

	if( !string || lineSize <= 0 )
	{
		m_fullLine = NULL;
		return;
	}

	fullLineLen = strlen( string );
	m_fullLine = Substring( string, fullLineLen );
	fullLineEnd = m_fullLine + fullLineLen;
		
	BitmapFont *bitmapFont = g_renderer->GetBitmapFont();

	startOfLine = m_fullLine;
	do
	{
		unsigned int sourceLineLen;

		for( endOfLine = startOfLine; *endOfLine != '\n' && *endOfLine != '\0'; endOfLine++ )
			;

		*endOfLine = '\0';

		sourceLineLen = endOfLine - startOfLine;
		
		// Is the line too long to fit?
		if( wrapToWindow && g_renderer->TextWidth( startOfLine, sourceLineLen, fontSize, bitmapFont ) > lineSize )
		{
			AddWrappedLines( startOfLine, sourceLineLen, lineSize, fontSize, bitmapFont );
		}
		else
		{
			m_lines.PutDataAtEnd( startOfLine );
		}
		
		startOfLine = endOfLine + 1;
	}
	while( endOfLine < fullLineEnd );
}

// Take a single line of source text (which we know is too long to fit), and
// split it into shorter lines that do.

void MultiLineText::AddWrappedLines( char *sourceLine, unsigned int sourceLineSize, float lineSize, float fontSize, BitmapFont *bitmapFont )
{
	char *wrappedLine;
	unsigned int wrappedLineLen;
	float cumulativeWidth;
	char *wordStart, *wordEnd;
	char *sourceLineEnd = sourceLine + sourceLineSize;

	wordStart = sourceLine;
	while( wordStart < sourceLineEnd ) // while we still have chars in the source line
	{
		wrappedLine = wordStart;
		wrappedLineLen = 0;
		cumulativeWidth = 0.0;
		
		// If only one word left on source line, don't try to wrap it
		char *wordTest;
		for( wordTest = wordStart; wordTest < sourceLineEnd && *wordTest != ' '; wordTest++ )
			;
		if ( wordTest >= sourceLineEnd ) 
		{
			m_lines.PutDataAtEnd( wordStart );
			break;
		}
		
		// Construct one wrapped line
		while( wordStart < sourceLineEnd && cumulativeWidth < lineSize ) 
		{
			unsigned int wordLen;
			
			// Find the next word in the source line
			for( wordEnd = wordStart+1; wordEnd < sourceLineEnd && *wordEnd != ' '; wordEnd++ )
				;
			wordLen = wordEnd - wordStart;
			
			// If adding the next word will fit, go ahead and append it to our buffer
			// Added by Chris to condition below : || wrappedLineLen == 0
			// This means a single word has been found that is too long to ever wrap properly
			// So just put the whole word in and be done with it
			cumulativeWidth += g_renderer->TextWidth( wordStart, wordLen, fontSize, bitmapFont );
			if( cumulativeWidth < lineSize || wrappedLineLen == 0 )
			{
				wrappedLineLen += wordLen;
				wordStart = wordEnd;
			}
		}
		
		if( wrappedLineLen > 0 )
		{
			wrappedLine[ wrappedLineLen ] = '\0';
			m_lines.PutDataAtEnd( wrappedLine );
		}
		wordStart++;

		// Skip trailing whitespace, so it doesn't cause the next line to indent
		for( ; wordStart < sourceLineEnd && *wordStart == ' '; wordStart++ )
			;
	}
}
