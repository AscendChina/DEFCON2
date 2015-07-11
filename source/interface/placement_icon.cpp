#include "lib/universal_include.h"
#include "lib/gucci/input.h"
#include "lib/resource/resource.h"
#include "lib/preferences.h"
#include "lib/language_table.h"
#include "lib/sound/soundsystem.h"

#include "app/app.h"
#include "app/globals.h"

#include "world/world.h"
#include "world/team.h"

#include "network/ClientToServer.h"

#include "renderer/map_renderer.h"

#include "interface/interface.h"
#include "interface/placement_icon.h"

#include "world/silo.h"
#include "world/sub.h"
#include "world/radarstation.h"
#include "world/battleship.h"
#include "world/airbase.h"
#include "world/carrier.h"


PlacementIconWindow::PlacementIconWindow( char *name, int unitType )
:   InterfaceWindow( name )
{
	m_unitType = unitType;
    SetSize( 50, 50 );
}


void PlacementIconWindow::Render( bool hasFocus )
{
	m_x = g_inputManager->m_mouseX - m_w/2;
    m_y = g_inputManager->m_mouseY - m_h/2;

    EclWindow::Render( hasFocus );      
}

void PlacementIconWindow::Create()
{
	int x = 0;
    int y = 0;
    int w = m_w;
    int h = m_h;

    PlacementIconButton *icon = new PlacementIconButton(m_unitType);
    icon->SetProperties( "Icon", x, y, w, h, "Icon", " ", false, false );
    RegisterButton( icon );
}


// UnitPlacementButton class implementation
// Used to draw image buttons for unit placement
PlacementIconButton::PlacementIconButton( int unitType )
{
	m_unitType = unitType;
}

void PlacementIconButton::Render( int realX, int realY, bool highlighted, bool clicked )
{ 
	bmpImage			= g_resource->GetImage( g_app->GetMapRenderer()->m_imageFiles[m_unitType] );
    Team *team          = g_app->GetWorld()->GetTeam(g_app->GetWorld()->m_myTeamId);
	Colour colour       = team->GetTeamColour();

    float longitude = 0.0f;
    float latitude = 0.0f;
    
    g_app->GetMapRenderer()->ConvertPixelsToAngle( g_inputManager->m_mouseX, g_inputManager->m_mouseY, &longitude, &latitude );
    if( !g_app->GetWorld()->IsValidPlacement( team->m_teamId, Fixed::FromDouble(longitude),
															  Fixed::FromDouble(latitude), m_unitType ) )
    {
        colour.Set(10,10,10,255);
        g_app->GetMapRenderer()->m_tooltip->PutData( LANGUAGEPHRASE("tooltip_invalidplacement") );
    }
    else
    {
        g_app->GetMapRenderer()->m_tooltip->PutData( LANGUAGEPHRASE("tooltip_placeobject") );
    }

    g_app->GetMapRenderer()->m_tooltip->PutData( LANGUAGEPHRASE("tooltip_spacetocancel") );
    

    if( team->m_unitsAvailable[m_unitType] == 0 ||
        team->m_unitCredits < g_app->GetWorld()->GetUnitValue(m_unitType) )
    {
        g_app->GetMapRenderer()->m_showTeam[ g_app->GetWorld()->m_myTeamId ] = false;
        if( m_unitType == WorldObject::TypeRadarStation )
        {
            g_app->GetMapRenderer()->m_showRadar = false;
        }
		EclRemoveWindow( "Placement" );
        return;
    }

	float size = m_w;
	
	if( bmpImage )
	{
        g_renderer->SetBlendMode( Renderer::BlendModeAdditive );
		g_renderer->Blit( bmpImage, realX, realY, size, size, colour );
        g_renderer->SetBlendMode( Renderer::BlendModeNormal );
	}


    int numRemaining = team->m_unitsAvailable[m_unitType];
    
    g_renderer->SetFont( "kremlin", false );
    g_renderer->Text( realX-5, realY, White, 20, "%d", numRemaining );
    g_renderer->SetFont();

    if( g_keys[KEY_SPACE] )
    {
        g_app->GetMapRenderer()->m_showTeam[ g_app->GetWorld()->m_myTeamId ] = false;
        if( m_unitType == WorldObject::TypeRadarStation )
        {
            g_app->GetMapRenderer()->m_showRadar = false;
        }
        EclRemoveWindow( "Placement" );
    }
}
    
void PlacementIconButton::MouseUp()
{
    EclWindow *panel = EclGetWindow("Side Panel");
    if( panel && EclMouseInWindow(panel) ) 
    {
        EclRemoveWindow( "Placement" );
        return;
    }

	float longitude;
	float latitude;

	g_app->GetMapRenderer()->
		ConvertPixelsToAngle( g_inputManager->m_mouseX, 
							  g_inputManager->m_mouseY, 
							  &longitude, &latitude );

	int teamId = g_app->GetWorld()->m_myTeamId;

    if( g_app->GetWorld()->IsValidPlacement( teamId, Fixed::FromDouble(longitude), Fixed::FromDouble(latitude), m_unitType ) )
    {
        g_app->GetClientToServer()->RequestPlacement( teamId, m_unitType, Fixed::FromDouble(longitude), Fixed::FromDouble(latitude) );

	    if(EclMouseInWindow( EclGetWindow("Side Panel") ))
	    {
            g_app->GetMapRenderer()->m_showTeam[ g_app->GetWorld()->m_myTeamId ] = false;
            if( m_unitType == WorldObject::TypeRadarStation )
            {
                g_app->GetMapRenderer()->m_showRadar = false;
            }
	    }
    }
    else
    {
        g_soundSystem->TriggerEvent( "Interface", "Error" );
    }
}


