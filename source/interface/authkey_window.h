#ifndef _included_authkeywindow_h
#define _included_authkeywindow_h

#include "interface/components/core.h"


#if !defined(RETAIL_DEMO)

class AuthKeyWindow : public InterfaceWindow
{
public:
    char    m_key[256];
    double  m_keyCheckTimer;
    double  m_timeOutTimer;

public:
    AuthKeyWindow();

    void PasteFromClipboard();

    void Create();
    void Render( bool _hasFocus );
};



class PlayDemoButton : public InterfaceButton
{
public:
    bool m_forceVisible;

public:
    PlayDemoButton();
    bool IsVisible();
    void Render( int realX, int realY, bool highlighted, bool clicked );
    void MouseUp();
};

#endif

#endif

