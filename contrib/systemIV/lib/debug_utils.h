#ifndef DEBUG_UTILS_H
#define DEBUG_UTILS_H


void    AppDebugOut            (char *_msg, ...);                                           // Outputs to Devstudio output pane                                    

void    AppDebugOutData        (void *_data, int _dataLen);                                 // Dumps the data as a hex table


#ifdef _DEBUG
void    AppDebugAssert         (bool _condition);                                           // Does nothing in Release builds
#else
#define AppDebugAssert(x) {}
#endif // _DEBUG

void    AppReleaseAssertFailed (char const *_fmt, ...);                    // Same as DebugAssert in Debug builds

void    AppGenerateBlackBox    (char *_filename, char *_msg );

#ifdef TARGET_MSVC
#define AppReleaseAssert(cond, ...) \
	do { \
		if (!(cond)) \
			AppReleaseAssertFailed( __VA_ARGS__ ); \
	} while (false)
#else
// GCC
#define AppReleaseAssert(cond, fmt, args...) \
	do { \
		if (!(cond)) \
			AppReleaseAssertFailed( fmt, ## args ); \
	} while (false)
#endif

#define AppAssert(x)        AppReleaseAssert((x),          \
                            "Assertion failed : '%s'\n\n"  \
                            "%s\nline number %d",          \
                            #x, __FILE__, __LINE__ )

#define AppAbort(x)         AppReleaseAssert(false,        \
                            "Abort : '%s'\n\n"             \
                            "%s\nline number %d",          \
                            x, __FILE__, __LINE__ )


void    AppDebugOutRedirect     (char *_filename);




#endif

