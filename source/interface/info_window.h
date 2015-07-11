#ifndef _included_infowindow_h
#define _included_infowindow_h

#include "interface/fading_window.h"



class InfoWindow : public FadingWindow
{
protected:
    int     m_objectId;
    int     m_infoType;

public:
    InfoWindow();

    void Create();
    void Render( bool _hasFocus );
    void SwitchInfoDisplay( int _objectId, int _type );
};


#endif
