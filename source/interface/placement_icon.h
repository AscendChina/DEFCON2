
#ifndef _included_placementwindow_h
#define _included_placementwindow_h

#include "interface/components/core.h"


class PlacementIconWindow : public InterfaceWindow
{
public:
    int m_unitType;

public:
    PlacementIconWindow( char *name, int unitType );

	void Create();
    void Render( bool hasFocus );
};


class PlacementIconButton : public InterfaceButton
{
public:
	PlacementIconButton( int unitType);

	int m_unitType;

protected:
	Image *bmpImage;

public:
    void Render( int realX, int realY, bool highlighted, bool clicked );
    void MouseUp();
};

#endif

