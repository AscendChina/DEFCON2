#ifndef _included_badkeywindow_h
#define _included_badkeywindow_h

#include "interface/components/core.h"


class BadKeyWindow : public InterfaceWindow
{    
public:
    char    m_extraMessage[1024];
    bool    m_offerDemo;

public:
    BadKeyWindow();

    void Create();
    void Render( bool _hasFocus );
};



#endif
