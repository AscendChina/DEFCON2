
/*
 * ==========
 * Multi-line (i.e. wrapped) text
 * ==========
 *
 */


#ifndef _included_multiline_text_h
#define _included_multiline_text_h

#include "lib/tosser/llist.h"

class BitmapFont;


// Represents a multi-line chunk of text using a linked list of pointers into
// a byte buffer. The buffer contains adjacent C strings (created by replacing
// chars in the original with null bytes). The first pointer in the list is
// also the pointer to the whole buffer.

class MultiLineText
{
	public:
		MultiLineText( const char *string, float lineSize,
					   float fontSize=0, bool wrapToWindow=true );
		~MultiLineText()
		{
			if( m_fullLine )
			{
				m_lines.Empty();
				delete [] m_fullLine;
			}
		}
		
		char *operator[](unsigned index)
		{
			return m_lines.GetData(index);
		}
		
		unsigned Size()
		{
			return m_lines.Size();
		}
	
	private:
		void AddWrappedLines( char *sourceLine, unsigned int sourceLineSize, float lineSize, float fontSize, BitmapFont *bitmapFont );

		LList <char *> m_lines;
		char *m_fullLine;
};

#endif
