#include "lib/universal_include.h"

#include <time.h>

#include "lib/resource/image.h"
#include "lib/render/renderer.h"
#include "lib/netlib/net_lib.h"
#include "lib/debug_utils.h"
#include "lib/hi_res_time.h"
#include "lib/tosser/directory.h"
#include "lib/metaserver/metaserver.h"
#include "lib/metaserver/authentication.h"
#include "lib/metaserver/metaserver_defines.h"
#include "lib/metaserver/matchmaker.h"
#include "lib/language_table.h"
#include "lib/gucci/input.h"
#include "lib/gucci/window_manager.h"
#include "lib/math/math_utils.h"
#include "lib/preferences.h"
#include "lib/string_utils.h"

#include "app/app.h"
#include "app/globals.h"
#include "app/game.h"
#include "lib/multiline_text.h"

#include "renderer/map_renderer.h"

#include "network/Server.h"
#include "network/ServerToClient.h"
#include "network/network_defines.h"

#include "world/world.h"

#include "interface/components/inputfield.h"
#include "interface/components/drop_down_menu.h"
#include "interface/components/message_dialog.h"
#include "interface/demo_window.h"
#include "interface/badkey_window.h"

#include "chat_window.h"
#include "lobby_window.h"


// ============================================================================


class LobbyOptionsButton : public TextButton
{
public:
    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        if( highlighted )
        {
            g_renderer->RectFill( realX, realY, m_w, m_h, Colour(100,100,200,100) );
        }

        if( strcmp(EclGetCurrentButton(), m_name) == 0 )
        {
            g_renderer->RectFill( realX, realY, m_w, m_h, Colour(100,100,200,200) );
            TextButton::Render( realX, realY, highlighted, true );
        }
        else
        {
            TextButton::Render( realX, realY, highlighted, clicked);
        }
    }
};


class RequestJoinSpectatorsButton : public LobbyOptionsButton
{
public:
    bool IsVisible()
    {
        if( g_app->GetClientToServer() &&
            g_app->GetClientToServer()->IsConnected() )
        {
            bool spectating = g_app->GetWorld()->AmISpectating();

            if( !spectating )
            {
                int numSpectators = g_app->GetWorld()->m_spectators.Size();
                int maxSpectators = g_app->GetGame()->GetOptionValue( "MaxSpectators" );

                if( numSpectators < maxSpectators )
                {
                    return true;
                }
            }
        }

        return false;
    }

    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        if( IsVisible() )
        {
            LobbyOptionsButton::Render( realX, realY, highlighted, clicked );
        }
    }

    void MouseUp() 
    {
        if( IsVisible() )
        {
            g_app->GetClientToServer()->RequestSpectate();
            EclRemoveWindow( m_parent->m_name );
        }
    }
};


class RequestJoinGameButton : public InterfaceButton
{
public:
    bool IsVisible()
    {
        if( g_app->GetClientToServer() &&
            g_app->GetClientToServer()->IsConnected() )
        {
            bool spectating = g_app->GetWorld()->AmISpectating();

            if( spectating )
            {
                int numPlayers = g_app->GetWorld()->m_teams.Size();
                int maxPlayers = g_app->GetGame()->GetOptionValue( "MaxTeams" );

                if( numPlayers < maxPlayers )
                {
                    return true;
                }
            }
        }

        return false;
    }

    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        if( IsVisible() )
        {
            InterfaceButton::Render( realX, realY, highlighted, clicked );
        }
    }

    void MouseUp()
    {
        if( IsVisible() )
        {
            g_app->GetClientToServer()->RequestTeam( Team::TypeLocalPlayer );
        }
    }
};


class RenameButton : public LobbyOptionsButton
{
    void MouseUp()
    {
        TeamOptionsWindow *parent = (TeamOptionsWindow *)m_parent;
        Team *team = g_app->GetWorld()->GetTeam( parent->m_teamId );
        if( team )
        {
            EclRegisterWindow( new RenameTeamWindow(parent->m_teamId) );
        }
        
        EclRemoveWindow( m_parent->m_name );
    }
};


class KickTeamButton : public LobbyOptionsButton
{
    void MouseUp()
    {
        TeamOptionsWindow *parent = (TeamOptionsWindow *)m_parent;
        Team *team = g_app->GetWorld()->GetTeam( parent->m_teamId );
        if( team )
        {
            if( team->m_type == Team::TypeAI )
            {
                g_app->GetClientToServer()->RequestRemoveAI( team->m_teamId );
            }
            else
            {
                g_app->GetServer()->RemoveClient( team->m_clientId, Disconnect_KickedFromGame );
            }
        }

        EclRemoveWindow( m_parent->m_name );
    }
};


class CancelLobbyOptionsButton : public LobbyOptionsButton
{
    void MouseUp()
    {
        EclRemoveWindow( m_parent->m_name );
    }
};


class TeamAllianceButton : public LobbyOptionsButton
{
public:
    int m_allianceId;

    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        TeamOptionsWindow *parent = (TeamOptionsWindow *)m_parent;
        Team *team = g_app->GetWorld()->GetTeam( parent->m_teamId );

        if( team && team->m_allianceId == m_allianceId )
        {
            g_renderer->RectFill( realX, realY, m_w, m_h, Colour(100,100,200,200) );
            g_renderer->Rect( realX, realY, m_w, m_h, Colour(255,255,255,155) );
        }

        LobbyOptionsButton::Render( realX, realY, highlighted, clicked );

        Colour allianceCol = g_app->GetWorld()->GetAllianceColour( m_allianceId );

        allianceCol.m_a = 255;
        Colour darker = allianceCol;
        darker.m_r *= 0.3f;
        darker.m_g *= 0.3f;
        darker.m_b *= 0.3f;

        float colourX = realX + 90;
        float colourY = realY + 3;
        float colourW = m_w - 95;
        float colourH = m_h - 6;

        g_renderer->SetBlendMode( Renderer::BlendModeNormal );
        g_renderer->RectFill( colourX, colourY, colourW, colourH, allianceCol, darker, false );
        g_renderer->Rect( colourX, colourY, colourW, colourH, Colour(255,255,255,55), 0.5f );
    }

    void MouseUp()
    {
        TeamOptionsWindow *parent = (TeamOptionsWindow *)m_parent;
        g_app->GetClientToServer()->RequestAlliance( parent->m_teamId, m_allianceId );
        
        EclRemoveWindow( parent->m_name );
    }
};


TeamOptionsWindow::TeamOptionsWindow( int teamId )
:   InterfaceWindow("TeamOptions", "dialog_lobby_team_options", true),
    m_teamId(teamId)
{
    SetSize( 140, 150 );
}

void TeamOptionsWindow::Create()
{
    float yPos = 22;

    Team *team = g_app->GetWorld()->GetTeam( m_teamId );
    if( !team ) return;


    //
    // Rename button

    if( team->m_type == Team::TypeLocalPlayer ||
        (team->m_type == Team::TypeAI && g_app->GetServer() ) )
    {
        RenameButton *rename = new RenameButton();
        rename->SetProperties( "Rename", 2, yPos, m_w-4, 17, "dialog_rename", "tooltip_lobby_teamname", true, true );
        RegisterButton( rename );

        yPos += 18;
    }
    
    //
    // Kick button

    if( g_app->GetServer() &&
        m_teamId != g_app->GetWorld()->m_myTeamId &&
        team->m_type != Team::TypeUnassigned )
    {
        KickTeamButton *ktb = new KickTeamButton();
        ktb->SetProperties( "Kick", 2, yPos, m_w-4, 17, "dialog_kick", " ", true, false );
        RegisterButton( ktb );

        yPos += 18;
    }

    //
    // Join spectators

    if( m_teamId == g_app->GetWorld()->m_myTeamId )
    {
        RequestJoinSpectatorsButton *join = new RequestJoinSpectatorsButton();
		join->SetProperties( "Spectate", 2, yPos, m_w-4, 17, "dialog_spectate", " ", true, false );
        RegisterButton(join);

        yPos += 18;
    }


    //
    // Alliance colour buttons

    if( g_app->GetGame()->GetOptionValue("GameMode") != GAMEMODE_DIPLOMACY )
    {
        if( m_teamId == g_app->GetWorld()->m_myTeamId ||
            (g_app->GetServer() && team->m_type == Team::TypeAI ) )
        {
            yPos += 9;
                            
            for( int i = 0; i < 6; ++i )
            {
                float xPos = 2;
                float w = m_w - 4;

                TeamAllianceButton *rtb = new TeamAllianceButton();
                rtb->m_allianceId = i;
                char name[64];
                sprintf( name, "alliance %d", i + 1 );

                char caption[256];
				sprintf( caption, LANGUAGEPHRASE("dialog_join_alliance") );
				LPREPLACESTRINGFLAG( 'A', g_app->GetWorld()->GetAllianceName(i), caption );

				rtb->SetProperties( name, xPos, yPos, w, 17, caption, " ", false, false );
                RegisterButton( rtb );        
                yPos += 20;
            }
        }
    }
    

    //
    // Cancel button

    yPos += 9;

    CancelLobbyOptionsButton *close = new CancelLobbyOptionsButton();
    close->SetProperties( "Cancel", 2, yPos, m_w-4, 17, "dialog_cancel", " ", true, false );
    RegisterButton( close );    

    yPos += 18;

    m_h = yPos + 2;
}


void TeamOptionsWindow::Update()
{
    Team *team = g_app->GetWorld()->GetTeam( m_teamId );
    if( !team ) 
    {
        EclRemoveWindow(m_name);
    }

    if( strcmp( EclGetCurrentFocus(), m_name ) != 0 )
    {
        EclRemoveWindow(m_name);
    }
}


// ============================================================================


class TeamButton : public EclButton
{
public:
    int m_teamIndex;
    
    bool TeamOptionsAvailable()
    {
        LobbyWindow *lobby = (LobbyWindow *) m_parent;
        int teamId = lobby->m_teamOrder[m_teamIndex];
        Team *team = g_app->GetWorld()->GetTeam(teamId);

        if( team->m_type != Team::TypeUnassigned &&
            (team->m_type == Team::TypeLocalPlayer ||
             g_app->GetServer()) )
        {
            return true;
        }

        return false;
    }

    void GetTeamOptionsButton( int &_x, int &_y, int &_w, int &_h )
    {
        _x = m_w - 25;
        _y = m_h/2 - 7;
        _w = 25;
        _h = 15;
    }

    bool MouseInTeamOptionsButton()
    {
        int optionsX, optionsY, optionsW, optionsH;
        GetTeamOptionsButton( optionsX, optionsY, optionsW, optionsH );

        return( g_inputManager->m_mouseX >= m_parent->m_x + m_x + optionsX &&
                g_inputManager->m_mouseX <= m_parent->m_x + m_x + optionsX + optionsW &&
                g_inputManager->m_mouseY >= m_parent->m_y + m_y + optionsY &&
                g_inputManager->m_mouseY <= m_parent->m_y + m_y + optionsY + optionsH );

    }

    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        if( !g_app->GetClientToServer()->IsConnected() ) 
        {
            return;
        }

        //
        // Start rendering

        LobbyWindow *lobby = (LobbyWindow *) m_parent;

        g_renderer->RectFill( realX, realY, m_w, m_h, Colour(15,15,15,100) );            
        g_renderer->Rect( realX, realY, m_w, m_h, Colour(255,255,255,30), 0.5f );

        if( highlighted && g_app->GetServer() )
        {
            g_renderer->Rect( realX, realY, m_w, m_h, Colour(255,255,255,100), 1 );
        }

        int maxTeams = g_app->GetGame()->GetOptionValue("MaxTeams");

        int maxDemoSize, maxDemoPlayers;
        bool allowDemoServers;
        g_app->GetClientToServer()->GetDemoLimitations( maxDemoSize, maxDemoPlayers, allowDemoServers );

        if( m_teamIndex >= maxTeams )
        {
            // This is a closed slot
            Colour col(255,255,255,50);
            g_renderer->TextSimple( realX + 10, realY + m_h/2 - 6, col, 12, LANGUAGEPHRASE("dialog_closed") );
                    
            if( g_app->GetServer() )
            {
                if( g_app->GetClientToServer()->AmIDemoClient() &&
                    m_teamIndex >= maxDemoSize )
                {
                    g_renderer->TextSimple( realX + 120, realY + m_h/2 - 8, col, 11, LANGUAGEPHRASE("dialog_demo_user") );

					char caption[256];
					sprintf( caption, LANGUAGEPHRASE("dialog_number_players_max") );
					LPREPLACEINTEGERFLAG( 'N', maxDemoSize, caption );

                    g_renderer->TextSimple( realX + 120, realY + m_h/2 + 2, col, 9, caption );

                   // SetTooltip( "tooltip_lobby_demo_max_player", true );
                }
                else
                {
                    g_renderer->TextSimple( realX + 120, realY + m_h/2 - 5, col, 11, LANGUAGEPHRASE("dialog_click_to_open") );
                    SetTooltip( "tooltip_lobby_click_open_slot", true );
                }
            }
        }
        else
        {
            g_renderer->RectFill( realX, realY, m_w, m_h, Colour(255,255,255,20) );            
            g_renderer->Rect( realX, realY, m_w, m_h, Colour(255,255,255,30), 1 );

            int teamId = lobby->m_teamOrder[m_teamIndex];
            Team *team = g_app->GetWorld()->GetTeam(teamId);

            if( !team )
            {
                // This is an empty slot
                Colour col(255,255,255,150);
                if( fmodf( GetHighResTime()*2, 2.0f ) < 1.0f )
                {
                    col.m_a *= 0.3f;
                }
                g_renderer->TextSimple( realX + 10, realY + m_h/2 - 6, col, 12, LANGUAGEPHRASE("dialog_awaiting_player") );

                if( g_app->GetServer() )
                {
                    Colour col(255,255,255,50);
                    g_renderer->TextSimple( realX + 120, realY + m_h/2 - 5, col, 11, LANGUAGEPHRASE("dialog_click_to_close") );
                    SetTooltip( "tooltip_lobby_click_close_slot", true );
                }
            }
            else
            {
                // There is a team here
                // Work out their colours
                Colour colour = team->GetTeamColour();
                colour.m_a = 200;
                if( highlighted && 
                    (team->m_type == Team::TypeLocalPlayer ||
                    (team->m_type == Team::TypeAI && g_app->GetServer())))
                {
                    colour.m_a = 255;
                }
                                
                Colour teamColDark = colour;
                teamColDark.m_r *= 0.2f;
                teamColDark.m_g *= 0.2f;
                teamColDark.m_b *= 0.2f;

                g_renderer->RectFill( realX, realY, m_w, m_h, colour, teamColDark, teamColDark, colour );            
                g_renderer->Rect( realX, realY, m_w, m_h, Colour(255,255,255,100) );

                char *teamName = team->GetTeamName();

                //
                // Write their name

                bool isDemo = g_app->GetClientToServer()->IsClientDemo( team->m_clientId ) &&
                              team->m_type != Team::TypeAI;

                char teamNameFull[256];                
                if( team->m_type == Team::TypeAI )
				{
					sprintf( teamNameFull, LANGUAGEPHRASE("dialog_cpu_team_name") );
					LPREPLACESTRINGFLAG( 'T', teamName, teamNameFull );
				}
                else if( !team->m_nameSet )
				{
					sprintf( teamNameFull, LANGUAGEPHRASE("dialog_joining") );
				}
                else if( isDemo )
				{
					sprintf( teamNameFull, LANGUAGEPHRASE("dialog_demo_team_name") );
					LPREPLACESTRINGFLAG( 'T', teamName, teamNameFull );
				}
                else
				{
					sprintf( teamNameFull, "%s", teamName );
				}
                
                g_renderer->TextSimple( realX + 10, realY+m_h/2-6, White, 12, teamNameFull );

                //
                // Ready status

                if( team->m_readyToStart )
                {
                    g_renderer->TextSimple( realX + 130, realY+m_h/2-5, White, 11, LANGUAGEPHRASE("dialog_ready") );
                }


                //
                // Selection box

                if( lobby->m_selectionId == team->m_teamId )
                {
                    g_renderer->Rect( realX-2, realY-2, m_w+4, m_h+4, Colour(255,255,255,255), 1.5f );
                }

                //
                // Team options button

                if( TeamOptionsAvailable() )
                {
                    int optionsX, optionsY, optionsW, optionsH;
                    GetTeamOptionsButton( optionsX, optionsY, optionsW, optionsH );
                    
                    Image *img = g_resource->GetImage("gui/arrow.bmp");
                    g_renderer->SetBlendMode( Renderer::BlendModeAdditive );
                    g_renderer->Blit( img, realX+optionsX, realY+optionsY, optionsW, optionsH, Colour(255,255,255,150) );
                    
                    if( MouseInTeamOptionsButton() )
                    {
                        g_renderer->Blit( img, realX+optionsX, realY+optionsY, optionsW, optionsH, Colour(255,255,255,250) );                    
                    }
                    g_renderer->SetBlendMode( Renderer::BlendModeNormal );
                }


                //
                // Extra data if we are the server
                // Put the extra data into a tooltip

                if( team->m_type != Team::TypeAI &&
                    g_app->GetServer() && 
                    g_app->GetServer()->m_teams.ValidIndex(team->m_teamId) )
                {
                    ServerTeam *sTeam = g_app->GetServer()->m_teams[team->m_teamId];
                    ServerToClient *sToC = g_app->GetServer()->GetClient(sTeam->m_clientId);
                    
                    if( sToC )
                    {
                        char netLocation[512];
						sprintf( netLocation, LANGUAGEPHRASE("tooltip_lobby_address_port") );
						LPREPLACESTRINGFLAG( 'I', sToC->m_ip, netLocation );
						LPREPLACEINTEGERFLAG( 'P', sToC->m_port, netLocation );
                        SetTooltip( netLocation, false );
                    }                   
                }     
                else
                {
                    SetTooltip( " ", false );
                }
            }
        }
    }

    void MouseUp()
    {
        if( !g_app->GetClientToServer()->IsConnected() )
        {
            return;
        }

        int maxTeams = g_app->GetGame()->GetOptionValue("MaxTeams");
        if( m_teamIndex >= maxTeams )
        {
            // This is a closed slot. Clicking = open the slot (if we are a server)            
            if( g_app->GetServer() )
            {                
                int maxDemoSize, maxDemoPlayers;
                bool allowDemoServers;
                g_app->GetClientToServer()->GetDemoLimitations( maxDemoSize, maxDemoPlayers, allowDemoServers );

                if( g_app->GetClientToServer()->AmIDemoClient() &&
                    m_teamIndex >= maxDemoSize )
                {
                    BadKeyWindow *badKey = new BadKeyWindow();

					sprintf( badKey->m_extraMessage, LANGUAGEPHRASE("dialog_demo_too_many_players") );
					LPREPLACEINTEGERFLAG( 'N', maxDemoSize, badKey->m_extraMessage );

					badKey->m_offerDemo = false;
                    EclRegisterWindow(badKey);                

                }
                else
                {
                    int optionIndex = g_app->GetGame()->GetOptionIndex("MaxTeams");
                    g_app->GetClientToServer()->RequestOptionChange( optionIndex, m_teamIndex+1 );
                }
            }
        }
        else
        {
            LobbyWindow *lobby = (LobbyWindow *) m_parent;
            int teamId = lobby->m_teamOrder[m_teamIndex];

            if( g_app->GetWorld()->GetTeam(teamId) )
            {
                // We just clicked on a team

                if( TeamOptionsAvailable() )
                {
                    Team *team = g_app->GetWorld()->GetTeam(teamId);
                    LobbyWindow *lobby = (LobbyWindow *) m_parent;
                    lobby->m_selectionId = team->m_teamId;
                    
                    if( MouseInTeamOptionsButton() )
                    {
                        EclWindow *teamOptions = EclGetWindow("TeamOptions");
                        int existingTeam = -1;
                        
                        if( teamOptions )
                        {
                            existingTeam = ((TeamOptionsWindow*)teamOptions)->m_teamId;
                            EclRemoveWindow( "TeamOptions");
                        }

                        if( existingTeam == -1 || existingTeam != team->m_teamId )
                        {
                            TeamOptionsWindow *teamOptions = new TeamOptionsWindow( team->m_teamId );
                            EclRegisterWindow( teamOptions );
                            teamOptions->SetPosition( m_parent->m_x + m_x + m_w + 5, 
                                                      m_parent->m_y + m_y );
                        }
                    }
                }
            }
            else
            {
                // Close this slot
                if( g_app->GetServer() )
                {                
                    int optionIndex = g_app->GetGame()->GetOptionIndex("MaxTeams");
                    g_app->GetClientToServer()->RequestOptionChange( optionIndex, m_teamIndex );
                }
            }
        }
    }
};


// ============================================================================


class ClientOptionButton : public EclButton
{
public:
    int     m_optionId;
    int     m_previousValue;
    float   m_changeTimer;
    bool    m_showChanges;

public:
    ClientOptionButton()
        :   EclButton(),
            m_optionId(-1),
            m_previousValue(0.0f),
            m_changeTimer(0.0f),
            m_showChanges(true)
    {
    }

    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        GameOption *option = g_app->GetGame()->m_options[m_optionId];               
        
        if( option )
        {
            char caption[256];

            //
            // Render the option

            if( option->m_change == 0 )
            {
                AppAssert( option->m_subOptions.ValidIndex(option->m_currentValue) );
				strcpy( caption, GameOption::TranslateValue( option->m_subOptions[option->m_currentValue] ) );
            }
            else if( option->m_change == -1 )
            {
                strcpy( caption, option->m_currentString );
            }
            else
            {
                sprintf( caption, "%d", option->m_currentValue );
            }

            bool optionEditable = g_app->GetGame()->IsOptionEditable(m_optionId);

            Colour colour = White;
            if( !optionEditable )
            {
                colour.Set( 128,128,128,255 );
            }

            if( m_showChanges &&
                option->m_currentValue != option->m_default &&
                optionEditable )
            {
                g_renderer->RectFill( realX, realY, m_w, m_h, Colour(255,255,255,20) );
                g_renderer->TextRight( realX+m_w, realY+m_h-8, Colour(255,0,0,200), 8, LANGUAGEPHRASE("dialog_edited") );
            }
            
            if( !optionEditable )
            {
                g_renderer->TextRight( realX+m_w, realY+m_h-8, Colour(255,0,0,200), 8, LANGUAGEPHRASE("dialog_locked") );
            }

            g_renderer->TextCentreSimple( realX + m_w/2, realY + 4, colour, 12, caption);

            //
            // Has this option changed since last frame?

            if( option->m_currentValue != m_previousValue )
            {
                m_changeTimer = 2.0f;
                m_previousValue = option->m_currentValue;
            }

            if( m_changeTimer > 0.0f )
            {
                m_changeTimer -= g_advanceTime;
                float fraction = m_changeTimer / 2.0f;
                Clamp( fraction, 0.0f, 1.0f );
                Colour col( 255, 255, 255, fraction*155 );
                g_renderer->RectFill( realX, realY, m_w, m_h, col );
            }
        }
    }
};


class TeamReadyButton : public InterfaceButton
{
public:
    char   *m_lastServerName;
	bool    m_enable;

public:
    TeamReadyButton()
        :   m_lastServerName(NULL),
            m_enable(true)
    {
    }

	~TeamReadyButton()
	{
		if( m_lastServerName )
		{
			delete [] m_lastServerName;
			m_lastServerName = NULL;
		}
	}

	bool IsEnable()
	{
        if( g_app->GetServer() ) 
		{
			int serverNameIndex = g_app->GetGame()->GetOptionIndex( "ServerName" );
			if( serverNameIndex != -1 )
			{
				char *newServerName = g_app->GetGame()->GetOption( "ServerName" )->m_currentString;
				if( m_lastServerName && strcmp( m_lastServerName, newServerName ) == 0 )
				{
					return m_enable;
				}

				size_t lenServerName = strlen( newServerName );
				if( m_lastServerName )
				{
					delete [] m_lastServerName;
				}
				m_lastServerName = new char[ lenServerName + 1 ];
				strncpy( m_lastServerName, newServerName, lenServerName + 1 );

				char *copyServerName = new char[ lenServerName + 1 ];
				strncpy( copyServerName, newServerName, lenServerName + 1 );
				SafetyString( copyServerName );
				ReplaceExtendedCharacters( copyServerName );
				size_t nbBadChar = 0;
				for( int i = 0; i < lenServerName; i++ )
				{
					if( copyServerName[ i ] == ' ' )
					{
						nbBadChar++;
					}
				}
				delete [] copyServerName;

				if( nbBadChar == lenServerName && m_enable )
				{
					m_enable = false;
					SetTooltip( "tooltip_lobby_ready_disable", true );
				}
				else if( nbBadChar < lenServerName && !m_enable )
				{
					m_enable = true;
					SetTooltip( "tooltip_lobby_ready", true );
				}
			}
		}

		return m_enable;
	}

    bool IsVisible()
    {
        if( g_app->GetServer() ) 
        {
            return true;
        }

        return !g_app->GetWorld()->AmISpectating();
    }

    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        if( IsVisible() )
        {
			if( IsEnable() )
			{
				InterfaceButton::Render( realX, realY, highlighted, clicked );
			}
			else
			{
				float originalAlpha = g_renderer->m_alpha;
				g_renderer->m_alpha *= 0.667f;
				InterfaceButton::Render( realX, realY, false, false );
				g_renderer->m_alpha = originalAlpha;
			}
        }
    }

    void MouseUp()
    {
        if( !IsVisible() || !IsEnable() ) return;

        //
        // Are we turning on or off?
        bool enable = false;
        Team *thisTeam = g_app->GetWorld()->GetMyTeam();
        bool isServer = g_app->GetServer() != NULL;

		if( isServer )
		{
			int serverNameIndex = g_app->GetGame()->GetOptionIndex( "ServerName" );
			if( serverNameIndex != -1 )
			{
				SafetyString( g_app->GetGame()->GetOption( "ServerName" )->m_currentString );
				ReplaceExtendedCharacters( g_app->GetGame()->GetOption( "ServerName" )->m_currentString );
			}
		}

        if( !thisTeam && isServer ) 
        {
            // We dont have a team...maybe its AIs only?
            for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
            {
                Team *team = g_app->GetWorld()->m_teams[i];
                if( team->m_type == Team::TypeAI )
                {
                    thisTeam = team;
                    break;
                }
            }
        }

        if( !thisTeam ) return;
        
        enable = !thisTeam->m_readyToStart;
                
        //
        // Now do it
        for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
        {
            Team *thisTeam = g_app->GetWorld()->m_teams[i];
            bool validTeam = thisTeam->m_type == Team::TypeLocalPlayer;
            if( thisTeam->m_type == Team::TypeAI && isServer ) validTeam = true;

            if( validTeam )
            {
                // Generate a non-deterministic random seed
                // So the game wont be the same everytime it plays
                unsigned char randSeed = (unsigned char)time(NULL);
                randSeed += thisTeam->m_teamId;

                if( enable && !thisTeam->m_readyToStart )
                {
                    g_app->GetClientToServer()->RequestStartGame( thisTeam->m_teamId, randSeed );
                }

                if( !enable && thisTeam->m_readyToStart )
                {
                    g_app->GetClientToServer()->RequestStartGame( thisTeam->m_teamId, randSeed );
                }
            }   
        }
    }
};


class RequestPlayerButton : public InterfaceButton
{
public:
    float m_timer;
    RequestPlayerButton()
        : m_timer(0.0f)
    {
    }

    bool IsVisible()
    {
        return( !g_app->GetWorld()->GetMyTeam() );
    }

    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        if( IsVisible() )
        {
            if( m_timer > 0.0f )
            {
                m_timer -= g_advanceTime;
                m_timer = max( 0.0f, m_timer );
            }
            else
            {
                InterfaceButton::Render( realX, realY, highlighted, clicked );
            }
        }
    }       
    void MouseUp()
    {    
        if( m_timer == 0.0f &&
            IsVisible() )
        {
            g_app->GetClientToServer()->RequestTeam( Team::TypeLocalPlayer );
            m_timer = 5.0f;
        }
    }
};


class RequestAIButton : public InterfaceButton
{
    bool IsVisible()
    {
        int numTeams = g_app->GetWorld()->m_teams.Size();
        int maxTeams = g_app->GetGame()->GetOptionValue("MaxTeams");

        return( numTeams < maxTeams );
    }

    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        if( IsVisible() &&
            g_app->GetServer() )
        {
            InterfaceButton::Render( realX, realY, highlighted, clicked );
        }
    }

    void MouseUp()
    {
        if( IsVisible() &&
            g_app->GetServer() )
        {
            g_app->GetClientToServer()->RequestTeam( Team::TypeAI );
        }
    }
};


class RequestChatButton : public InterfaceButton
{
    void MouseUp()
    {
        if( !EclGetWindow("Comms Window") )
        {
            EclRegisterWindow( new ChatWindow(), m_parent );
        }
    }
};


class TeamNameInputField : public InputField
{
    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        LobbyWindow *lobby = (LobbyWindow *) m_parent;
        Team *team = g_app->GetWorld()->GetTeam(lobby->m_selectionId);
        if( team && team->m_type != Team::TypeUnassigned &&
            (team->m_teamId == g_app->GetWorld()->m_myTeamId ||
            (g_app->GetServer() && g_app->GetWorld()->GetTeam( lobby->m_selectionId )->m_type == Team::TypeAI )))
        {
            InputField::Render( realX, realY, highlighted, clicked );            
        }
    }
};


class RequestTerritoryButton : public InterfaceButton
{
public:
    int m_territoryId;
    
    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        LobbyWindow *lobby = (LobbyWindow *) m_parent;

        if( lobby->m_selectionId != -1 &&
            (lobby->m_selectionId == g_app->GetWorld()->m_myTeamId ||
            (g_app->GetServer() && g_app->GetWorld()->GetTeam( lobby->m_selectionId )->m_type == Team::TypeAI ) ) ) 
        {
            Team *team = g_app->GetWorld()->GetTeam(lobby->m_selectionId);
            if( team && team->OwnsTerritory( m_territoryId ) )
            {
                InterfaceButton::Render( realX, realY, highlighted, true );
            }
            else if( g_app->GetWorld()->GetTerritoryOwner( m_territoryId ) == -1 )
            {
                InterfaceButton::Render( realX, realY, highlighted, clicked );            
            }
        }
    }

    void MouseUp()
    {
        LobbyWindow *lobby = (LobbyWindow *) m_parent;

        if( lobby->m_selectionId != -1 &&
            (lobby->m_selectionId == g_app->GetWorld()->m_myTeamId ||
            (g_app->GetServer() && g_app->GetWorld()->GetTeam( lobby->m_selectionId )->m_type == Team::TypeAI ) ) ) 
        {
            Team *team = g_app->GetWorld()->GetTeam(lobby->m_selectionId);

            if( g_app->GetWorld()->GetTerritoryOwner( m_territoryId ) == -1 ||
                team->OwnsTerritory(m_territoryId) )
            {
                g_app->GetClientToServer()->RequestTerritory( lobby->m_selectionId, m_territoryId );
            }
        }
    }
};


class ExitButton : public InterfaceButton
{
public:
    char testString[512];
    void MouseUp()
    {
        EclRemoveWindow( "Comms Window" );
        EclRemoveWindow( m_parent->m_name );

		g_app->SaveGameName();
        g_app->ShutdownCurrentGame();
    }
};


class MiniWorldMapButton : public EclButton
{
public:
    int m_selectionId;

    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        //
        // Render the world map

        Image *bmpWorldMap = g_resource->GetImage( "graphics/map.bmp" );
        AppDebugAssert( bmpWorldMap );

        int worldMapW = m_w;
        int worldMapH = m_h;
        int worldMapX = realX;
        int worldMapY = realY;

        g_renderer->SetBlendMode( Renderer::BlendModeNormal );
        g_renderer->Blit( bmpWorldMap, worldMapX, worldMapY, worldMapW, worldMapH, White );
        g_renderer->Rect( worldMapX, worldMapY, worldMapW, worldMapH, Colour(100,100,200,255) );


        //
        // Caption : select territories

        int selectedTeamId = ((LobbyWindow *)m_parent)->m_selectionId;         
        Team *selectedTeam = g_app->GetWorld()->GetTeam(selectedTeamId);
        
        int numTerritories = ( selectedTeam ? selectedTeam->m_territories.Size() : 0 );
        int maxTerritories = g_app->GetGame()->GetOptionValue( "TerritoriesPerTeam" );
        int randomTerritories = g_app->GetGame()->GetOptionValue("RandomTerritories");

        if( !randomTerritories &&
            selectedTeam &&
            numTerritories < maxTerritories )
        {
            Colour col(200,200,255,200);
            g_renderer->SetFont( "kremlin" );

			char caption[512];
			sprintf( caption, LANGUAGEPHRASE("dialog_click_select_territory") );
			LPREPLACEINTEGERFLAG( 'T', numTerritories, caption );
			LPREPLACEINTEGERFLAG( 'M', maxTerritories, caption );

            g_renderer->TextCentreSimple( realX+m_w/2, realY+m_h-15, col, 15, caption );
            g_renderer->SetFont();
        }

        if( randomTerritories )
        {
            g_renderer->SetFont( "kremlin" );
            g_renderer->TextCentre( realX+m_w/2, realY+m_h-15, Colour(200,200,255,100), 15, LANGUAGEPHRASE("dialog_random_territories") );
            g_renderer->SetFont();
        }


        //
        // Render territory ownership

        g_renderer->SetBlendMode( Renderer::BlendModeAdditive );

        for( int t = 0; t < MAX_TEAMS; ++t )
        {
            Team *team = g_app->GetWorld()->GetTeam(t);
            if( team )
            {
                for( int i = 0; i <  team->m_territories.Size(); ++i )
                {
                    int territoryId = team->m_territories[i];
                    Image *img = g_app->GetMapRenderer()->GetTerritoryImage(territoryId);
                    Colour col = team->GetTeamColour();
                    col.m_a = 200;
                    g_renderer->Blit( img,  worldMapX, worldMapY, worldMapW, worldMapH, col );
                    if( team->m_teamId == ((LobbyWindow *)m_parent)->m_selectionId )
                    {
                        col.Set(255,255,255,70);
                        g_renderer->Blit( img,  worldMapX, worldMapY, worldMapW, worldMapH, col );
                    }
                }
            }
        }

        //
        // Is the player highlighting a territory right now?

        m_selectionId = -1;

        if( !randomTerritories )
        {
            if( highlighted || clicked )
            {
                float fractionX = ( g_inputManager->m_mouseX - realX ) / (float)m_w;
                float fractionY = ( g_inputManager->m_mouseY - realY ) / (float)m_h;

                for( int i = 0; i < World::NumTerritories; ++i )
                {
                    Image *img = g_app->GetMapRenderer()->GetTerritoryImage(i);

                    int pixelX = img->Width() * fractionX;
                    int pixelY = img->Height() * (1.0f - fractionY);

                    Colour thisCol = img->GetColour( pixelX, pixelY );

                    if( thisCol.m_r > 128 ) m_selectionId = i;
                }            
            }


            if( m_selectionId != -1 )
            {
                Image *img = g_app->GetMapRenderer()->GetTerritoryImage(m_selectionId);

                Colour col = selectedTeam ? selectedTeam->GetTeamColour() : Colour(255,255,255,255);            
                col.m_a = 100 + fabs(sinf(GetHighResTime()*10)) * 100;

                int currentOwner = g_app->GetWorld()->GetTerritoryOwner( m_selectionId );
                if( numTerritories < maxTerritories ||
                    currentOwner == -1 ||
                    currentOwner == selectedTeamId )
                {
                    if( selectedTeam )
                    {
                        g_renderer->Blit( img,  worldMapX, worldMapY, worldMapW, worldMapH, col );
                    }
                }   


                //
                // left click - select territory 

                if( g_inputManager->m_lmbUnClicked )
                {                
                    if( selectedTeamId == g_app->GetWorld()->m_myTeamId ||
                        (g_app->GetServer() && 
                        selectedTeam &&
                        selectedTeam->m_type == Team::TypeAI ) ) 
                    {
                        g_app->GetClientToServer()->RequestTerritory( selectedTeamId, m_selectionId );
                    }                
                }
            }
        }
    }
};


// ============================================================================


void RegisterGameOptionButton( InterfaceWindow *_window, 
                               float _xPos, float _yPos, float _w, float _h, float _captionW, 
                               int _optionId, BoundedArray<GameOption *> &_options,
                               bool showChanges, bool _inputNoExtendedCharacters, bool _onTwoLines, float _lineH )
{
    //
    // Create the option data

    GameOption *_option = new GameOption();
    _option->Copy( g_app->GetGame()->m_options[_optionId] );
    _options[_optionId] = _option;

    
    //
    // Create the buttons

    char name[256];
    sprintf( name, "Param%d", _optionId );

    float optionW = _w - _captionW - 10;
    float optionX = _xPos + _captionW;
    float optionY = _yPos;

    if( _onTwoLines )
    {
        optionW = _w - 10;
        optionX = _xPos;
        optionY = _yPos + _lineH;
    }

    TextButton *textButton = new TextButton();
    textButton->SetProperties( name, _xPos, _yPos, _captionW, _h, GameOption::TranslateValue( _option->m_name ), " ", false, false );
    textButton->m_fontSize = 11;
    textButton->m_fontCol.Set(255,255,255,180);
    _window->RegisterButton( textButton );

    if( g_app->GetServer() &&
        g_app->GetGame()->IsOptionEditable( _optionId ) )
    {
        if( _option->m_change == 0 )
        {            
            // Drop down menu
            DropDownMenu *menu = new DropDownMenu();
            menu->SetProperties( name, optionX, optionY, optionW, _h, GameOption::TranslateValue( _option->m_name ), " ", false, false );
            for( int p = _option->m_min; p <= _option->m_max; ++p )
            {
                menu->AddOption( GameOption::TranslateValue( _option->m_subOptions[p] ), p );
            }
            _window->RegisterButton( menu );
            menu->RegisterInt( &_option->m_currentValue );
        }
        else if( _option->m_change == -1 )
        {
            // String
            _window->CreateValueControl( name, optionX, optionY, optionW, _h, InputField::TypeString,
                                         _option->m_currentString, _option->m_change, _option->m_min, _option->m_max, 
                                         NULL, " ", false, _inputNoExtendedCharacters );
        }
        else
        {
            // Ordinary int selector
            _window->CreateValueControl( name, optionX, optionY, optionW, _h, InputField::TypeInt, 
                                         &_option->m_currentValue, _option->m_change, _option->m_min, _option->m_max, 
                                         NULL, " ", false );            
        }
    }
    else
    {
        //
        // We are a client, so just show us the value
        // Or this option might not be editable
        ClientOptionButton *cob = new ClientOptionButton();
        cob->SetProperties( name, optionX, optionY, optionW, _h, " ", " ", false, false );
        cob->m_optionId = _optionId;
        cob->m_showChanges = showChanges;
        _window->RegisterButton( cob );
    }   
}


void SetGameOptions( BoundedArray<GameOption *> &_options )
{
    for( int i = 0; i < g_app->GetGame()->m_options.Size(); ++i )
    {        
        if( _options[i] )
        {
            GameOption *param = g_app->GetGame()->m_options[i];
            if( _options[i]->m_currentValue != param->m_currentValue )
            {
                g_app->GetClientToServer()->RequestOptionChange( i, _options[i]->m_currentValue );
            }

            if( _options[i]->m_change == -1 &&
                strcmp( _options[i]->m_currentString, param->m_currentString ) != 0 )
            {
                g_app->GetClientToServer()->RequestOptionChange( i, _options[i]->m_currentString );
            }
        }
    }
}


class AdvancedOptionsButton : public InterfaceButton
{
    void MouseUp()
    {
        if( !EclGetWindow( "Advanced Options" ) )
        {
            EclRegisterWindow( new LobbyOptionsWindow() );
        }
        else
        {
            EclBringWindowToFront( "Advanced Options" );
        }
    }
};



LobbyOptionsWindow::LobbyOptionsWindow()
:   InterfaceWindow("Advanced Options", "dialog_lobby_advanced_options", true),
    m_updateTimer(0.0f),
    m_gameMode(0)
{
    int numOptions = g_app->GetGame()->m_options.Size();
    int actualNumOptions = numOptions-4;
    int height = 50 + actualNumOptions * 25;

    SetSize( 550, height );    
    Centralise();

    m_minH = 350;
}


void LobbyOptionsWindow::Create()
{
    InterfaceWindow::Create();

    //
    // Inverted box

    InvertedBox *box = new InvertedBox();
    box->SetProperties( "invert", 10, 30, 320, m_h-40, " ", " ", false, false );
    RegisterButton( box );


    //
    // Options

    int numOptions = g_app->GetGame()->m_options.Size();
    m_gameOptions.Initialise( numOptions ); 
    m_gameOptions.SetAll( NULL );

    int availableSpace = m_h - 50;
    int actualNumOptions = numOptions-4;
    float heightPerOption = availableSpace / (float)actualNumOptions;
    heightPerOption = min( heightPerOption, 25.0f );

    float optionY = 40;
    float optionGap = heightPerOption*0.25f;
    float optionH = heightPerOption-optionGap;
    int optionW = 320;
    int optionX = 10;


    for( int i = 0; i < g_app->GetGame()->m_options.Size(); ++i )
    {        
        GameOption *param = g_app->GetGame()->m_options[i];

        if( stricmp( param->m_name, "MaxTeams" ) != 0 &&
            stricmp( param->m_name, "ServerName" ) != 0 &&
            stricmp( param->m_name, "GameMode" ) != 0 &&
            stricmp( param->m_name, "ScoreMode" ) != 0 )
        {
            RegisterGameOptionButton( this, optionX, optionY, optionW, optionH, 170, i, m_gameOptions, true, false, false, 0 );

            optionY += optionH;
            optionY += optionGap;
        }
    }

    //
    // Close button

    CloseButton *close = new CloseButton();
    close->SetProperties( "Close", m_w - 170, m_h - 30, 160, 20, "dialog_close", " ", true, false );
    RegisterButton( close );
}


void LobbyOptionsWindow::Update()
{
    if( !g_app->GetClientToServer() ||
        !g_app->GetClientToServer()->IsConnected() )
    {
        EclRemoveWindow(m_name);
        return;
    }


    //
    // If the game mode has changed, we need to rebuild

    if( g_app->GetServer() )
    {
        int currentGameMode = g_app->GetGame()->GetOptionValue("GameMode");
        if( currentGameMode != m_gameMode )
        {
            m_gameMode = currentGameMode;
            Remove();
            Create();
            return;
        }
    }


    //
    // Every few ms, transmit all the options that are different
    // locally than the known network values

    float timeNow = GetHighResTime();

    if( timeNow > m_updateTimer )
    {
        m_updateTimer = timeNow + 0.5f;

        
        //
        // If we are the server, transmit all modified options

        if( g_app->GetServer() )
        {
            SetGameOptions( m_gameOptions );

            int numTeams = g_app->GetWorld()->m_teams.Size();
            if( numTeams > 0 )
            {
                int numTerritories = World::NumTerritories / numTeams;
                int optionId = g_app->GetGame()->GetOptionIndex("TerritoriesPerTeam");
                if( optionId != -1 )
                {                
                    if ( g_app->GetGame()->IsOptionEditable( optionId ) )
					{
						char name[16];
						sprintf(name, "Param%d", optionId );
						InputField *control = (InputField *)GetButton( name );
							
						if( control )
						{
							control->m_highBound = numTerritories;
						}
					}
                
                    if( numTeams * g_app->GetGame()->GetOptionValue("TerritoriesPerTeam") > World::NumTerritories )
                    {
                        m_gameOptions[optionId]->m_currentValue = numTerritories;
                    }

                    for( int i = 0; i < numTeams; ++i )
                    {
                        Team *team = g_app->GetWorld()->m_teams[i];
                        int teamTerritories = team->m_territories.Size();
                        while( teamTerritories > m_gameOptions[optionId]->m_currentValue )
                        {
                           g_app->GetClientToServer()->RequestTerritory( team->m_teamId, team->m_territories[teamTerritories -1] );
                           teamTerritories -= 1;
                        }
                    }
                }
            }
        }
    }
}


void LobbyOptionsWindow::Render( bool _hasFocus )
{
    InterfaceWindow::Render( _hasFocus );


    //
    // Is the mouse highlighting any server option?

    float x = m_x + 340;
    float y = m_y + 30;
    float w = m_w - (x-m_x) - 10;

    char *currentButton = EclGetCurrentButton();

    if( strncmp( currentButton, "Param", 5 ) == 0 )
    {
        char number[8];
        number[0] = *(currentButton+5);
        number[1] = *(currentButton+6);
        number[2] = '\x0';
        
        int paramIndex = atoi(number);

        char *paramName = g_app->GetGame()->m_options[paramIndex]->m_name;
        char stringId[256];
        sprintf( stringId, "tooltip_gameoption_%s", paramName );

        if( g_languageTable->DoesPhraseExist( stringId ) )
        {
            g_renderer->TextSimple( x, y, White, 14, GameOption::TranslateValue( paramName ) );
            y+= 20;

            char *fullString = LANGUAGEPHRASE(stringId);
            
            MultiLineText wrapped( fullString, w, 11 );
            
            for( int i = 0; i < wrapped.Size(); ++i )
            {
                char *thisString = wrapped[i];
                g_renderer->TextSimple( x, y+=13, White, 11, thisString );
            }
        }

        if( !g_app->GetGame()->IsOptionEditable(paramIndex) )
        {
            y+=30;

            char authKey[256];
            Authentication_GetKey( authKey );
            if( Authentication_IsDemoKey(authKey) )
            {
                g_renderer->TextSimple( x, y+=13, White, 11, LANGUAGEPHRASE("dialog_option_locked_demo_1") );
                g_renderer->TextSimple( x, y+=13, White, 11, LANGUAGEPHRASE("dialog_option_locked_demo_2") );
            }
            else
            {
                g_renderer->TextSimple( x, y+=13, White, 11, LANGUAGEPHRASE("dialog_option_locked_game_mode_1") );
                g_renderer->TextSimple( x, y+=13, White, 11, LANGUAGEPHRASE("dialog_option_locked_game_mode_2") );
            }
        }
    }
}



// ============================================================================


LobbyWindow::LobbyWindow()
:   InterfaceWindow( "LOBBY", "dialog_lobby", true ),
    m_selectionId(-1),
    m_updateTimer(0.0f),
    m_gameMode(0)
{    
    SetSize( 530, 470 );
    Centralise();

    for( int i = 0; i < MAX_TEAMS; ++i )
    {
        m_teamOrder[i] = -1;
    }    
}


LobbyWindow::~LobbyWindow()
{   
    int numSpectators = g_app->GetGame()->GetOptionValue( "MaxSpectators" );

    if( numSpectators == 0 )
    {
        if( MetaServer_IsRegisteringOverWAN() )
        {
            MetaServer_StopRegisteringOverWAN();
        }

        if( MetaServer_IsRegisteringOverLAN() )
        {
            MetaServer_StopRegisteringOverLAN();
        }
    }
}   


bool LobbyWindow::StartNewServer()
{
    char ourIp[256];
    GetLocalHostIP( ourIp, sizeof(ourIp) );

    bool success = g_app->InitServer();

    if( success )
    {
        int ourPort = g_app->GetServer()->GetLocalPort();
        g_app->GetClientToServer()->ClientJoin( ourIp, ourPort );
        g_app->GetClientToServer()->RequestTeam( Team::TypeLocalPlayer );
        g_app->InitWorld();
    }

    return success;
}



void LobbyWindow::Create()
{
    //
    // Team buttons

    float boxW  = 200;
    float boxWTeam = boxW + 40;
    int boxX    = 20;
    int boxH    = 25;
    int boxGap  = 5;
    int boxY    = 40;
    int totalH  = (boxH+boxGap)*(MAX_TEAMS-1)+15;
    totalH += 150;

    float worldMapW = boxW + 40;
    float worldMapH = worldMapW * 220.0f/360.0f;

    InvertedBox *teamsBox = new InvertedBox();
    teamsBox->SetProperties( "TeamsBox", 10, 30, m_w-20, m_h - 70, " ", " ", false, false );
    RegisterButton( teamsBox );

    for( int i = 0; i < MAX_TEAMS-1; ++i )
    {        
        char buttonName[256];
        sprintf( buttonName, "Team%d", i );

        TeamButton *tb = new TeamButton();
        tb->SetProperties( buttonName, boxX, boxY, boxWTeam, boxH, " ", " ", false, false );
        tb->m_teamIndex = i;
        RegisterButton( tb );

        boxY += boxH + boxGap;
    }


    if( g_app->GetServer() )
    {
        boxY += 3;
        RequestAIButton *rai = new RequestAIButton();
        rai->SetProperties( "Request AI", boxX+30, boxY, boxWTeam-60, 18, "dialog_requestai", "tooltip_lobby_requestai", true, true );
        RegisterButton( rai );
    }


    //
    // Join button

    RequestJoinGameButton *join = new RequestJoinGameButton();
    join->SetProperties( "Join Game", boxX+30, m_h - 62, boxWTeam-60, 18, "dialog_joingame", " ", true, false );
    RegisterButton( join );


    boxX += boxW + 50;
    boxY = 40;
    
    //
    // World Map

    MiniWorldMapButton *worldMap = new MiniWorldMapButton();
    worldMap->SetProperties( "worldmap", boxX, boxY, worldMapW, worldMapH, " ", " ", false, false );
    RegisterButton( worldMap );

    boxW = worldMapW;
    boxY += worldMapH;
    boxY += 5;


    //
    // Important game options

    int numOptions = g_app->GetGame()->m_options.Size();
    m_gameOptions.Initialise( numOptions ); 
    m_gameOptions.SetAll( NULL );

    int serverNameIndex = g_app->GetGame()->GetOptionIndex("ServerName");
    int gameModeIndex = g_app->GetGame()->GetOptionIndex("GameMode");
    int scoreModeIndex = g_app->GetGame()->GetOptionIndex("ScoreMode");

    if( g_app->GetServer() )
    {
        // Make room for Server IP details
        //boxY -= 10;
    }

    RegisterGameOptionButton( this, boxX, boxY, boxW+10, 17, 100, serverNameIndex, m_gameOptions, false, true, true, 20 );
    RegisterGameOptionButton( this, boxX, boxY+=40, boxW+10, 17, 100, gameModeIndex, m_gameOptions, false, false, false, 0 );
    RegisterGameOptionButton( this, boxX, boxY+=20, boxW+10, 17, 100, scoreModeIndex, m_gameOptions, false, false, false, 0 );


    //
    // Team Ready / control buttons

    AdvancedOptionsButton *options = new AdvancedOptionsButton();
    options->SetProperties( "Advanced Options", 10, m_h - 30, 140, 20, "dialog_lobby_advanced_options", " ", true, false );
    RegisterButton( options );

    ExitButton *exit = new ExitButton();
    exit->SetProperties( "Exit", m_w - 260, m_h - 30, 120, 20, "dialog_leavegame", " ", true, false );
    RegisterButton( exit );

    TeamReadyButton *trb = new TeamReadyButton();
    trb->SetProperties( "Ready", m_w - 130, m_h - 30, 120, 20, "dialog_ready", "tooltip_lobby_ready", true, true );
    RegisterButton( trb );

}


void LobbyWindow::OrderTeams()
{
    //
    // Wipe out existing data

    for( int i = 0; i < MAX_TEAMS; ++i )
    {
        m_teamOrder[i] = -1;
    }    


    //
    // Build a list of all teams;

    LList<Team *> teams;
    for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
    {
        Team *team = g_app->GetWorld()->m_teams[i];
        teams.PutData( team );
    }


    //
    // Now put the other teams in, in alliance order

    int currentIndex = 0;

    while( teams.Size() > 0 )
    {
        Team *baseTeam = teams[0];
        m_teamOrder[currentIndex] = baseTeam->m_teamId;
        ++currentIndex;
        teams.RemoveData(0);

        for( int i = 0; i < teams.Size(); ++i )
        {
            Team *possibleAlly = teams[i];
            if( possibleAlly->m_allianceId == baseTeam->m_allianceId )
            {
                m_teamOrder[currentIndex] = possibleAlly->m_teamId;
                ++currentIndex;
                teams.RemoveData(i);
                --i;
            }
        }
    }
}


void LobbyWindow::Update()
{
    if( !g_app->GetClientToServer() ||
        !g_app->GetClientToServer()->IsConnected() )
    {
        return;
    }
    

    //
    // If the game mode has changed, we need to rebuild

    if( g_app->GetServer() )
    {
        int currentGameMode = g_app->GetGame()->GetOptionValue("GameMode");
        if( currentGameMode != m_gameMode )
        {
            m_gameMode = currentGameMode;
            Remove();
            Create();
            return;
        }
    }


    //
    // If we are a DEMO user, 
    //enforce max players + game tyoe

    if( g_app->GetServer() &&
        g_app->GetClientToServer()->AmIDemoClient() )
    {
        int maxGameSize, maxPlayers;
        bool allowDemoServers;
        g_app->GetClientToServer()->GetDemoLimitations( maxGameSize, maxPlayers, allowDemoServers );

        if( g_app->GetGame()->GetOptionValue("MaxTeams") > maxGameSize )
        {
            int optionIndex = g_app->GetGame()->GetOptionIndex("MaxTeams");
            g_app->GetClientToServer()->RequestOptionChange( optionIndex, maxGameSize );
        }
    }

 
    //
    // Send updated options now and then

    float timeNow = GetHighResTime();

    if( timeNow > m_updateTimer )
    {
        m_updateTimer = timeNow + 0.5f;        
 
        // 
        // Special case - if in Diplomacy mode, ensure we are Green

        int currentGameMode = g_app->GetGame()->GetOptionValue("GameMode");
        if( currentGameMode == GAMEMODE_DIPLOMACY )
        {
            for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
            {
                Team *team = g_app->GetWorld()->m_teams[i];
                if( team->m_clientId == g_app->GetClientToServer()->m_clientId &&
                    team->m_allianceId != 1 )
                {
                    g_app->GetClientToServer()->RequestAlliance( team->m_teamId, 1 );
                }
            }
        }


        //
        // Special case - if RandomTerritories is enforced, 
        // remove all of our territories

        int randomTerritories = g_app->GetGame()->GetOptionValue("RandomTerritories");
        if( randomTerritories )
        {
            for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
            {
                Team *team = g_app->GetWorld()->m_teams[i];
                if( team->m_clientId == g_app->GetClientToServer()->m_clientId )
                {
                    for( int j = 0; j < team->m_territories.Size(); ++j )
                    {
                        int territoryId = team->m_territories[j];
                        g_app->GetClientToServer()->RequestTerritory( team->m_teamId, territoryId, -1 );
                    }
                }
            }
        }


        //
        // Set the rest of the options if we are a server

        if( g_app->GetServer() )
        {
            SetGameOptions( m_gameOptions );
        }
    }


    //
    // Build a team ordering list based on alliances

    OrderTeams();


    //
    // Auto select our team if no team is selected


    if( m_selectionId == -1 )
    {
        m_selectionId = g_app->GetWorld()->m_myTeamId;
    }


    //
    // Kick us out if we arent authenticated 

#if AUTHENTICATION_LEVEL == 1 
    char authKey[256];
    Authentication_GetKey( authKey );
    int authResult = Authentication_GetStatus(authKey);

    if( authResult < 0 )
    {
        EclRemoveWindow( "Comms Window" );
        g_app->ShutdownCurrentGame();
        EclRemoveWindow( m_name );

        char *errorString = LANGUAGEPHRASE(Authentication_GetStatusStringLanguagePhrase(authResult));

        char authKey[256];
		if( Authentication_IsKeyFound() )
		{
			Authentication_GetKey( authKey );
		}
		else
		{
			sprintf( authKey, LANGUAGEPHRASE("dialog_authkey_not_found") );
		}

        char *metaServerDetails = (char *)( authResult == AuthenticationKeyInactive ? 
                                            LANGUAGEPHRASE("dialog_meta_server_details") : " " );
        char msg[2048];
        sprintf( msg, LANGUAGEPHRASE("dialog_not_permitted_start_game") );
		LPREPLACESTRINGFLAG( 'K', authKey, msg );
		LPREPLACESTRINGFLAG( 'E', LANGUAGEPHRASE(errorString), msg );
		LPREPLACESTRINGFLAG( 'M', metaServerDetails, msg );

        EclRegisterWindow( new MessageDialog( "Authentication FAILED", msg, false, "dialog_auth_failed", true ) );
        return;
    }
#endif
}


void LobbyWindow::Render( bool _hasFocus )
{
    if( !g_app->GetClientToServer()->IsConnected() )
    {
        return;
    }

    InterfaceWindow::Render( _hasFocus );


    //
    // Players and Spectators

    EclButton *specBox = GetButton("TeamsBox");
    if( specBox )
    {
        int maxSpectators = g_app->GetGame()->GetOptionValue("MaxSpectators");
        int numSpectators = g_app->GetWorld()->m_spectators.Size();

        float specY = m_y + specBox->m_y + 230;  
        float specX = m_x + specBox->m_x + 10;
        float specW = 200;

        g_renderer->SetFont( "kremlin" );

		char caption[512];
		sprintf( caption, LANGUAGEPHRASE("dialog_spectators_number") );
		LPREPLACEINTEGERFLAG( 'N', maxSpectators, caption );

        g_renderer->Text( specX+10, specY, Colour(255,255,255,130), 18, caption );
        g_renderer->SetFont();

        specY += 20;

        if( numSpectators == 0 )
        {
            if( maxSpectators > 0 )
            {
                Colour col(100,100,100,200);
                if( fmodf(GetHighResTime()*2, 2) < 1.0f ) col.m_a *= 0.6f;
                g_renderer->TextSimple( specX+10, specY+10, col, 11, LANGUAGEPHRASE("dialog_awaiting_spectators") );
            }
        }   
        else
        {
            float specH = (specBox->m_h - 270) / numSpectators;
            specH = min( specH, 20.0f );
            float gap = specH * 0.1f;

            for( int i = 0; i < numSpectators; ++i )
            {            
                if( g_app->GetWorld()->m_spectators.ValidIndex(i) )
                {
                    // There is a spectator here
                    Spectator *spec = g_app->GetWorld()->m_spectators[i];        
                    
                    char specName[512];
                    if( g_app->GetClientToServer()->IsClientDemo( spec->m_clientId ) )
                    {
						sprintf( specName, LANGUAGEPHRASE("dialog_demo_team_name") );
						LPREPLACESTRINGFLAG( 'T', spec->m_name, specName );
                    }
                    else
                    {
                        sprintf( specName, spec->m_name );
                    }

                    g_renderer->RectFill( specX, specY, specW, specH-gap, Colour(155,155,155,30), Colour(55,55,55,30), false );            
                    g_renderer->Text( specX+10, specY+(specH-gap)/2-6, Colour(255,255,255,128), 11, specName );

                    if( spec->m_clientId == g_app->GetClientToServer()->m_clientId )
                    {
                        g_renderer->Rect( specX, specY, specW, specH-gap, Colour(255,255,255,100), 1.0f );
                    }
                }

                specY += specH;
            }
        }
    }

    //
    // Server identity

    if( g_app->GetServer() )
    {
        char publicIP[256];
        int publicPort = -1;
        bool identityKnown = g_app->GetServer()->GetIdentity( publicIP, &publicPort );

        Colour col(255,255,255,200);
        char caption[512];

        if( identityKnown )
		{
			sprintf( caption, LANGUAGEPHRASE("dialog_internet_identity_details") );
			LPREPLACESTRINGFLAG( 'I', publicIP, caption );
			LPREPLACEINTEGERFLAG( 'P', publicPort, caption );
		}
        else
		{
			sprintf( caption, LANGUAGEPHRASE("unknown") );
		}

        g_renderer->TextSimple( m_x+275, m_y+272, col, 11, LANGUAGEPHRASE("dialog_internet_identity") );
        g_renderer->TextCentreSimple( m_x+440, m_y+272, White, 11, caption );


        char localIp[256];
        GetLocalHostIP( localIp, 256 );
        int localPort = g_app->GetServer()->GetLocalPort();

		sprintf( caption, LANGUAGEPHRASE("dialog_internet_identity_details") );
		LPREPLACESTRINGFLAG( 'I', localIp, caption );
		LPREPLACEINTEGERFLAG( 'P', localPort, caption );

		g_renderer->TextSimple( m_x+275, m_y+292, col, 11, LANGUAGEPHRASE("dialog_lan_identity") );
        g_renderer->TextCentreSimple( m_x+440, m_y+292, White, 11, caption );       
    }
    else
    {
        char serverIp[256];
        int serverPort;
        g_app->GetClientToServer()->GetServerDetails( serverIp, &serverPort );

        Colour col(255,255,255,200);

		char caption[512];
		sprintf( caption, LANGUAGEPHRASE("dialog_internet_identity_details") );
		LPREPLACESTRINGFLAG( 'I', serverIp, caption );
		LPREPLACEINTEGERFLAG( 'P', serverPort, caption );

        g_renderer->TextSimple( m_x+275, m_y+272, col, 11, LANGUAGEPHRASE("dialog_server_identity") );
        g_renderer->TextCentreSimple( m_x+440, m_y+272, White, 11, caption );       

    }


    //
    // Help on the current game mode settings

    GameOption *gameMode = g_app->GetGame()->GetOption("GameMode");
    GameOption *scoreMode = g_app->GetGame()->GetOption("ScoreMode");

    char gameModeStringId[256];
    char scoreModeStringId[256];
    
    sprintf( gameModeStringId, "tooltip_gamemode_%s", gameMode->m_subOptions[gameMode->m_currentValue] );
    sprintf( scoreModeStringId, "tooltip_scoremode_%s", scoreMode->m_subOptions[scoreMode->m_currentValue] );

    for( int i = 0; i < strlen(gameModeStringId); ++i )
    {
        if( gameModeStringId[i] == ' ' ) gameModeStringId[i] = '_';
    }

    char fullString[10000];
    sprintf( fullString, "%s\n\n%s", LANGUAGEPHRASE(gameModeStringId), LANGUAGEPHRASE(scoreModeStringId ) );

    float captionX = 270;
    float captionY = m_y + m_h - 70;
    float captionW = 250;
    float gap = 11;

    MultiLineText wrapped(fullString, captionW, 10 );

    for( int i = wrapped.Size()-1; i >= 0; --i )
    {
        char *thisLine = wrapped[i];
        g_renderer->Text( m_x+captionX, captionY-=gap, Colour(255,255,255,128), 10, thisLine );
    }


    //
    // Render the ready timer

    if( g_app->m_gameStartTimer > 0 )
    {
        char caption[256];
        sprintf( caption, LANGUAGEPHRASE("dialog_gamestart") );

        char timeLeft[256];
        sprintf( timeLeft, "%d", int(g_app->m_gameStartTimer)+1 );
		LPREPLACESTRINGFLAG( 'X', timeLeft, caption );
                
        g_renderer->TextSimple( m_x+captionX, m_y + m_h - 60, White, 17, caption );        
    }
    else
    {
        bool youLeft = true;
        for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
        {
            Team *team = g_app->GetWorld()->m_teams[i];
            if( !team->m_readyToStart && 
                team->m_clientId != g_app->GetClientToServer()->m_clientId )
            {
                youLeft = false;
            }
        }

        char *caption = NULL;
        if( youLeft ) caption = LANGUAGEPHRASE("dialog_gamestart_youready");
        else          caption = LANGUAGEPHRASE("dialog_gamestart_whenready");

        g_renderer->Text( m_x+captionX, m_y + m_h - 55, Colour(255,255,255,220), 10, caption );
    }


    g_renderer->SetBlendMode( Renderer::BlendModeNormal );


    //
    // Public IP:port
    // and Local IP:port

    //if( g_app->GetServer() )
    //{
    //    char publicIP[256];
    //    int publicPort = -1;
    //    bool identityKnown = g_app->GetServer()->GetIdentity( publicIP, &publicPort );

    //    char caption[512];
    //    Colour col;

    //    if( identityKnown )
    //    {
    //        sprintf( caption, "Internet Identity : %s : %d", publicIP, publicPort );
    //        col.Set(0,255,0,255);
    //    }
    //    else
    //    {
    //        sprintf( caption, "Internet Identity : Unknown" );
    //        col.Set(200,200,30,255);
    //    }

    //    //g_renderer->SetFont( "kremlin" );
    //    g_renderer->TextCentreSimple( m_x+m_w*3/4, m_y+204, col, 12, caption );


    //    //
    //    // Local IP and port

    //    char localIp[256];
    //    GetLocalHostIP( localIp, 256 );
    //    int localPort = g_app->GetServer()->GetLocalPort();

    //    sprintf( caption, "LAN Identity : %s : %d", localIp, localPort );
    //    col.Set(0,255,0,255);
    //    g_renderer->TextCentreSimple( m_x+m_w*3/4, m_y+220, col, 12, caption );

    //    g_renderer->SetFont();    
    //}
   
}


class ApplyTeamNameButton : public InterfaceButton
{
    void MouseUp()
    {
        RenameTeamWindow *parent = (RenameTeamWindow *)m_parent;

		SafetyString( parent->m_teamName );
		ReplaceExtendedCharacters( parent->m_teamName );

		if ( Team::IsValidName(parent->m_teamName) )
		{
			if( g_app->GetClientToServer()->IsConnected() && g_app->m_world )
			{
				Team *myTeam = g_app->GetWorld()->GetMyTeam();
				Team *team = g_app->GetWorld()->GetTeam( parent->m_teamId );

				if( team )
				{
					g_app->GetClientToServer()->RequestSetTeamName( team->m_teamId, parent->m_teamName );
				}
				else if( myTeam && parent->m_teamId == -1 )
				{
					g_app->GetClientToServer()->RequestSetTeamName( myTeam->m_teamId, parent->m_teamName );
				}
			}
			else if( parent->m_teamId == -1 )
			{
				if( strcmp( parent->m_teamName, LANGUAGEPHRASE("gameoption_PlayerName") ) == 0 )
				{
					g_preferences->SetString( "PlayerName", g_languageTable->LookupBasePhrase( "gameoption_PlayerName" ) );
				}
				else
				{
					g_preferences->SetString( "PlayerName", parent->m_teamName );
				}
				g_preferences->Save();
			}
		}
        
        EclRemoveWindow( parent->m_name );
    }
};


RenameTeamWindow::RenameTeamWindow( int _teamId )
:   InterfaceWindow( "RenameTeamWindow" )
{
    SetSize( 300, 120 );
    Centralise();

	bool found = false;
	if( g_app->GetClientToServer()->IsConnected() && g_app->m_world )
	{
		Team *myTeam = g_app->GetWorld()->GetMyTeam();
		Team *team = g_app->GetWorld()->GetTeam( _teamId );

		if( team && team != myTeam )
		{
			m_teamId = team->m_teamId;
			strncpy( m_teamName, team->m_name, sizeof( m_teamName ) );
			m_teamName[ sizeof( m_teamName ) - 1 ] = '\x0';
			found = true;
		}
		else if( myTeam && ( _teamId == -1 || _teamId == myTeam->m_teamId ) )
		{
			m_teamId = -1;
			strncpy( m_teamName, myTeam->m_name, sizeof( m_teamName ) );
			m_teamName[ sizeof( m_teamName ) - 1 ] = '\x0';
			found = true;
		}
	}
	else if( _teamId == -1 )
	{
		m_teamId = -1;
		char *prefPlayerName = g_preferences->GetString( "PlayerName" );
		strncpy( m_teamName, prefPlayerName, sizeof( m_teamName ) );
		m_teamName[ sizeof( m_teamName ) - 1 ] = '\x0';

		if( g_languageTable->DoesPhraseExist( "gameoption_PlayerName" ) && 
			strcmp( m_teamName, g_languageTable->LookupBasePhrase( "gameoption_PlayerName" ) ) == 0 )
		{
			if( g_languageTable->DoesTranslationExist( "gameoption_PlayerName" ) )
			{
				strncpy( m_teamName, LANGUAGEPHRASE("gameoption_PlayerName"), sizeof( m_teamName ) );
				m_teamName[ sizeof( m_teamName ) - 1 ] = '\x0';
			}
		}

		SafetyString( m_teamName );
		ReplaceExtendedCharacters( m_teamName );

		found = true;
	}

	if( !found )
	{
		m_teamId = -2;
		strncpy( m_teamName, LANGUAGEPHRASE("unknown"), sizeof( m_teamName ) );
		m_teamName[ sizeof( m_teamName ) - 1 ] = '\x0';
	}

	SafetyString( m_teamName );
	ReplaceExtendedCharacters( m_teamName );

	if( m_teamId == -1 )
	{
		SetTitle( "dialog_playername", true );
	}
	else
	{
		SetTitle( "dialog_teamname", true );
	}
}

void RenameTeamWindow::Create()
{
    CreateValueControl( "TeamName", 20, 50, m_w-40, 20, InputField::TypeString, &m_teamName, -1, 0, 16, NULL, " ", false, true );

    ApplyTeamNameButton *apb = new ApplyTeamNameButton();
    apb->SetProperties( "Apply", 10, m_h-25, 100, 19, "dialog_apply", " ", true, false );
    RegisterButton( apb );

    CloseButton *close = new CloseButton();
    close->SetProperties( "Close", m_w - 110, m_h-25, 100, 19, "dialog_close", " ", true, false );
    RegisterButton( close );

    EclSetCurrentFocus( m_name );
    strcpy( m_currentTextEdit, "TeamName" );
}

void RenameTeamWindow::Render( bool hasFocus )
{
    InterfaceWindow::Render( hasFocus );

	if( m_teamId == -1 )
	{
		g_renderer->TextCentreSimple( m_x + m_w / 2, m_y+30, White, 15.0f, LANGUAGEPHRASE("dialog_setplayername") );
	}
	else
	{
		g_renderer->TextCentreSimple( m_x + m_w / 2, m_y+30, White, 15.0f, LANGUAGEPHRASE("dialog_setteamname") );
	}
}
