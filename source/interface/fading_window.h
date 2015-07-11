#ifndef _included_fadingwindow_h
#define _included_fadingwindow_h

#include "interface/components/core.h"



class FadingWindow : public InterfaceWindow
{
public:
    float m_alpha;

public:
    FadingWindow( char *_name );

    void Render( bool _hasFocus );
    void Render( bool _hasFocus, bool _renderButtons );


    void SaveProperties( char *_name );                     
    void LoadProperties( char *_name );
};



#endif

