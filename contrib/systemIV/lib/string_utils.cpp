#include "lib/universal_include.h"

#include "string_utils.h"

#include <string.h>
#include <ctype.h>


char *newStr( const char *s)
{
    if ( !s )
        return NULL;
	char *d = new char [strlen(s) + 1];
	strcpy(d, s);
	return d;
}	

void StringReplace( const char *target, const char *string, char *subject )
{
	// ISSUE: This function has the following problems:
	//		   1. It writes into a temporary space (final) which is of fixed length 1024.
	//		   2. It copies that result into 'subject', which not have sufficient space.
	
    char final[1024];
    memset( final, 0, sizeof(final) );
    int bufferPosition = 0;

	size_t subjectLen = strlen( subject );
	size_t targetLen = strlen( target );
	size_t stringLen = strlen( string );

    for( size_t i = 0; i <= subjectLen; ++i )
    {
        if( subject[i] == target[0] && i + targetLen <= subjectLen)
        {
            bool match = true;
            for( size_t j = 0; j < targetLen; ++j )
            {
                if( subject[i + j] != target[j] )
                {
                    match = false;
                }
            }

            if( match )
            {
                for( size_t j = 0; j < stringLen; ++j )
                {                        
                    final[bufferPosition] = string[j];
                    bufferPosition++;
                }
            }
            else
            {
                final[bufferPosition] = subject[i];
                bufferPosition++;
            }

        }
        else
        {
            final[bufferPosition] = subject[i];
            bufferPosition++;
        }
    }
    strcpy( subject, final );
}


void SafetyString( char *string )
{
    int strlength = strlen(string);

    for( int i = 0; i < strlength; ++i )
    {
        if( string[i] == '%' ) 
        {
            string[i] = ' ';
        }
    }
}


void ReplaceExtendedCharacters( char *string )
{
    int strlength = strlen(string);

    for( int i = 0; i < strlength; ++i )
    {
        if( ( (unsigned char) string[i] ) > 127 )
        {
            string[i] = ' ';
        }
    }
}


void StripExtendedCharacters( char *string )
{
    unsigned char *searchString = (unsigned char *) string;

    for( ; *searchString; searchString++ )
    {
        if( *searchString <= 127 )
        {
            *string = *searchString;
            string++;
        }
    }
    *string = '\x0';
}


void StripTrailingWhitespace ( char *string )
{
    for( int i = strlen(string)-1; i >= 0; --i )
    {
        if( string[i] == ' ' ||
            string[i] == '\n' ||
            string[i] == '\r' )
        {
            string[i] = '\x0';
        }
        else
        {
            break;
        }
    }
}


