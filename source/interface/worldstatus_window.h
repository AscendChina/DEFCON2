
#ifndef _included_worldstatuswindow_h
#define _included_worldstatuswindow_h

#include "interface/fading_window.h"


class WorldStatusWindow : public FadingWindow
{
protected:
    int m_lastKnownDefcon;
        
public:
    WorldStatusWindow( char *name );
    ~WorldStatusWindow();

    void Create();
    void Render( bool hasFocus );
};



// ============================================================================



class ScoresWindow : public FadingWindow
{
public:
    ScoresWindow();
    ~ScoresWindow();

    void Create();
    void Render( bool _hasFocus );
};



// ============================================================================


class StatsWindow : public InterfaceWindow
{

public:
    StatsWindow();

    void Create();
    void Render( bool hasFocus );
};


#endif

