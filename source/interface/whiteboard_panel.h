#ifndef _included_whiteboard_panel_h
#define _included_whiteboard_panel_h


#include "interface/fading_window.h"


class WhiteBoardPanel : public FadingWindow
{
protected:
	float m_iconSize;
	float m_gap;

public:
    WhiteBoardPanel( float iconSize, float gap );
	~WhiteBoardPanel();

    void Create();
    void Render( bool _hasFocus );
};

#endif

