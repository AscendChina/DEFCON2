#ifndef _included_toolbar_h
#define _included_toolbar_h


#include "interface/fading_window.h"


class Toolbar : public FadingWindow
{
public:
    int     m_numButtons;

public:
    Toolbar();
    ~Toolbar();

    void Create();
    void Render( bool _hasFocus );
};



class ToolbarButton : public InterfaceButton
{
public:
    char m_iconFilename[256];
    bool m_disabled;
    bool m_toggle;

public:
    ToolbarButton();

    void Render( int realX, int realY, bool highlighted, bool clicked );
};


#endif

