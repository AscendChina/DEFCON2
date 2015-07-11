
#ifndef _included_sidepanel_h
#define _included_sidepanel_h

#include "interface/components/core.h"
#include "interface/fading_window.h"

#include "world/worldobject.h"

class SidePanel : public FadingWindow
{
public:
    bool m_expanded;
    bool m_moving;

    int m_mode;
    int m_currentFleetId;
    
    int m_previousUnitCount;

	float m_fontsize;

    enum
    {
        ModeUnitPlacement,
        ModeFleetPlacement,
        ModeUnitControl,
        ModeFleetControl,
        NumModes
    };

public:
    SidePanel( char *name );
    ~SidePanel();

    void Create();
    
    void Render( bool hasFocus );

    void ChangeMode( int mode );
};



class UnitPlacementButton : public InterfaceButton
{
public:
	UnitPlacementButton( int unitType);

	int m_unitType;

protected:
	bool	m_disabled;

public:
    void Render( int realX, int realY, bool highlighted, bool clicked );
    void MouseUp();
};




class PanelModeButton : public InterfaceButton
{
public:
    char    bmpImageFilename[128];
    bool    m_disabled;

protected:    
    bool    m_imageButton;
    int     m_mode;

public:
    PanelModeButton( int mode, bool useImage );
    void Render( int realX, int realY, bool highlighted, bool clicked );
    void MouseUp();
};




class AddToFleetButton : public InterfaceButton
{
public:
    int m_unitType;

protected:
    bool    m_disabled;

public:
    AddToFleetButton( int unitType );
    void Render( int realX, int realY, bool highlighted, bool clicked );
    void MouseUp();
};



class RemoveUnitButton : public InterfaceButton
{
public:
    int m_memberId;
protected:
    bool m_disabled;

public:
    RemoveUnitButton();
    void Render( int realX, int realY, bool highlighted, bool clicked );
    void MouseUp();
};



class NewFleetButton : public InterfaceButton
{
protected:
    bool    m_disabled;

public:
    NewFleetButton();
    void Render( int realX, int realY, bool highlighted, bool clicked );
    void MouseUp();
};



class FleetPlacementButton : public InterfaceButton
{
public:
    bool	m_disabled;

protected:
    Image	*bmpImage;

public:
    FleetPlacementButton();
    void Render( int realX, int realY, bool highlighted, bool clicked );
    void MouseUp();
};

#endif

