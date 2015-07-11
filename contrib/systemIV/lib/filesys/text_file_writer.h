#ifndef _included_textfilewriter_h
#define _included_textfilewriter_h

/*
 *	A thin wrapper for ordinary text file IO
 *  with the added ability to encrypt the data
 *  as it is sent to the file
 *
 */

#include <stdio.h>


class TextFileWriter
{
protected:
	int		m_offsetIndex;
	FILE	*m_file;
	bool	m_encrypt;

public:
	TextFileWriter  (char *_filename, bool _encrypt);
	~TextFileWriter ();

	int printf  (char *fmt, ...);
};


#endif
