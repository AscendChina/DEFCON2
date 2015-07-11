#include "lib/universal_include.h"
#include "lib/gucci/input.h"
#include "lib/gucci/window_manager.h"
#include "lib/render/colour.h"
#include "lib/resource/resource.h"
#include "lib/language_table.h"
#include "lib/sound/soundsystem.h"

#include "app/app.h"
#include "app/globals.h"

#include "world/world.h"
#include "world/team.h"

#include "network/ClientToServer.h"

#include "renderer/map_renderer.h"

#include "interface/interface.h"
#include "interface/fleet_placement_icon.h"
#include "interface/side_panel.h"

#include "world/silo.h"
#include "world/sub.h"
#include "world/radarstation.h"
#include "world/battleship.h"
#include "world/airbase.h"
#include "world/carrier.h"
#include "world/fleet.h"


FleetPlacementIconWindow::FleetPlacementIconWindow( char *name, int fleetId )
:   InterfaceWindow( name )
{
	m_fleetId = fleetId;
    SetSize( 128, 128 );
}


void FleetPlacementIconWindow::Render( bool hasFocus )
{
	m_x = g_inputManager->m_mouseX - 80;
    m_y = g_inputManager->m_mouseY - 80;
           
    EclWindow::Render( hasFocus );       
}

void FleetPlacementIconWindow::Create()
{
	int x = 64;
    int y = 64;
    int w = 32;
    int h = 32;

    FleetPlacementIconButton *icon = new FleetPlacementIconButton(m_fleetId);
    icon->SetProperties( "Icon", x, y, w, h, "Icon", "", false, false );
    RegisterButton( icon );
}


// UnitPlacementButton class implementation
// Used to draw image buttons for unit placement
FleetPlacementIconButton::FleetPlacementIconButton( int fleetId )
{
	m_fleetId = fleetId;
}

void FleetPlacementIconButton::Render( int realX, int realY, bool highlighted, bool clicked )
{ 
    Team *team          = g_app->GetWorld()->GetTeam(g_app->GetWorld()->m_myTeamId);
    if( !team ) return;

	Colour colour       = team->GetTeamColour();

    float longitude = 0.0f;
    float latitude = 0.0f;
    
    g_app->GetMapRenderer()->ConvertPixelsToAngle( g_inputManager->m_mouseX, g_inputManager->m_mouseY, &longitude, &latitude );
    if( !team->m_fleets[m_fleetId]->ValidFleetPlacement( Fixed::FromDouble(longitude), Fixed::FromDouble(latitude) ) )
    {
        colour = Colour(50,50,50,200);

        g_app->GetMapRenderer()->m_tooltip->PutData( LANGUAGEPHRASE("tooltip_invalidplacement") );
    }
    else
    {
        g_app->GetMapRenderer()->m_tooltip->PutData( LANGUAGEPHRASE("tooltip_placefleet") );
    }

    g_app->GetMapRenderer()->m_tooltip->PutData( LANGUAGEPHRASE("tooltip_spacetocancel") );


    for( int i = 0; i < team->m_fleets[m_fleetId]->m_memberType.Size(); ++i )
    {
        int unitType = team->m_fleets[m_fleetId]->m_memberType[i];
        bmpImage = g_resource->GetImage( g_app->GetMapRenderer()->m_imageFiles[unitType] );
        float size = 32.0f;

        Fixed exactX = 0;
        Fixed exactY = 0;
        float x, y;

        /*switch(i)
        {
            case 0  :   x = realX; y = realY - 32;           break;
            case 1  :   x = realX; y = realY + 32;      break;
            case 2  :   x = realX + 32; y = realY - 16; break;
            case 3  :   x = realX - 32; y = realY - 16; break;
            case 4  :   x = realX + 32; y = realY + 16; break;
            case 5  :   x = realX - 32; y = realY + 16; break;
        }*/

        team->m_fleets[m_fleetId]->GetFormationPosition( team->m_fleets[m_fleetId]->m_memberType.Size(), i, &exactX, &exactY );
		x = exactX.DoubleValue();
		y = exactY.DoubleValue();
        x *= 10.667;
        y *= -10.667;

        x+= realX;
        y+= realY;
	    
	    if( bmpImage )
	    {
            g_renderer->SetBlendMode( Renderer::BlendModeAdditive );
		    g_renderer->Blit( bmpImage, x, y, size, size, colour );
            g_renderer->SetBlendMode( Renderer::BlendModeNormal );
	    }
    }
    
    if( g_keys[KEY_SPACE] &&
        g_app->GetWorld()->m_myTeamId != -1 )
    {
        g_app->GetMapRenderer()->m_showTeam[ g_app->GetWorld()->m_myTeamId ] = false;
        EclRemoveWindow( "Placement" );
    }
}
    
void FleetPlacementIconButton::MouseUp()
{
	float longitude, latitude;
	Fixed exactLongitude, exactLatitude;

    g_app->GetMapRenderer()->ConvertPixelsToAngle( g_inputManager->m_mouseX, 
                                                   g_inputManager->m_mouseY, 
                                                   &longitude, &latitude );
	exactLongitude = Fixed::FromDouble(longitude);
	exactLatitude = Fixed::FromDouble(latitude);

    Team *team = g_app->GetWorld()->GetTeam(g_app->GetWorld()->m_myTeamId);
    team->m_fleets[m_fleetId]->m_teamId = team->m_teamId;
	
    if( team->m_fleets[m_fleetId]->ValidFleetPlacement( exactLongitude, exactLatitude ) )
    {
        for( int i = 0; i < team->m_fleets[m_fleetId]->m_memberType.Size(); ++i )
        {
            g_app->GetMapRenderer()->ConvertPixelsToAngle( g_inputManager->m_mouseX, 
                                                           g_inputManager->m_mouseY, 
                                                           &longitude, &latitude );
            Fixed thisLong = exactLongitude;
            Fixed thisLat = exactLatitude;

            team->m_fleets[m_fleetId]->GetFormationPosition( team->m_fleets[m_fleetId]->m_memberType.Size(), i, &thisLong, &thisLat );
            if( thisLong > 180 )
            {
                thisLong -= 360;
            }
            else if( thisLong < -180 )
            {
                thisLong += 360;
            }

            team->m_unitsAvailable[ team->m_fleets[m_fleetId]->m_memberType[i] ]++;
            team->m_unitCredits += g_app->GetWorld()->GetUnitValue( team->m_fleets[m_fleetId]->m_memberType[i] );
            g_app->GetClientToServer()->RequestPlacement( team->m_teamId, team->m_fleets[m_fleetId]->m_memberType[i],
                                                          thisLong, thisLat, m_fleetId );
        }
        team->m_fleets[m_fleetId]->m_longitude = exactLongitude;
        team->m_fleets[m_fleetId]->m_latitude = exactLatitude;
        team->m_fleets[m_fleetId]->m_active = true;
        g_app->GetMapRenderer()->m_showTeam[ g_app->GetWorld()->m_myTeamId ] = false;        
    	EclRemoveWindow( "Placement" );
        SidePanel *sidepanel = (SidePanel *)EclGetWindow("Side Panel");
        sidepanel->ChangeMode(SidePanel::ModeUnitPlacement);
        sidepanel->m_currentFleetId = -1;
    }
    else
    {
        g_soundSystem->TriggerEvent( "Interface", "Error" );
    }
}
