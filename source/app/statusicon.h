
#ifndef _included_statusicon_h
#define _included_statusicon_h

//
// Get status icon names from the generated resources header (Win32), or use
// the actual filename in the Resources directory of the app bundle (OS X).
//
#if defined(WIN32)
    #include "../resources/resource.h"
    // kludges to stringify preprocessor defines
    #define _mkstr(x) _mkval(x)
    #define _mkval(x) #x
    
    #define STATUS_ICON_MAIN  _mkstr(IDI_ICON1)
    #define STATUS_ICON_NUKE  _mkstr(IDI_ICON_NUKE)
    #define STATUS_ICON_SUB   _mkstr(IDI_ICON_SUB)
    #define STATUS_ICON_EVENT _mkstr(IDI_ICON_EVENT)
    #define STATUS_ICON_TIMER _mkstr(IDI_ICON_TIMER)
#elif defined(TARGET_OS_MACOSX) || defined(TARGET_OS_LINUX)
    // all have implicit .png extension
    #define STATUS_ICON_MAIN  "Defcon"
    #define STATUS_ICON_NUKE  "StatusIconNuke"
    #define STATUS_ICON_SUB   "StatusIconSub"
    #define STATUS_ICON_EVENT "StatusIconEvent"
    #define STATUS_ICON_TIMER "StatusIconTimer"
#endif

//
// abstract base class that abstracts across platforms
//
class StatusIcon
{
public:
    static StatusIcon *Create();

    virtual void SetIcon    ( const char *_iconId ) = 0;
    virtual void SetSubIcon ( const char *_iconId ) = 0;
    virtual void SetCaption ( const char *_caption ) = 0;
    virtual void RemoveIcon () = 0;
    
    virtual void Update     () = 0;
};

#endif
