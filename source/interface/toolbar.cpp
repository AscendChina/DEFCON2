#include "lib/universal_include.h"
#include "lib/gucci/window_manager.h"
#include "lib/gucci/input.h"
#include "lib/resource/resource.h"
#include "lib/language_table.h"
#include "lib/preferences.h"

#include "interface/toolbar.h"
#include "interface/alliances_window.h"
#include "interface/chat_window.h"
#include "interface/side_panel.h"
#include "interface/worldstatus_window.h"
#include "interface/info_window.h"
#include "interface/whiteboard_panel.h"

#include "renderer/map_renderer.h"

#include "app/app.h"
#include "app/globals.h"
#include "app/game.h"
#include "app/version_manager.h"


// ============================================================================


ToolbarButton::ToolbarButton()
:   InterfaceButton(),
    m_disabled(false),
    m_toggle(false)
{
    m_iconFilename[0] = '\x0';
}


void ToolbarButton::Render( int realX, int realY, bool highlighted, bool clicked )
{
    Image *image = g_resource->GetImage( m_iconFilename );
    if( image )
    {
        g_renderer->SetBlendMode( Renderer::BlendModeSubtractive );
        Colour col(100,100,100,0);
        for( int x = -1; x <= 1; ++x )
        {
            for( int y = -1; y <= 1; ++y )
            {
                g_renderer->Blit( image, realX+x, realY+y, m_w, m_h, col );
            }
        }

		if( !m_disabled )
		{
			g_renderer->SetBlendMode( Renderer::BlendModeAdditive );

			if( highlighted || clicked || m_toggle )
			{
				col.Set( 0, 0, 255, 255 );
				int size = (clicked || m_toggle) ? 2 : 1;           

				for( int x = -size; x <= size; x++ )
				{
					for( int y = -size; y <= size; y++ )
					{
						g_renderer->Blit( image, realX+x, realY+y, m_w, m_h, col );
					}
				}
			}

			if( m_toggle )
			{
				FadingWindow *parent = (FadingWindow *)m_parent;
	            
				g_renderer->RectFill( realX-2, realY-2, m_w+4, m_h+4, Colour(255,255,255,50.0f*parent->m_alpha) );
				g_renderer->Rect( realX-2, realY-2, m_w+4, m_h+4, Colour(200,200,255,200.0f*parent->m_alpha) );
			}
		}

        g_renderer->SetBlendMode( Renderer::BlendModeAdditive );
		if( !m_disabled )
		{
	        col.Set(200,200,255,255);
		}
		else
		{
	        col.Set(200,200,255,200);
		}
        g_renderer->Blit( image, realX, realY, m_w, m_h, col );

        g_renderer->SetBlendMode( Renderer::BlendModeNormal );
    }
    else
    {
        g_renderer->RectFill( realX, realY, m_w, m_h, Colour(50,50,100,100) );
    }

   
    float fontSize = (m_w/4.0f);
    if( fontSize > 5.0f )
    {
        float fontSize = (m_w/4.0f);
        if( fontSize > 5.0f )
        {
            Colour col = White;
            if( m_disabled )
            {
                col = DarkGray;
            }

			if( m_captionIsLanguagePhrase )
			{
	            g_renderer->TextCentreSimple( realX+m_w/2, realY+m_h+2, col, fontSize, LANGUAGEPHRASE(m_caption) );
			}
			else
			{
				g_renderer->TextCentreSimple( realX+m_w/2, realY+m_h+2, col, fontSize, m_caption );
			}
        }
    }
}


// ============================================================================

class PlacementWindowButton : public ToolbarButton
{
    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        m_toggle = g_app->GetWorld()->GetDefcon() > 3 &&        
                   EclGetWindow("Side Panel");
            
        if( !m_disabled )
        {
            ToolbarButton::Render( realX, realY, highlighted, clicked );
        }

        Team *team = g_app->GetWorld()->GetMyTeam();
        if( team )
        {
            int totalUnits = 0;
            for( int i = 0; i < WorldObject::NumObjectTypes; ++i )
            {
                totalUnits += team->m_unitsAvailable[i];
            }

            m_disabled = ( totalUnits == 0 );
        }
        else
        {
            m_disabled = true;
        }
    }

    void MouseUp()
    {
        if( !m_disabled )
        {
            if( EclGetWindow( "Side Panel" ) )
            {
                EclRemoveWindow( "Side Panel" );
            }
            else
            {
                SidePanel *sidePanel = new SidePanel("Side Panel");
                EclRegisterWindow( sidePanel );
            }
        }
    }
};


class ChatWindowButton : public ToolbarButton
{
public:
    int m_previousMessageCount;
    float m_timer;

    ChatWindowButton() 
    :   ToolbarButton(),
        m_previousMessageCount(0),
        m_timer(0.0f)
    {
        m_previousMessageCount = CountVisableMessages();
    }

    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        if( EclGetWindow( "Comms Window" ) )
        {
            m_previousMessageCount = CountVisableMessages();
            m_timer = 0.0f;
            m_toggle = true;
            ToolbarButton::Render( realX, realY, highlighted, clicked );
        }
        else
        {
            m_toggle = false;
            if( CountVisableMessages() > m_previousMessageCount )
            {
                m_timer += SERVER_ADVANCE_PERIOD.DoubleValue();
                ToolbarButton::Render( realX, realY, highlighted, (m_timer > 0.0f ) );
                if( m_timer > 2.0f )
                {
                    m_timer = -2.0f;
                }
            }
            else
            {
                ToolbarButton::Render( realX, realY, highlighted, clicked );
            }
        }
    }
    void MouseUp()
    {
        if( EclGetWindow( "Comms Window" ) )
        {
            EclRemoveWindow( "Comms Window" );
        }
        else
        {
            EclRegisterWindow( new ChatWindow() );
        }
    }

    int CountVisableMessages()
    {
        int num = 0;
        int myTeamId = g_app->GetWorld()->m_myTeamId;
        for( int i = 0; i < g_app->GetWorld()->m_chat.Size(); ++i )
        {
            ChatMessage *msg = g_app->GetWorld()->m_chat[i];
            if( msg->m_visible )
            {
                num++;
            }
        }
        return num;
    }
};


class ShowAlliancesButton : public ToolbarButton
{
    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        m_toggle = EclGetWindow( "Alliances" );
        
        ToolbarButton::Render( realX, realY, highlighted, clicked );
    }
    void MouseUp()
    {
        if( EclGetWindow( "Alliances" ) )
        {
            EclRemoveWindow( "Alliances" );
        }
        else
        {
            EclRegisterWindow( new AlliancesWindow() );
        }
    }
};


class ScoresWindowButton : public ToolbarButton
{
    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        m_toggle = EclGetWindow( "Scores" );

        ToolbarButton::Render( realX, realY, highlighted, clicked );
    }
    void MouseUp()
    {
        if( EclGetWindow( "Scores" ) )
        {
            EclRemoveWindow( "Scores" );
        }
        else
        {
            EclRegisterWindow( new ScoresWindow() );
        }
    }
};

class InfoWindowButton : public ToolbarButton
{
    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        m_toggle = EclGetWindow( "Info" );
        
        ToolbarButton::Render( realX, realY, highlighted, clicked );
    }
    void MouseUp()
    {
        if( EclGetWindow( "Info" ) )
        {
            EclRemoveWindow( "Info" );
        }
        else
        {
            EclRegisterWindow( new InfoWindow() );
        }
    }
};


class ToggleBoolButton : public ToolbarButton
{
public:
    bool *m_value;
    bool m_hoverToggle;
    bool m_justTurnedOff;

    ToggleBoolButton()
        : ToolbarButton(),
        m_hoverToggle(false)
    {
    }

    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        m_toggle = (*m_value && !m_hoverToggle);
        
        ToolbarButton::Render( realX, realY, highlighted, clicked );

        //
        // Mouse in this button?

        float mouseX = g_inputManager->m_mouseX;
        float mouseY = g_inputManager->m_mouseY;

        if( mouseX >= realX && mouseY >= realY &&
            mouseX <= realX+m_w &&
            mouseY <= realY+m_h )
        {
            if( !(*m_value) && !m_justTurnedOff )
            {
                *m_value = true;
                m_hoverToggle = true;
            }
        }
        else
        {
            m_justTurnedOff = false;
            if( m_hoverToggle )
            {
                *m_value = false;
                m_hoverToggle = false;
            }
        }    
    }
    void MouseUp()
    {
        if( m_hoverToggle )
        {
            *m_value = false;
            m_hoverToggle = false;
        }

        if( *m_value ) 
        {
            *m_value = false;
            Toolbar *parent = (Toolbar *)m_parent;
            m_justTurnedOff = true;
        }
        else 
        {
            *m_value = true;        
        }
    }
};


class WhiteBoardButton : public ToggleBoolButton
{
public:
	bool m_toggleValue;

	WhiteBoardButton() :
		ToggleBoolButton(),
		m_toggleValue(false)
	{
		m_value = &m_toggleValue;
	}

    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
		if ( !m_disabled )
		{
			m_toggleValue = g_app->GetMapRenderer()->GetShowWhiteBoard();
	        
			ToggleBoolButton::Render( realX, realY, highlighted, clicked );

			g_app->GetMapRenderer()->SetShowWhiteBoard ( m_toggleValue );
		}
		else
		{
			ToggleBoolButton::Render( realX, realY, highlighted, clicked );
		}
    }

    void MouseUp()
    {
		if ( !m_disabled )
		{
			ToggleBoolButton::MouseUp();

			g_app->GetMapRenderer()->SetShowWhiteBoard ( m_toggleValue );
			g_app->GetMapRenderer()->SetEditWhiteBoard ( !g_app->GetMapRenderer()->GetEditWhiteBoard() );

			if ( g_app->GetMapRenderer()->GetEditWhiteBoard() )
			{
				if( !EclGetWindow( "WhiteBoardPanel" ) )
				{
					EclRegisterWindow( new WhiteBoardPanel( m_w, m_w * 1.2f ) );
				}
			}
			else
			{
				if( EclGetWindow( "WhiteBoardPanel" ) )
				{
					EclRemoveWindow( "WhiteBoardPanel" );
				}
			}
		}
    }
};


// ============================================================================


Toolbar::Toolbar()
:   FadingWindow( "Toolbar" ),
    m_numButtons(10)
{
    bool chatWindow = false;
    for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
    {
        if( g_app->GetWorld()->m_teams[i]->m_type == Team::TypeRemotePlayer )
        {
            chatWindow = true;
        }
    }
    if( g_app->GetGame()->GetOptionValue("MaxSpectators") > 0 )
    {
        chatWindow = true;
    }

    if( chatWindow )
    {
        m_numButtons++;
    }

#ifdef NON_PLAYABLE
	m_numButtons -= 2;
#endif

	SetSize( 57 * m_numButtons + 10, 65 );
    SetPosition( g_windowManager->WindowW() - m_w, 
                 g_windowManager->WindowH() - m_h );

    LoadProperties( "WindowToolbar" );
}


Toolbar::~Toolbar()
{
    SaveProperties( "WindowToolbar" );
}


void Toolbar::Create()
{
    bool chatWindow = false;
    for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
    {
        if( g_app->GetWorld()->m_teams[i]->m_type == Team::TypeRemotePlayer )
        {
            chatWindow = true;
        }
    }
    if( g_app->GetGame()->GetOptionValue("MaxSpectators") > 0 )
    {
        chatWindow = true;
    }

    float gap = (m_w-10)/(float)m_numButtons;
    float iconSize = gap;
    if( iconSize > m_h - 15 ) iconSize = m_h - 15;
	iconSize /= 1.2f;
    float x = 5 + ( gap - iconSize ) / 2.0f;
    float y = m_h/2 - iconSize/2;

#ifndef NON_PLAYABLE
    int isSpectator = g_app->GetWorld()->IsSpectating( g_app->GetClientToServer()->m_clientId );
    if( isSpectator == -1 )
    {
		PlacementWindowButton *placement = new PlacementWindowButton();
        placement->SetProperties( "Placement", x, y, iconSize, iconSize, "dialog_toolbar_units", "tooltip_toolbar_units", true, true );
        strcpy(placement->m_iconFilename, "gui/tb_units.bmp" );
        RegisterButton( placement );

        ShowAlliancesButton *showAlliances = new ShowAlliancesButton();
        showAlliances->SetProperties( "Alliances", x+=gap, y, iconSize, iconSize, "dialog_toolbar_allies", "tooltip_toolbar_alliances", true, true );
        strcpy(showAlliances->m_iconFilename, "gui/tb_alliances.bmp" );
        RegisterButton( showAlliances );
    }
#endif

    if( chatWindow )
    {
        ChatWindowButton *chatWindow = new ChatWindowButton();
        chatWindow->SetProperties( "Chat", x+=gap, y, iconSize, iconSize, "dialog_toolbar_comms", "tooltip_toolbar_comms", true, true );
        strcpy(chatWindow->m_iconFilename, "gui/tb_comms.bmp" );
        RegisterButton( chatWindow );
    }

    ScoresWindowButton *scores = new ScoresWindowButton();
    scores->SetProperties( "Scores", x+=gap, y, iconSize, iconSize, "dialog_toolbar_scores", "tooltip_toolbar_score", true, true );
    strcpy(scores->m_iconFilename, "gui/tb_scores.bmp" );
    RegisterButton( scores );

    ToggleBoolButton *radar = new ToggleBoolButton();
    radar->SetProperties( "Radar", x+=gap, y, iconSize, iconSize, "dialog_toolbar_radar", "tooltip_toolbar_radar", true, true );  
    radar->m_value = &g_app->GetMapRenderer()->m_showRadar;
    strcpy(radar->m_iconFilename, "gui/tb_radar.bmp");
    RegisterButton( radar );

    ToggleBoolButton *population = new ToggleBoolButton();
    population->SetProperties( "Population", x+=gap, y, iconSize, iconSize, "dialog_toolbar_people", "tooltip_toolbar_pop", true, true );
    population->m_value = &g_app->GetMapRenderer()->m_showPopulation;
    strcpy(population->m_iconFilename, "gui/tb_population.bmp");
    RegisterButton( population );

    ToggleBoolButton *orders = new ToggleBoolButton();
    orders->SetProperties( "Orders", x+=gap, y, iconSize, iconSize, "dialog_toolbar_orders", "tooltip_toolbar_orders", true, true );
    orders->m_value = &g_app->GetMapRenderer()->m_showOrders;
    strcpy(orders->m_iconFilename, "gui/tb_orders.bmp");
    RegisterButton( orders );

    ToggleBoolButton *territory = new ToggleBoolButton();
    territory->SetProperties( "Territory", x+=gap, y, iconSize, iconSize, "dialog_toolbar_territory", "tooltip_toolbar_territory", true, true );
    territory->m_value = &g_app->GetMapRenderer()->m_showAllTeams;
    strcpy(territory->m_iconFilename, "gui/tb_territory.bmp");
    RegisterButton( territory );

    ToggleBoolButton *nukes = new ToggleBoolButton();
    nukes->SetProperties( "Nukes", x+=gap, y, iconSize, iconSize, "dialog_toolbar_nukes", "tooltip_toolbar_nukes", true, true );
    nukes->m_value = &g_app->GetMapRenderer()->m_showNukeUnits;
    strcpy(nukes->m_iconFilename, "gui/tb_nukes.bmp");
    RegisterButton( nukes );

#ifndef NON_PLAYABLE
    if( isSpectator == -1 )
    {
        bool supportWhiteBoard = VersionManager::DoesSupportWhiteBoard( g_app->GetClientToServer()->m_serverVersion );
        WhiteBoardButton *whiteBoard = new WhiteBoardButton();
        if ( supportWhiteBoard )
        {
            whiteBoard->SetProperties( "WhiteBoard", x+=gap, y, iconSize, iconSize, "dialog_toolbar_white_board", "tooltip_toolbar_whiteboard", true, true );
        }
        else
        {
            whiteBoard->SetProperties( "WhiteBoard", x+=gap, y, iconSize, iconSize, "dialog_toolbar_white_board", "tooltip_toolbar_whiteboard_disabled", true, true );
            whiteBoard->m_disabled = true;
        }
        strcpy(whiteBoard->m_iconFilename, "gui/tb_board.bmp" );
        RegisterButton( whiteBoard );
    }
#endif

    InfoWindowButton *infoWindow = new InfoWindowButton();
    infoWindow->SetProperties( "Info", x+=gap, y, iconSize, iconSize, "dialog_toolbar_info", "tooltip_toolbar_info", true, true );
    strcpy(infoWindow->m_iconFilename, "gui/tb_info.bmp" );
    RegisterButton( infoWindow );
}


void Toolbar::Render( bool _hasFocus )
{
    FadingWindow::Render( _hasFocus, true );    
}



