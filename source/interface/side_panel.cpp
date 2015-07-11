#include "lib/universal_include.h"
#include "lib/render/colour.h"
#include "lib/render/renderer.h"
#include "lib/resource/image.h"
#include "lib/language_table.h"
#include "lib/gucci/input.h"
#include "lib/gucci/window_manager.h"

#include "app/app.h"
#include "app/globals.h"
#include "app/game.h"

#include "interface/side_panel.h"
#include "interface/placement_icon.h"
#include "interface/fleet_placement_icon.h"
#include "interface/info_window.h"

#include "renderer/map_renderer.h"

#include "world/world.h"
#include "world/team.h"
#include "world/fleet.h"


SidePanel::SidePanel( char *name )
:   FadingWindow(name),
    m_expanded(false),
    m_moving(false),
    m_mode(ModeUnitPlacement),
    m_currentFleetId(-1),
    m_previousUnitCount(0),
	m_fontsize(13.0f)
{
    //SetMovable(false);
    SetSize( 100, 400 );
	if( g_windowManager->WindowH() > 480 )
	{
		SetPosition( 0, 100 );
	}
	else
	{
		SetPosition( 0, 0 );
	}

    LoadProperties( "WindowUnitCreation" );
}


SidePanel::~SidePanel()
{
	Team *myTeam = g_app->GetWorld()->GetTeam( g_app->GetWorld()->m_myTeamId );
	
	// Put back any ships currently being placed, so they don't end up in limbo
	if( myTeam->m_fleets.ValidIndex( m_currentFleetId ) &&
	    !myTeam->m_fleets[ m_currentFleetId ]->m_active )
	{
		Fleet *fleet = myTeam->GetFleet( m_currentFleetId );
		
		while ( fleet->m_memberType.Size() > 0 )
		{
			myTeam->m_unitsAvailable[ fleet->m_memberType[0] ]++;
			myTeam->m_unitCredits += g_app->GetWorld()->GetUnitValue( fleet->m_memberType[0] );
			fleet->m_memberType.RemoveData(0);
		}
		
		myTeam->m_fleets.RemoveData( m_currentFleetId );
		delete fleet;
	}
	
    SaveProperties( "WindowUnitCreation" );
}


void SidePanel::Create()
{
    //InterfaceWindow::Create();
    //CreateExpandButton();
    int x = 26;
    int y = 50;

	m_fontsize = 48 / 1.2f / 4.0f;

    UnitPlacementButton *radar = new UnitPlacementButton(WorldObject::TypeRadarStation);
    radar->SetProperties( "Radar", x, y, 48, 48, "", "tooltip_place_radar", false, true );
    RegisterButton( radar );

    UnitPlacementButton *silo = new UnitPlacementButton(WorldObject::TypeSilo);
    silo->SetProperties( "Silo", x, y+75, 48, 48, "", "tooltip_place_silo", false, true );
    RegisterButton( silo );

    UnitPlacementButton *airbase = new UnitPlacementButton(WorldObject::TypeAirBase);
    airbase->SetProperties( "AirBase", x, y+150, 48, 48, "", "tooltip_place_airbase", false, true );
    RegisterButton( airbase );

    PanelModeButton *fmb = new PanelModeButton( ModeFleetPlacement, true );
    fmb->SetProperties( "FleetMode", x, y+220, 48, 48, "dialog_fleets", "tooltip_fleet_button", true, true );
    strcpy( fmb->bmpImageFilename, "graphics/fleet.bmp" );
    RegisterButton( fmb );
}


void SidePanel::Render( bool hasFocus )
{
    int currentSelectionId = g_app->GetMapRenderer()->GetCurrentSelectionId();
    if( currentSelectionId == -1 )
    {
        if( m_mode != ModeUnitPlacement &&
            m_mode != ModeFleetPlacement )
        {
            ChangeMode( ModeUnitPlacement );
            m_currentFleetId = -1;
        }
    }
    Team *myTeam = g_app->GetWorld()->GetMyTeam();

    if( m_mode == ModeUnitPlacement )
    {
        PanelModeButton *button = (PanelModeButton *)GetButton("FleetMode");
        //if( !button->m_disabled )
        {
            int shipsRemaining = 0;
            if( myTeam )
            {
                shipsRemaining += myTeam->m_unitsAvailable[WorldObject::TypeBattleShip];
                shipsRemaining += myTeam->m_unitsAvailable[WorldObject::TypeCarrier];
                shipsRemaining += myTeam->m_unitsAvailable[WorldObject::TypeSub];

                bool unplacedFleets = false;
                bool outOfCredits = myTeam->m_unitCredits <= 0;
                if( !g_app->GetGame()->GetOptionValue("VariableUnitCounts") ) outOfCredits = false;

                if( myTeam->m_fleets.Size() > 0 &&
                    myTeam->GetFleet( myTeam->m_fleets.Size() - 1 )->m_active == false )
                {
                    unplacedFleets = true;
                }
                if( (shipsRemaining <= 0 ||
                    myTeam->m_unitCredits <= 0 )&&
                    !unplacedFleets )
                {
                    button->m_disabled = true;
                }
                else
                {
                    button->m_disabled = false;
                }
            }
        }
    }

    int w = m_w;
    m_w = 100;
    FadingWindow::Render( hasFocus, true );    
    m_w = w;
        
    char text[128];
    switch(m_mode)
    {
        case ModeUnitPlacement:     sprintf(text, LANGUAGEPHRASE("dialog_placeunit"));   break;
        case ModeFleetPlacement:    sprintf(text, LANGUAGEPHRASE("dialog_createfleet"));  break;
    }
    g_renderer->TextCentreSimple(m_x+50, m_y+10, White, m_fontsize, text );

    if( m_mode == ModeFleetPlacement )
    {
        if( m_currentFleetId != -1 )
        {
            int x = 0;
            int y = 0;
            int yMod = 0;
            
            Team *myTeam = g_app->GetWorld()->GetTeam( g_app->GetWorld()->m_myTeamId );
            if( myTeam && myTeam->m_fleets.ValidIndex(m_currentFleetId) )
            {
                if( m_mode == ModeFleetPlacement )
                {
                    x = m_x + 120;
                    y = m_y + 20;
                    yMod = 50;

                    for( int i = 0; i < 6; ++i )
                    {
                        g_renderer->RectFill( x, y, 40, 40, Colour(90,90,170,200) );
                        g_renderer->Rect(x, y, 40, 40, White );
                        y += yMod;
                    }
                    y = m_y+20;
                }
                else
                {
                    x = m_x + 10;
                    y = m_y + 40;
                    yMod = 60;
                }

                for( int i = 0; i < myTeam->m_fleets[ m_currentFleetId ]->m_memberType.Size(); ++i )
                {
                    int type = myTeam->m_fleets[ m_currentFleetId ]->m_memberType[i];
                    Image *bmpImage	= g_resource->GetImage( g_app->GetMapRenderer()->m_imageFiles[type] );
                    g_renderer->SetBlendMode( Renderer::BlendModeAdditive );
                    g_renderer->Blit( bmpImage, x+3, y, 35, 35, myTeam->GetTeamColour() );
                    g_renderer->SetBlendMode( Renderer::BlendModeNormal );
                    y += yMod;
                }
            }
        }
    }
    int variableTeamUnits = g_app->GetGame()->GetOptionValue("VariableUnitCounts");
    if( myTeam )
    {
        if( variableTeamUnits == 1 )
        {
            Colour col = Colour(255,255,0,255);
            char msg[128];
            sprintf( msg, LANGUAGEPHRASE("dialog_credits_1") );
            LPREPLACEINTEGERFLAG( 'C', myTeam->m_unitCredits, msg );
            g_renderer->TextCentreSimple( m_x+50, m_y+m_h-28, col, m_fontsize, msg );

            sprintf( msg, LANGUAGEPHRASE("dialog_credits_2"));
            g_renderer->TextCentreSimple( m_x+50, m_y+m_h-15, col, m_fontsize, msg );
        }

        int totalUnits = 0;
        for( int i = 0; i < WorldObject::NumObjectTypes; ++i )
        {
            totalUnits += myTeam->m_unitsAvailable[i];
        }

        if( totalUnits == 0 &&
            m_previousUnitCount > 0 &&
            m_currentFleetId == -1 &&
            !g_app->GetTutorial() )
        {
            EclRemoveWindow("Side Panel" );
            return;
        }
        m_previousUnitCount = totalUnits;
    }
}


void SidePanel::ChangeMode( int mode )
{
    m_mode = mode;

    int x = 26;
    int y = 50;
    m_w = 100;

    InterfaceWindow::Remove();
//    CreateExpandButton();

    if( m_mode == ModeUnitPlacement )
    {
        UnitPlacementButton *radar = new UnitPlacementButton(WorldObject::TypeRadarStation);
        radar->SetProperties( "Radar", x, y, 48, 48, "", "tooltip_place_radar", false, true );
        RegisterButton( radar );

        UnitPlacementButton *silo = new UnitPlacementButton(WorldObject::TypeSilo);
        silo->SetProperties( "Silo", x, y+75, 48, 48, "", "tooltip_place_silo", false, true );
        RegisterButton( silo );

        UnitPlacementButton *airbase = new UnitPlacementButton(WorldObject::TypeAirBase);
        airbase->SetProperties( "AirBase", x, y+150, 48, 48, "", "tooltip_place_airbase", false, true );
        RegisterButton( airbase );

        Team *myTeam = g_app->GetWorld()->GetTeam( g_app->GetWorld()->m_myTeamId );
        int shipsRemaining = 0;
        shipsRemaining += myTeam->m_unitsAvailable[WorldObject::TypeBattleShip];
        shipsRemaining += myTeam->m_unitsAvailable[WorldObject::TypeCarrier];
        shipsRemaining += myTeam->m_unitsAvailable[WorldObject::TypeSub];

        PanelModeButton *fmb = new PanelModeButton( ModeFleetPlacement, true );
        fmb->SetProperties( "FleetMode", x, y+220, 48, 48, "dialog_fleets", "tooltip_fleet_button", true, true );
        strcpy( fmb->bmpImageFilename, "graphics/fleet.bmp" );
        if( shipsRemaining == 0 )
        {
            fmb->m_disabled = true;
        }
        RegisterButton( fmb );
    }
    else if( m_mode == ModeFleetPlacement )
    {
        m_w = 160;

        AddToFleetButton *battleship = new AddToFleetButton(WorldObject::TypeBattleShip);
        battleship->SetProperties( "BattleShip", x, y, 48, 48, "", "tooltip_place_battleship", false, true );
        RegisterButton( battleship );

        AddToFleetButton *sub = new AddToFleetButton(WorldObject::TypeSub);
        sub->SetProperties( "Sub", x, y+75, 48, 48, "", "tooltip_place_sub", false, true );
        RegisterButton( sub );

        AddToFleetButton *carrier = new AddToFleetButton(WorldObject::TypeCarrier);
        carrier->SetProperties( "Carrier", x, y+150, 48, 48, "", "tooltip_place_carrier", false, true );
        RegisterButton( carrier );

        PanelModeButton *umb = new PanelModeButton( ModeUnitPlacement, true );
        umb->SetProperties( "UnitMode", x, y+220, 48, 48, "dialog_units", "", true, false );
        strcpy( umb->bmpImageFilename, "graphics/units.bmp" );
        RegisterButton( umb );

        x = 120;
        y = 20;
        for( int i = 0; i < 6; ++i )
        {
            char name[128];
            sprintf( name, "Remove Unit %d", i );
            RemoveUnitButton *rb = new RemoveUnitButton();
            rb->SetProperties( name, x, y, 40, 40, "", "tooltip_fleet_remove", false, true );
            rb->m_memberId = i;
            RegisterButton( rb );
            y += 50;   
        }

        FleetPlacementButton *fpb = new FleetPlacementButton();
        fpb->SetProperties( "PlaceFleet", x, y, 40, 40, "dialog_place_fleet", "tooltip_fleet_place", true, true );
        RegisterButton( fpb );

        Team *myTeam = g_app->GetWorld()->GetTeam( g_app->GetWorld()->m_myTeamId );
        if( m_currentFleetId == -1 ||
            myTeam->m_fleets[myTeam->m_fleets.Size()-1]->m_active == true )
        {
            int shipsRemaining = 0;
            shipsRemaining += myTeam->m_unitsAvailable[WorldObject::TypeBattleShip];
            shipsRemaining += myTeam->m_unitsAvailable[WorldObject::TypeCarrier];
            shipsRemaining += myTeam->m_unitsAvailable[WorldObject::TypeSub];
            if( shipsRemaining > 0 )
            {
                m_currentFleetId = myTeam->m_fleets.Size();
                g_app->GetClientToServer()->RequestFleet( myTeam->m_teamId );
            }
        }
    }
}

/*  ####################
    Unit Placement Button
    Places individual units
    ####################
*/

UnitPlacementButton::UnitPlacementButton( int unitType )
{
	m_unitType = unitType;
	m_disabled = false;
}

void UnitPlacementButton::Render( int realX, int realY, bool highlighted, bool clicked )
{ 
#ifndef NON_PLAYABLE
    SidePanel *parent = (SidePanel *)m_parent;
   
    m_disabled = false;
	Team *team          = g_app->GetWorld()->GetMyTeam();
	if( !team ) 
    {
        m_disabled = true;
        return;
    }
    else
    {
        m_disabled = false;
    }

	Image *bmpImage			= g_resource->GetImage( g_app->GetMapRenderer()->m_imageFiles[m_unitType] );
    g_renderer->SetBlendMode( Renderer::BlendModeSubtractive );
    Colour col(30,30,30,0);
    for( int x = -1; x <= 1; ++x )
    {
        for( int y = -1; y <= 1; ++y )
        {
            g_renderer->Blit( bmpImage, realX+x, realY+y, m_w, m_h, col );
        }
    }
    g_renderer->SetBlendMode( Renderer::BlendModeAdditive );
	Colour colour       = team->GetTeamColour();
	
	if( highlighted )
	{
		colour.m_b += 40;
		colour.m_g += 40;
		colour.m_r += 40;
	}
	if( clicked ) 
	{
		colour.m_b += 80;
		colour.m_g += 80;
		colour.m_r += 80;
	}
	if( team->m_unitsAvailable[m_unitType] <= 0 ||
        team->m_unitCredits < g_app->GetWorld()->GetUnitValue(m_unitType) )
	{
		m_disabled = true;
		colour = Colour(50, 50, 50, 255);
	}
    else
    {
        m_disabled = false;
    }
	
    colour.m_a = 255;
    
	//float size = 32.0f;
	if( bmpImage )
	{
		g_renderer->Blit( bmpImage, realX, realY, m_w, m_h, colour );
        g_renderer->SetBlendMode( Renderer::BlendModeNormal );
	}

    char languageString[64];
    switch( m_unitType )
    {
        case WorldObject::TypeSilo          : sprintf( languageString, "unit_silo" );       break;
        case WorldObject::TypeAirBase       : sprintf( languageString, "unit_airbase" );    break;
        case WorldObject::TypeRadarStation  : sprintf( languageString, "unit_radar" );       break;
    }

	char caption[256];
	sprintf(caption, "%s(%u)", LANGUAGEPHRASE(languageString), team->m_unitsAvailable[m_unitType]);
    
    Colour textCol = White;
    if( m_disabled )
    {
        textCol = Colour(50,50,50,255);
    }
	g_renderer->TextCentreSimple( realX + m_w/2, realY+m_h, textCol, parent->m_fontsize, caption );

    int variableTeamUnits = g_app->GetGame()->GetOptionValue("VariableUnitCounts");
    if( variableTeamUnits == 1 )
    {
        char caption[8];
        sprintf( caption, "%d", g_app->GetWorld()->GetUnitValue( m_unitType ));
        Colour col = Colour(255,255,0,255);

	    g_renderer->TextCentreSimple( realX + m_w/2, realY+m_h+14, col, parent->m_fontsize, caption );
    }

    if( highlighted || clicked )
    {
        InfoWindow *info = (InfoWindow *)EclGetWindow("Info");
        if( info )
        {
            info->SwitchInfoDisplay( m_unitType, -1 );
        }
    }
#endif
}
    


void UnitPlacementButton::MouseUp()
{
#ifndef NON_PLAYABLE
    if( g_app->GetWorld()->GetTimeScaleFactor() == 0 )
    {
        return;
    }
	if( !m_disabled )
	{
		if( !EclGetWindow( "Placement" ) )
		{
            SidePanel *parent = (SidePanel *)m_parent;
            parent->m_mode = SidePanel::ModeUnitPlacement;
			PlacementIconWindow *icon = new PlacementIconWindow( "Placement", m_unitType );
			EclRegisterWindow( icon, m_parent );

			g_app->GetMapRenderer()->m_showTeam[ g_app->GetWorld()->m_myTeamId ] = true;
            if( m_unitType == WorldObject::TypeRadarStation )
            {
                g_app->GetMapRenderer()->m_showRadar = true;
            }
            InfoWindow *info = (InfoWindow *)EclGetWindow("Info");
            if( info )
            {
                info->SwitchInfoDisplay( m_unitType, -1 );
            }
		}
		else
		{
	        EclRemoveWindow( "Placement" );
		}
	}
#endif
}

/*  ####################
    Fleet Mode Button
    Switches side panel to fleet creation mode
    ####################
*/

PanelModeButton::PanelModeButton( int mode, bool useImage )
:   m_disabled(false)
{
    m_mode = mode;
    m_imageButton = useImage;
}

void PanelModeButton::Render(int realX, int realY, bool highlighted, bool clicked)
{
#ifndef NON_PLAYABLE
    SidePanel *parent = (SidePanel *)m_parent;

    if( m_imageButton )
    {
	    Team *team          = g_app->GetWorld()->GetTeam(g_app->GetWorld()->m_myTeamId);
	    if( !team ) return;

	    Image *bmpImage			= g_resource->GetImage( bmpImageFilename );

        g_renderer->SetBlendMode( Renderer::BlendModeSubtractive );
        Colour col(30,30,30,0);
        for( int x = -1; x <= 1; ++x )
        {
            for( int y = -1; y <= 1; ++y )
            {
                g_renderer->Blit( bmpImage, realX+x, realY+y, m_w, m_h, col );
            }
        }
        g_renderer->SetBlendMode( Renderer::BlendModeAdditive );
	    Colour colour       = team->GetTeamColour();
	    
	    if( highlighted )
	    {
		    colour.m_b += 40;
		    colour.m_g += 40;
		    colour.m_r += 40;
	    }
	    if( clicked ) 
	    {
		    colour.m_b += 80;
		    colour.m_g += 80;
		    colour.m_r += 80;
	    }
        colour.m_a = 255;

        if( m_disabled )
        {
            colour = Colour(50, 50, 50, 255);
        }
	    
	    float size = 32.0f;
	    if( bmpImage )
	    {
		    g_renderer->Blit( bmpImage, realX, realY, m_w, m_h, colour );
            g_renderer->SetBlendMode( Renderer::BlendModeNormal );
	    }
        Colour textCol = White;
        if( m_disabled )
        {
            textCol = Colour(50,50,50,255);
        }

		if( m_captionIsLanguagePhrase )
		{
			g_renderer->TextCentreSimple( realX + m_w/2, realY+m_h, textCol, parent->m_fontsize, LANGUAGEPHRASE(m_caption) );
		}
		else
		{
			g_renderer->TextCentreSimple( realX + m_w/2, realY+m_h, textCol, parent->m_fontsize, m_caption );
		}
    }
    else
    {
        InterfaceButton::Render( realX, realY, highlighted, clicked );
    }
#endif
}

void PanelModeButton::MouseUp()
{
#ifndef NON_PLAYABLE
    if( g_app->GetWorld()->GetTimeScaleFactor() == 0 )
    {
        return;
    }
    if( !m_disabled )
    {
        SidePanel *parent = (SidePanel *)m_parent;
        parent->ChangeMode(m_mode);
    }
#endif
}

/*  ####################
    AddToFleet Button
    Adds a ship to the current fleet
    ####################
*/

AddToFleetButton::AddToFleetButton( int unitType )
{
	m_unitType = unitType;
	m_disabled = false;
}

void AddToFleetButton::Render( int realX, int realY, bool highlighted, bool clicked )
{ 
#ifndef NON_PLAYABLE
    SidePanel *parent = (SidePanel *)m_parent;

    Team *team = g_app->GetWorld()->GetTeam(g_app->GetWorld()->m_myTeamId);
    if( parent->m_mode == SidePanel::ModeFleetPlacement )
    {
	    m_disabled = false;
	    Image *bmpImage		= g_resource->GetImage( g_app->GetMapRenderer()->m_imageFiles[m_unitType] );
        g_renderer->SetBlendMode( Renderer::BlendModeSubtractive );
        Colour col(30,30,30,0);
        for( int x = -1; x <= 1; ++x )
        {
            for( int y = -1; y <= 1; ++y )
            {
                g_renderer->Blit( bmpImage, realX+x, realY+y, m_w, m_h, col );
            }
        }
        g_renderer->SetBlendMode( Renderer::BlendModeAdditive );
	    Colour colour       = team->GetTeamColour();
	    
	    if( highlighted )
	    {
		    colour.m_b += 40;
		    colour.m_g += 40;
		    colour.m_r += 40;
	    }
	    if( clicked ) 
	    {
		    colour.m_b += 80;
		    colour.m_g += 80;
		    colour.m_r += 80;
	    }
	    if( team->m_unitsAvailable[m_unitType] <= 0 ||
            team->m_unitCredits < g_app->GetWorld()->GetUnitValue(m_unitType))
	    {
		    m_disabled = true;
	    }
        else
        {
            m_disabled = false;
        }

        if( m_disabled )
        {
            colour = Colour(50, 50, 50, 255);
        }
    	    
	    //float size = 32.0f;
	    if( bmpImage )
	    {
		    g_renderer->Blit( bmpImage, realX, realY, m_w, m_h, colour );
            g_renderer->SetBlendMode( Renderer::BlendModeNormal );
	    }

        char languageString[64];
        switch( m_unitType )
        {
            case WorldObject::TypeBattleShip    : sprintf( languageString, "unit_battleship" ); break;
            case WorldObject::TypeCarrier       : sprintf( languageString, "unit_carrier" );    break;
            case WorldObject::TypeSub           : sprintf( languageString, "unit_sub" );        break;
        }

        char caption[256];
	    sprintf(caption, "%s(%u)", LANGUAGEPHRASE(languageString), team->m_unitsAvailable[m_unitType]);

        Colour textCol = White;
        if( m_disabled )
        {
            textCol = Colour(50,50,50,255);
        }
	    g_renderer->TextCentreSimple( realX + m_w/2, realY+m_h, textCol, parent->m_fontsize, caption );

        int variableTeamUnits = g_app->GetGame()->GetOptionValue("VariableUnitCounts");
        if( variableTeamUnits == 1 )
        {
            char caption[8];
            sprintf( caption, "%d", g_app->GetWorld()->GetUnitValue( m_unitType ));
            Colour col = Colour(255,255,0,255);

	        g_renderer->TextCentreSimple( realX + m_w/2, realY+m_h+14, col, parent->m_fontsize, caption );
        }
        if( highlighted || clicked )
        {
            InfoWindow *info = (InfoWindow *)EclGetWindow("Info");
            if( info )
            {
                info->SwitchInfoDisplay( m_unitType, -1 );
            }
        }
    }
    else
    {
        m_disabled = true;
    }
#endif
}

void AddToFleetButton::MouseUp()
{
#ifndef NON_PLAYABLE
    if( g_app->GetWorld()->GetTimeScaleFactor() == 0 )
    {
        return;
    }
    if( !m_disabled )
    {
        SidePanel *parent = (SidePanel *)m_parent;
        Team *team = g_app->GetWorld()->GetTeam(g_app->GetWorld()->m_myTeamId);

        if( parent->m_currentFleetId != -1 &&
            team->m_fleets.ValidIndex( parent->m_currentFleetId ) )
        {
            if( team->m_fleets[ parent->m_currentFleetId ]->m_memberType.Size() < 6 )
            {
                Fleet *fleet = team->m_fleets[ parent->m_currentFleetId ];
                if( g_keys[KEY_SHIFT] )
                {
                    for( int i = team->m_fleets[ parent->m_currentFleetId ]->m_memberType.Size(); i < 6; ++i )
                    {
                        if( team->m_unitsAvailable[m_unitType] > 0 &&
                            team->m_unitCredits >= g_app->GetWorld()->GetUnitValue( m_unitType ) )
                        {
                            team->m_unitsAvailable[m_unitType] --;
                            team->m_unitCredits -= g_app->GetWorld()->GetUnitValue( m_unitType );
                            fleet->m_memberType.PutData( m_unitType );
                        }
                    }

                }
                else
                {
                    team->m_unitsAvailable[m_unitType] --;
                    team->m_unitCredits -= g_app->GetWorld()->GetUnitValue( m_unitType );
                    fleet->m_memberType.PutData( m_unitType );
                }

                if( fleet->m_memberType.Size() == 6 )
                {
                    FleetPlacementButton *button = (FleetPlacementButton *)parent->GetButton("PlaceFleet");
                    button->m_disabled = false;                    
                    button->MouseUp();
                }
            }
        }
    }
#endif
}

/*  ####################
    RemoveUnit Button
    Removes the selected unit from current fleet
    ####################
*/

RemoveUnitButton::RemoveUnitButton()
:   InterfaceButton(),
    m_disabled(true)
{
}

void RemoveUnitButton::Render( int realX, int realY, bool highlighted, bool clicked )
{
    SidePanel *parent = (SidePanel *)m_parent;
    if( parent->m_mode == SidePanel::ModeFleetPlacement )
    {
        m_disabled = false;
    }
    else
    {
        m_disabled = true;
    }

    //g_renderer->RectFill( m_x, m_y, m_w, m_h, Colour(255,0,0,255) );
}

void RemoveUnitButton::MouseUp()
{
    if( g_app->GetWorld()->GetTimeScaleFactor() == 0 )
    {
        return;
    }
    if( !m_disabled )
    {
        SidePanel *parent = (SidePanel *)m_parent;
        Team *myTeam = g_app->GetWorld()->GetTeam( g_app->GetWorld()->m_myTeamId );
        if( parent->m_currentFleetId != -1 &&
            myTeam->m_fleets.ValidIndex( parent->m_currentFleetId ) )
        {
            if( myTeam->m_fleets[ parent->m_currentFleetId ]->m_memberType.ValidIndex(m_memberId) &&
                !myTeam->m_fleets[ parent->m_currentFleetId ]->m_active )
            {
                myTeam->m_unitsAvailable[ myTeam->m_fleets[ parent->m_currentFleetId ]->m_memberType[m_memberId]]++;
                myTeam->m_unitCredits += g_app->GetWorld()->GetUnitValue( myTeam->m_fleets[ parent->m_currentFleetId ]->m_memberType[m_memberId] );
                myTeam->m_fleets[ parent->m_currentFleetId ]->m_memberType.RemoveData(m_memberId);
            }
        }
    }
}

/*  ####################
    NewFleetButton
    Creates a new fleet
    ####################
*/

NewFleetButton::NewFleetButton()
:   InterfaceButton(),
    m_disabled(false)
{
}

void NewFleetButton::Render( int realX, int realY, bool highlighted, bool clicked )
{
    SidePanel *parent = (SidePanel *)m_parent;

    int transparency = g_renderer->m_alpha * 255;
    if( highlighted || clicked ) transparency = 255;
    
    Colour col = Colour(50,50,170, transparency);
    if( g_app->GetWorld()->GetTeam( g_app->GetWorld()->m_myTeamId )->m_fleets.Size() >= 9 )
    {
        m_disabled = true;
    }

    if( highlighted )   col = Colour(70,70,170, transparency);    
    if( clicked )       col = Colour(100,100,170, transparency);
    if( m_disabled )    col = Colour(100,100,100, transparency);

    
    g_renderer->RectFill ( realX, realY, m_w, m_h, col );
    g_renderer->Rect     ( realX, realY, m_w, m_h, Colour(150,150,200,transparency) );   

	if( m_captionIsLanguagePhrase )
	{
	    g_renderer->TextCentre ( realX + m_w/2, realY + m_h/5, Colour(255,255,255,transparency), parent->m_fontsize, LANGUAGEPHRASE(m_caption) );
	}
	else
	{
	    g_renderer->TextCentre ( realX + m_w/2, realY + m_h/5, Colour(255,255,255,transparency), parent->m_fontsize, m_caption );
	}
}

void NewFleetButton::MouseUp()
{
    Team *myTeam = g_app->GetWorld()->GetTeam( g_app->GetWorld()->m_myTeamId );

    if( myTeam->m_fleets.Size() < 9 )
    {
        g_app->GetClientToServer()->RequestFleet( myTeam->m_teamId );
    }
}

/*  ####################
    Fleet Placement Button
    Creates fleet placement icon
    ####################
*/

FleetPlacementButton::FleetPlacementButton()
{
	m_disabled = false;
}

void FleetPlacementButton::Render( int realX, int realY, bool highlighted, bool clicked )
{
    SidePanel *parent = (SidePanel *)m_parent;

    g_renderer->SetClip( realX, realY, m_w, m_h );
    Team *myTeam = g_app->GetWorld()->GetTeam( g_app->GetWorld()->m_myTeamId );
    int currentFleetId = parent->m_currentFleetId;
    m_disabled = false;
    if( currentFleetId == -1 ||
        !myTeam->m_fleets.ValidIndex(currentFleetId) )
    {
        m_disabled = true;
    }
    else
    {
        if( myTeam->m_fleets[ currentFleetId ]->m_memberType.Size() == 0 ||
            myTeam->m_fleets[ currentFleetId ]->m_active )
        {
            m_disabled = true;
        }
    }

    if( !m_disabled )
    {
        Colour col = myTeam->GetTeamColour();
        if( EclMouseInButton( m_parent, this ) )
        {
            col += Colour(60,60,60,0);
        }
        if( clicked )
        {
            col += Colour(60,60,60,0);
        }

        Image *img = g_resource->GetImage( "graphics/cursor_target.bmp" );
        g_renderer->SetBlendMode( Renderer::BlendModeAdditive );
        g_renderer->Blit( img, realX, realY, m_w, m_h, col );
        g_renderer->SetBlendMode( Renderer::BlendModeNormal );
    }
    g_renderer->ResetClip();
}
    


void FleetPlacementButton::MouseUp()
{
    if( g_app->GetWorld()->GetTimeScaleFactor() == 0 )
    {
        return;
    }
	if( !m_disabled )
	{
        if( !EclGetWindow( "Placement" ) )
		{
            g_app->GetMapRenderer()->m_showTeam[ g_app->GetWorld()->m_myTeamId ] = true;
            SidePanel *parent = (SidePanel *)m_parent;
			FleetPlacementIconWindow *icon = new FleetPlacementIconWindow( "Placement", parent->m_currentFleetId );
			EclRegisterWindow( icon, m_parent );
		}
		else
		{
	        EclRemoveWindow( "Placement" );
		}
		
	}
}



