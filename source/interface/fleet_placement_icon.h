
#ifndef _included_fleetplacementwindow_h
#define _included_fleetplacementwindow_h

#include "interface/components/core.h"


class FleetPlacementIconWindow : public InterfaceWindow
{

public:
    FleetPlacementIconWindow( char *name, int fleetId );

	void Create();
    void Render( bool hasFocus );
protected:
	int m_fleetId;
};


class FleetPlacementIconButton : public InterfaceButton
{
public:
	FleetPlacementIconButton( int unitType);

	int m_fleetId;

protected:
	Image *bmpImage;

public:
    void Render( int realX, int realY, bool highlighted, bool clicked );
    void MouseUp();
};

#endif
