
#ifndef _included_casualtieswindow_h
#define _included_casualtieswindow_h

#include "interface/components/core.h"

class ScrollBar;


class CasualtiesWindow : public InterfaceWindow
{

public:
    
    ScrollBar   *m_scrollbar;

public:
    CasualtiesWindow( char *name );

    void Create();
    void Remove();
    
    void Render( bool hasFocus );

};



#endif

