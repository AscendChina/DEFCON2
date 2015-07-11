#ifndef _included_dockicon_h
#define _included_dockicon_h

#include <Carbon/Carbon.h>
#include "statusicon.h"

#ifdef __OBJC__
#import <Cocoa/Cocoa.h>
#else
class NSImage;
class NSDate;
class NSMenu;
#endif

class DockIcon : public StatusIcon
{
protected:
    NSImage *m_mainIcon;
	const char *m_subIconId;
    NSImage *m_subIcon;
    NSMenu *m_dockMenu;
    
    NSDate *m_animationStart;
	NSDate *m_lastDuplicateRequest;
	bool m_mainIconIsSet;
    
public:
    DockIcon();
    ~DockIcon();
    
    void SetIcon    ( const char *_iconId );
    void SetSubIcon ( const char *_iconId );
    void SetCaption ( const char *_caption );
    void RemoveIcon ();
	NSMenu *GetDockMenu ();

    void Update     ();
};

#endif

