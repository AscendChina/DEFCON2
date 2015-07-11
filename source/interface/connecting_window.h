#ifndef _included_connectingwindow_h
#define _included_connectingwindow_h


#include "interface/components/core.h"



class ConnectingWindow : public InterfaceWindow
{
public:
    int     m_maxUpdatesRemaining;
    int     m_maxLagRemaining;
    bool    m_popupLobbyAtEnd;
    
    int     m_stage;
    float   m_stageStartTime;

public:
    ConnectingWindow();
    ~ConnectingWindow();

    void Create();
    void Render( bool _hasFocus );
    void RenderTimeRemaining( float _fractionDone );
};




#endif
