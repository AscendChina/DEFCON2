#include "lib/universal_include.h"
#include "lib/hi_res_time.h"
#include "lib/gucci/window_manager.h"
#include "lib/gucci/input.h"
#include "lib/language_table.h"

#include "app.h"
#include "trayicon.h"
#include "globals.h"

#define SUBICON_BLINK_INTERVAL 0.5f

TrayIcon::TrayIcon()   
:   m_mainIcon(-1),
    m_subIcon(-1),
    m_currentIcon(-1)
{   	
    m_caption[0] = '\x0';
}

TrayIcon::~TrayIcon()
{
    RemoveIcon();
}

#ifdef TARGET_MSVC
#define NOMINMAX
#include <windows.h>

#include "lib/gucci/window_manager_win32.h"

void TrayIcon::SetSubIcon ( const char *_iconId )
{
    m_subIcon = atoi(_iconId);
}

void TrayIcon::SetIcon( const char *_iconId )
{
    m_mainIcon = atoi(_iconId);
}


void TrayIcon::SetCaption ( const char *_caption )
{
	if (strlen(_caption) > 0)
		snprintf(m_caption, sizeof(m_caption), "%s: %s", LANGUAGEPHRASE("tray_icon_caption"), _caption);
	else
		strcpy(m_caption, LANGUAGEPHRASE("tray_icon_caption"));
}


void TrayIcon::Update()
{
    int desiredIcon = m_mainIcon;
    if( m_subIcon != -1 && 
        fmodf(GetHighResTime(),1) < SUBICON_BLINK_INTERVAL )
    {
        desiredIcon = m_subIcon;
    }


    if( m_currentIcon != desiredIcon )
    {
	    m_icondata.cbSize = sizeof(NOTIFYICONDATA);    
        m_icondata.hWnd = GetWindowManagerWin32()->m_hWnd;
	    m_icondata.uCallbackMessage = TRAY_CALLBACK;
	    m_icondata.uFlags = NIF_ICON|NIF_MESSAGE|NIF_TIP;
	    m_icondata.uID = TRAY_ICONID;
        m_icondata.hIcon = LoadIcon(GetWindowManagerWin32()->GethInstance(), MAKEINTRESOURCE(desiredIcon));

        strcpy(m_icondata.szTip, m_caption );

	    
        //This load an Icon -- Note: Must be Resource Based
        if( !m_icondata.hIcon )
        {
            // Failed to find the icon
            return;
        }

        int operation = ( m_currentIcon == -1 ? NIM_ADD : NIM_MODIFY );
	    bool success = Shell_NotifyIcon(operation,&m_icondata);
        if( success )
        {
            m_currentIcon = desiredIcon;
        }
        else
        {
            m_currentIcon = -1;
        }
    }
}

void TrayIcon::RemoveIcon ()
{
    if( m_currentIcon != -1 )
    {
        Shell_NotifyIcon(NIM_DELETE,&m_icondata);
        m_currentIcon = -1;
        m_subIcon = -1;
    }
}

void TrayIcon::OnNotifyIcon(WPARAM wParam, LPARAM lParam)
{
	//This function recieves information when the user clicks on the icon
 
    UINT uID = (UINT)wParam; 
    UINT uMouseMsg = (UINT)lParam; 
	
	//First make sure it is our icon, and not somebody elses, that has been clicked
	if(uID == TRAY_ICONID)
	{
		if(uMouseMsg == WM_RBUTTONDOWN)
		{
            //g_app->ShowWindow();			
		}
		else if(uMouseMsg == WM_LBUTTONDOWN)
    	{
           //g_app->ShowWindow();
		}		 
	}

    return; 
}

long TrayIcon::EventHandler(unsigned int message, unsigned int wParam, long lParam)
{
    if(message == TRAY_CALLBACK)
    {
        switch (lParam)
        {
            case WM_LBUTTONDBLCLK:

            switch (wParam) 
            {
                case TRAY_ICONID:
                    ShowWindow(GetWindowManagerWin32()->m_hWnd, SW_RESTORE);
                    SetForegroundWindow(GetWindowManagerWin32()->m_hWnd);
                    SetFocus(GetWindowManagerWin32()->m_hWnd);
                    g_app->GetStatusIcon()->RemoveIcon();
                    g_app->m_hidden = false;

                    return 0;
                break;
            }
            break;
        }
    }
    return -1;
}
#endif
