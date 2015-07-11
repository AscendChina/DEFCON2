#ifndef _included_resynchronisewindow_h
#define _included_resynchronisewindow_h

#include "interface/components/core.h"


class ResynchroniseWindow : public InterfaceWindow
{
public:
    bool m_debugVersion;

public:
    ResynchroniseWindow();

    void Create();
    void Update();
    void Render( bool _hasFocus );
};


#endif

