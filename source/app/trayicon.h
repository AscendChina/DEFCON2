
#ifndef _included_trayicon_h
#define _included_trayicon_h

#define NOMINMAX
#include <windows.h>
#include <shellapi.h>

#include "statusicon.h"

#define TRAY_CALLBACK   WM_USER+666
#define TRAY_ICONID     69


class TrayIcon : public StatusIcon
{
protected:

    NOTIFYICONDATA  m_icondata;

    int             m_currentIcon;
    int             m_mainIcon;
    int             m_subIcon;

    char            m_caption[256];

public:
    TrayIcon();
    ~TrayIcon();
    
    void SetIcon    ( const char *_iconId );
    void SetSubIcon ( const char *_iconId );
    void SetCaption ( const char *_caption );
    void RemoveIcon ();

    void Update     ();

    void OnNotifyIcon(WPARAM wParam, LPARAM lParam);
    static long EventHandler (unsigned int message, unsigned int wParam, long lParam);
};


#endif
