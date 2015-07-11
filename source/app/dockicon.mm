#import "dockicon.h"
#import <math.h>

static const float SUBICON_BLINK_INTERVAL = 1.0;
static const float MAX_SUBICON_BLINK = 8.0;

DockIcon::DockIcon()
:   m_mainIcon(nil),
	m_subIconId(NULL),
    m_subIcon(nil),
    m_animationStart(nil),
	m_lastDuplicateRequest(nil),
	m_mainIconIsSet(false)
{
	m_dockMenu = [NSMenu new];
}

void DockIcon::SetIcon ( const char *_iconId )
{	
    [m_mainIcon release];
    if (_iconId)
    {
        m_mainIcon = [NSImage imageNamed:[NSString stringWithUTF8String:_iconId]];
        [m_mainIcon retain];
    }
    else
        m_mainIcon = nil;
		
	m_mainIconIsSet = false;
}

void DockIcon::SetSubIcon ( const char *_iconId )
{	
    if (_iconId)
    {
		if (_iconId == m_subIconId)
		{
			[m_lastDuplicateRequest release];
			m_lastDuplicateRequest = [NSDate new];
		}
		else
		{
			[m_subIcon release];
			[m_animationStart release];
			[m_lastDuplicateRequest release];
			
			m_subIcon = [[NSImage imageNamed:[NSString stringWithUTF8String:_iconId]] retain];
			m_animationStart = [NSDate new];
			m_lastDuplicateRequest = [m_animationStart retain];
		}
    }
    else
    {
		[m_subIcon release];
		[m_animationStart release];
		[m_lastDuplicateRequest release];
		
        m_subIcon = nil;
        m_animationStart = nil;
		m_lastDuplicateRequest = nil;
    }
	
	m_subIconId = _iconId;
}

void DockIcon::SetCaption ( const char *_caption )
{
    if (strcmp(_caption, "") == 0)
	{
		if ([m_dockMenu numberOfItems] > 0)
			[m_dockMenu removeItemAtIndex:0];
	}
    else
    {
		if ([m_dockMenu numberOfItems] == 0)
		{
			NSMenuItem *item = [[NSMenuItem new] autorelease];
			[item setEnabled:NO];
			[m_dockMenu addItem:item];
		}
		
		[[m_dockMenu itemAtIndex:0] setTitle:[NSString stringWithUTF8String:_caption]];
	}
}

void DockIcon::RemoveIcon ()
{
    SetIcon("NSApplicationIcon"); // special name for the default app icon
    SetSubIcon(NULL);
}

NSMenu *DockIcon::GetDockMenu()
{
	return m_dockMenu;
}

void DockIcon::Update ()
{
    NSImage *composite;
	float alphaValue;
    NSTimeInterval timeSinceLastRequest = -[m_lastDuplicateRequest timeIntervalSinceNow];
	NSTimeInterval timeSinceAnimationStart = -[m_animationStart timeIntervalSinceNow];
    
    if (m_subIcon && timeSinceLastRequest < MAX_SUBICON_BLINK)
    {
		composite = [[[NSImage alloc] initWithSize:[m_subIcon size]] autorelease];
        alphaValue = ( cos( timeSinceAnimationStart * (M_PI / SUBICON_BLINK_INTERVAL)) + 1 ) / 2;
        [composite lockFocus];
			[m_mainIcon compositeToPoint:NSZeroPoint operation:NSCompositePlusLighter fraction:1-alphaValue];
            [m_subIcon compositeToPoint:NSZeroPoint operation:NSCompositePlusLighter fraction:alphaValue];
        [composite unlockFocus];
		[NSApp setApplicationIconImage:composite];
		
		m_mainIconIsSet = false;
    }
	else if (!m_mainIconIsSet)
	{
		[NSApp setApplicationIconImage:m_mainIcon];
		m_mainIconIsSet = true;
	}
}

DockIcon::~DockIcon ()
{
    [m_mainIcon release];
    [m_subIcon release];
    [m_dockMenu release];
	[m_animationStart release];
	[m_lastDuplicateRequest release];
}
