#ifndef INCLUDED_UNIVERSAL_INCLUDE_H
#define INCLUDED_UNIVERSAL_INCLUDE_H

#define APP_NAME        "Defcon"

#if (defined TARGET_OS_MACOSX)
	#define	APP_VERSION		"1.6"
#else
	#define APP_VERSION     "1.6"
#endif
//#define APP_VERSION     "1.5 fr rtl" // Defcon 1.5 Codemasters french build, use TARGET_RETAIL_MULTI_LANGUAGE_FRENCH
//#define APP_VERSION     "1.5 de rtl" // Defcon 1.5 german build, use TARGET_RETAIL_MULTI_LANGUAGE_GERMAN
//#define APP_VERSION     "1.5 it rtl" // Defcon 1.5 italian build, use TARGET_RETAIL_MULTI_LANGUAGE_ITALIAN
//#define APP_VERSION     "1.5 pl rtl" // Defcon 1.5 polish build, use TARGET_RETAIL_MULTI_LANGUAGE_POLISH
//#define APP_VERSION     "1.51 ru rtl" // Defcon 1.51 russian build, use TARGET_RETAIL_MULTI_LANGUAGE_RUSSIAN

//
// Choose one of these targets ONLY
//

//#define     TARGET_DEBUG
#define     TARGET_FINAL
//#define     TARGET_TESTBED
//#define     TARGET_RETAIL_UK
//#define     TARGET_RETAIL_UK_DEMO
//#define     TARGET_RETAIL
//#define     TARGET_RETAIL_DEMO
//#define     TARGET_RETAIL_MULTI_LANGUAGE_ALL_LANGUAGES
//#define     TARGET_RETAIL_MULTI_LANGUAGE_ENGLISH
//#define     TARGET_RETAIL_MULTI_LANGUAGE_FRENCH
//#define     TARGET_RETAIL_MULTI_LANGUAGE_GERMAN
//#define     TARGET_RETAIL_MULTI_LANGUAGE_ITALIAN
//#define     TARGET_RETAIL_MULTI_LANGUAGE_SPANISH
//#define     TARGET_RETAIL_MULTI_LANGUAGE_POLISH
//#define     TARGET_RETAIL_MULTI_LANGUAGE_RUSSIAN


// ============================================================================
// Dont edit anything below this line   
// ============================================================================

#ifdef TARGET_RETAIL_UK
    #define RETAIL
    #define RETAIL_BRANDING_UK
    #define TARGET_FINAL
#endif

#ifdef TARGET_RETAIL_UK_DEMO
    #define RETAIL
    #define RETAIL_DEMO
    #define RETAIL_BRANDING_UK
    #define TARGET_FINAL
#endif

#ifdef TARGET_RETAIL
    #define RETAIL
    #define RETAIL_BRANDING
    #define TARGET_FINAL
#endif

#ifdef TARGET_RETAIL_DEMO
    #define RETAIL
    #define RETAIL_DEMO
    #define RETAIL_BRANDING
    #define TARGET_FINAL
#endif

#ifdef TARGET_RETAIL_MULTI_LANGUAGE_ALL_LANGUAGES
    #define RETAIL
    #define RETAIL_BRANDING_MULTI_LANGUAGE
    #define TARGET_FINAL
#endif

#ifdef TARGET_RETAIL_MULTI_LANGUAGE_ENGLISH
    #define RETAIL
    #define RETAIL_BRANDING_MULTI_LANGUAGE
    #define TARGET_FINAL
    #define LANG_DEFAULT "english"
    #define LANG_DEFAULT_ONLY_SELECTABLE 1
    #define PREF_LANG 1
#endif

#ifdef TARGET_RETAIL_MULTI_LANGUAGE_FRENCH
    #define RETAIL
    #define RETAIL_BRANDING_MULTI_LANGUAGE
    #define TARGET_FINAL
    #define LANG_DEFAULT "french"
    #define LANG_DEFAULT_ONLY_SELECTABLE 1
    #define PREF_LANG 1
#endif

#ifdef TARGET_RETAIL_MULTI_LANGUAGE_GERMAN
    #define RETAIL
    #define RETAIL_BRANDING_MULTI_LANGUAGE
    #define TARGET_FINAL
    #define LANG_DEFAULT "german"
    #define LANG_DEFAULT_ONLY_SELECTABLE 1
    #define PREF_LANG 1
#endif

#ifdef TARGET_RETAIL_MULTI_LANGUAGE_ITALIAN
    #define RETAIL
    #define RETAIL_BRANDING_MULTI_LANGUAGE
    #define TARGET_FINAL
    #define LANG_DEFAULT "italian"
    #define LANG_DEFAULT_ONLY_SELECTABLE 1
    #define PREF_LANG 1
#endif

#ifdef TARGET_RETAIL_MULTI_LANGUAGE_SPANISH
    #define RETAIL
    #define RETAIL_BRANDING_MULTI_LANGUAGE
    #define TARGET_FINAL
    #define LANG_DEFAULT "spanish"
    #define LANG_DEFAULT_ONLY_SELECTABLE 1
    #define PREF_LANG 1
#endif

#ifdef TARGET_RETAIL_MULTI_LANGUAGE_POLISH
    #define RETAIL
    #define RETAIL_BRANDING_MULTI_LANGUAGE
    #define TARGET_FINAL
    #define LANG_DEFAULT "polish"
    #define LANG_DEFAULT_ONLY_SELECTABLE 1
    #define PREF_LANG 1
#endif

#ifdef TARGET_RETAIL_MULTI_LANGUAGE_RUSSIAN
    #define RETAIL
    #define RETAIL_BRANDING_MULTI_LANGUAGE
    #define TARGET_FINAL
    #define LANG_DEFAULT "russian"
    #define LANG_DEFAULT_ONLY_SELECTABLE 1
    #define PREF_LANG 1
#endif

#ifdef TARGET_FINAL
    #define HAVE_DSOUND 
    #define WAN_PLAY_ENABLED
    #define LAN_PLAY_ENABLED
    #define DUMP_DEBUG_LOG    
    #define TOOLS_ENABLED    
    #define PROFILER_ENABLED
    #define AUTHENTICATION_LEVEL 1
    #define FLOAT_NUMERICS
    //#define TRACK_SYNC_RAND
    #define TRACK_LANGUAGEPHRASE_ERRORS
#endif

#ifdef TARGET_DEBUG
    #define HAVE_DSOUND 
    #define WAN_PLAY_ENABLED
    #define LAN_PLAY_ENABLED
    //#define TRACK_SYNC_RAND
    #define TOOLS_ENABLED
    #define SOUND_EDITOR_ENABLED
    #define PROFILER_ENABLED
    //#define DUMP_DEBUG_LOG 
    //#define NON_PLAYABLE
    //#define SHOW_TEST_HOURS
    #define AUTHENTICATION_LEVEL 1
    #define FLOAT_NUMERICS
    #define TRACK_LANGUAGEPHRASE_ERRORS
#endif

#ifdef TARGET_TESTBED
    #define HAVE_DSOUND 
    #define WAN_PLAY_ENABLED
    #define LAN_PLAY_ENABLED  
    #define TRACK_SYNC_RAND
    #define TOOLS_ENABLED
    #define PROFILER_ENABLED
    #define NON_PLAYABLE
    #define DUMP_DEBUG_LOG 
    #define TESTBED
    #define AUTHENTICATION_LEVEL 3
    #define FLOAT_NUMERICS
#endif

//#define TRACK_MEMORY_LEAKS

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#if !defined TARGET_MSVC && !defined TARGET_OS_MACOSX && !defined TARGET_OS_LINUX
#error Please make sure that one of TARGET_MSVC, TARGET_OS_MACOSX, TARGET_OS_LINUX is set in your project preprocessor options.
// Please don't set it in this file.
#endif

#ifdef TARGET_MSVC
    #define APP_SYSTEM "PC"
#endif
#ifdef TARGET_OS_MACOSX
	#ifdef __ppc__
		#define APP_SYSTEM "Mac (PPC)"
	#else
		#define APP_SYSTEM "Mac (Intel)"
	#endif
#endif
#ifdef TARGET_OS_LINUX
    #define APP_SYSTEM "Linux"
#endif

// Windows Specific options
#ifdef TARGET_MSVC
#include "../../resources/resource.h"

#pragma warning( disable : 4244 4305 4800 4018 )
// Defines that will enable you to double click on a #pragma message
// in the Visual Studio output window.
#define MESSAGE_LINENUMBERTOSTRING(linenumber)	#linenumber
#define MESSAGE_LINENUMBER(linenumber)			MESSAGE_LINENUMBERTOSTRING(linenumber)
#define MESSAGE(x) message (__FILE__ "(" MESSAGE_LINENUMBER(__LINE__) "): "x) 

//#define USE_CRASHREPORTING

#include <crtdbg.h>
#define snprintf _snprintf

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#ifdef USE_CRASHREPORTING
#define _WIN32_WINDOWS 0x0500	// for IsDebuggerPresent
#endif
#include <windows.h>

//Fall back for getaddrinfo on older platform, see end of 
// http://msdn.microsoft.com/en-us/library/ms738520.aspx
#include <ws2tcpip.h>
#include <wspiapi.h>

#ifdef TRACK_MEMORY_LEAKS
#include "lib/memory_leak.h"
#endif
#endif // TARGET_MSVC

// Mac OS X Specific Settings
#if defined ( TARGET_OS_MACOSX ) || defined ( TARGET_OS_LINUX )
#undef HAVE_DSOUND 

#include <ctype.h>
#include <unistd.h>

#define stricmp strcasecmp
#define strnicmp strncasecmp
#define Sleep(x) usleep( 1000 * x) 

inline char * strlwr(char *s) {                                       
  for (char *p = s; *p; p++)                                          
		*p = tolower(*p);                                                 
  return s;                                                           
}                                                                     
inline char * strupr(char *s) {                                       
  for (char *p = s; *p; p++)                                          
		*p = toupper(*p);                                                 
  return s;                                                           
}

#if __GNUC__ == 3 
#define sinf sin
#define cosf cos
#define sqrtf sqrt
#define expf exp
#define powf pow
#define logf log
#endif
                        
#endif

// Include OpenGL
#ifdef TARGET_OS_MACOSX
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif // TARGET_OS_MACOSX

#endif
