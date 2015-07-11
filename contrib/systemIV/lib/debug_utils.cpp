#include "lib/universal_include.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#ifdef WIN32
#include <crtdbg.h>
#elif TARGET_OS_MACOSX
#include <CoreFoundation/CoreFoundation.h>
#include <sys/stat.h>
#elif TARGET_OS_LINUX
#ifndef __GNUC__
#error Compiling on linux without GCC?
#endif
#include <execinfo.h>
#endif

#include "lib/debug_utils.h"
#include "lib/string_utils.h"
#include "lib/filesys/filesys_utils.h"


static char *s_debugOutRedirect = NULL;

//
// Returns a new string containing the full path name for the log file _filename, inside the
// appropriate directory.
//
char *AppDebugLogPath(const char *_filename)
{
#if TARGET_OS_MACOSX
	// Put the log file in ~/Library/Logs/<application name>/ if possible, falling back to /tmp
	CFStringRef appName, dir, path;
	char *utf8Path;
	int maxUTF8PathLen;
	
	appName = (CFStringRef)CFDictionaryGetValue(CFBundleGetInfoDictionary(CFBundleGetMainBundle()),
												kCFBundleNameKey);
	if (appName && getenv("HOME"))
	{
		char *utf8Dir;
		int maxUTF8DirLen;
		struct stat dirInfo;
		
		dir = CFStringCreateWithFormat(NULL, NULL, CFSTR("%s/Library/Logs/%@"), getenv("HOME"), appName);
		maxUTF8DirLen = CFStringGetMaximumSizeForEncoding(CFStringGetLength(dir), kCFStringEncodingUTF8);
		utf8Dir = new char[maxUTF8DirLen + 1];
		CFStringGetCString(dir, utf8Dir, maxUTF8DirLen, kCFStringEncodingUTF8);
		
		// Make the directory in case it doesn't exist
		CreateDirectoryRecursively(utf8Dir);
			
		delete utf8Dir;
	}
	else
		dir = CFSTR("/tmp");
		
	path = CFStringCreateWithFormat(NULL, NULL, CFSTR("%@/%s"), dir, _filename);
	maxUTF8PathLen = CFStringGetMaximumSizeForEncoding(CFStringGetLength(path), kCFStringEncodingUTF8);
	utf8Path = new char[maxUTF8PathLen + 1];
	CFStringGetCString(path, utf8Path, maxUTF8PathLen, kCFStringEncodingUTF8);
	
	CFRelease(dir);
	CFRelease(path);
	return utf8Path;
	
#elif TARGET_OS_LINUX
	// Just throw it in /tmp
	char *path = new char[strlen(_filename) + sizeof("/tmp/") + 1];
	
	strcpy(path, "/tmp/");
	strcat(path, _filename);
	return path;
#else
	// Use the current directory
	return newStr(_filename);
#endif
}

#ifndef WIN32
inline
void OutputDebugString( const char *s )
{
	fputs( s, stderr );
}
#endif


void AppDebugOutRedirect(char *_filename)
{
	// re-using same log file is a no-op
    if ( !s_debugOutRedirect || strcmp(s_debugOutRedirect, AppDebugLogPath(_filename)) != 0 )
	{
		s_debugOutRedirect = AppDebugLogPath(_filename);

		// Check we have write access, and clear the file

		FILE *file = fopen(s_debugOutRedirect, "w" );

		if( !file )
		{
			delete s_debugOutRedirect;
			s_debugOutRedirect = NULL;

			AppDebugOut( "Failed to open %s for writing\n", _filename );
		}
		else
		{
			fclose( file );
		}
	}
}


void AppDebugOut(char *_msg, ...)
{
    char buf[512];
    va_list ap;
    va_start (ap, _msg);
    vsprintf(buf, _msg, ap);

    if( s_debugOutRedirect )
    {
        FILE *file = fopen(s_debugOutRedirect, "a" );
        if( !file )
        {
            AppDebugOut( "Failed to open %s for writing\n", s_debugOutRedirect );
            delete s_debugOutRedirect;
            s_debugOutRedirect = NULL;
            OutputDebugString(buf);
        }
        else
        {
            fprintf( file, "%s", buf );
            fclose( file );
        }
    }
    else
    {
        OutputDebugString(buf);
    }
}


void AppDebugOutData(void *_data, int _dataLen)
{
    for( int i = 0; i < _dataLen; ++i )
    {
        if( i % 16 == 0 )
        {
            AppDebugOut( "\n" );
        }
        AppDebugOut( "%02x ", ((unsigned char *)_data)[i] );
    }    
    AppDebugOut( "\n\n" );
}


#ifdef _DEBUG
void AppDebugAssert(bool _condition)
{
    if(!_condition)
    {
#ifdef WIN32
		ShowCursor(true);
		_ASSERT(_condition); 
#else
		abort();
#endif  
    }
}
#endif // _DEBUG


void AppReleaseAssertFailed(char const *_msg, ...)
{
	char buf[512];
	va_list ap;
	va_start (ap, _msg);
	vsprintf(buf, _msg, ap);

#ifdef WIN32
	ShowCursor(true);
	MessageBox(NULL, buf, "Fatal Error", MB_OK);
#else
	fputs(buf, stderr);
#endif

#ifndef _DEBUG
	AppGenerateBlackBox("blackbox.txt",buf);
	throw;
	//exit(-1);
#else
#ifdef WIN32
	_ASSERT(false);
#else
	abort();
#endif
#endif // _DEBUG
}


unsigned *getRetAddress(unsigned *mBP)
{
#ifdef WIN32
	unsigned *retAddr;

	__asm {
		mov eax, [mBP]
		mov eax, ss:[eax+4];
		mov [retAddr], eax
	}

	return retAddr;
#else
	unsigned **p = (unsigned **) mBP;
	return p[1];
#endif
}


void AppGenerateBlackBox( char *_filename, char *_msg )
{
	char *path = AppDebugLogPath( _filename );
    FILE *_file = fopen( path, "wt" );
    if( _file )
    {
        fprintf( _file, "=========================\n" );
        fprintf( _file, "=   BLACK BOX REPORT    =\n" );
        fprintf( _file, "=========================\n\n" );

        fprintf( _file, "%s %s built %s\n", APP_NAME, APP_VERSION, __DATE__  );

        time_t timet = time(NULL);
        tm *thetime = localtime(&timet);
        fprintf( _file, "Date %d:%d, %d/%d/%d\n\n", thetime->tm_hour, thetime->tm_min, thetime->tm_mday, thetime->tm_mon+1, thetime->tm_year+1900 );

        if( _msg ) fprintf( _file, "ERROR : '%s'\n", _msg );

		// For MacOSX, suggest Smart Crash Reports: http://smartcrashreports.com/
		
#ifndef TARGET_OS_MACOSX
        //
        // Print stack trace
	    // Get our frame pointer, chain upwards

        fprintf( _file, "\n" );
        fprintf( _file, "=========================\n" );
        fprintf( _file, "=      STACKTRACE       =\n" );
        fprintf( _file, "=========================\n\n" );
        fflush(_file);
#ifndef TARGET_OS_LINUX
	    unsigned *framePtr;
        unsigned *previousFramePtr = NULL;

    
#ifdef WIN32
	    __asm { 
		    mov [framePtr], ebp
	    }
#else
	    asm (
	        "movl %%ebp, %0;"
	        :"=r"(framePtr)
	        );
#endif
	    while(framePtr) {
		                    
		    fprintf(_file, "retAddress = %p\n", getRetAddress(framePtr));
		    framePtr = *(unsigned **)framePtr;

	        // Frame pointer must be aligned on a
	        // DWORD boundary.  Bail if not so.
	        if ( (unsigned long) framePtr & 3 )   
		    break;                    

            if ( framePtr <= previousFramePtr )
                break;

            // Can two DWORDs be read from the supposed frame address?          
#ifdef WIN32
            if ( IsBadWritePtr(framePtr, sizeof(PVOID)*2) ||
                 IsBadReadPtr(framePtr, sizeof(PVOID)*2) )
                break;
#endif

            previousFramePtr = framePtr;
    
        }
#else // __LINUX__
// new "GNUC" way of doing things
// big old buffer on the stack, (won't fail)
    void *array[40];
    size_t size;
// fetch the backtrace
    size = backtrace(array, 40);
// Stream it to the file
    backtrace_symbols_fd(array, size, fileno(_file));

#endif // __LINUX__
#endif // TARGET_OS_MACOSX



        fclose( _file );
    }
	delete path;
}
