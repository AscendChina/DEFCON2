#include "lib/universal_include.h"

#include <stdio.h>

#include "lib/gucci/window_manager.h"
#include "lib/gucci/input.h"
#include "lib/resource/resource.h"
#include "lib/language_table.h"
#include "lib/sound/soundsystem.h"

#include "app/app.h"
#include "app/globals.h"
#include "app/game.h"

#include "network/ClientToServer.h"
#include "network/ServerToClient.h"
#include "network/Server.h"

#include "world/world.h"

#include "renderer/map_renderer.h"

#include "interface/interface.h"
#include "interface/worldstatus_window.h"
#include "interface/alliances_window.h"


class TimeScaleButton : public InterfaceButton
{
public:
    Fixed m_timeScale;

    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        int myTeamId = g_app->GetWorld()->m_myTeamId;

        bool speedControllable = g_app->GetWorld()->CanSetTimeFactor( m_timeScale );

        if( speedControllable )
        {
            float oldAlpha = g_renderer->m_alpha;            

            if( g_app->GetWorld()->GetTimeScaleFactor() == m_timeScale )
            {
                g_renderer->m_alpha = 1.0f;
                InterfaceButton::Render( realX, realY, highlighted, true );
                glColor4f( 1.0f, 1.0f, 1.0f, 0.95f );
            }
            else
            {
                g_renderer->m_alpha = 0.6f;
                InterfaceButton::Render( realX, realY, highlighted, clicked );
                glColor4f( 1.0f, 1.0f, 1.0f, 0.7f );
            }

            g_renderer->m_alpha = oldAlpha;
        }
        else
        {
            glColor4f( 1.0f, 1.0f, 1.0f, 0.5f );
            if( g_app->GetWorld()->GetTimeScaleFactor() == m_timeScale )
            {
                g_renderer->RectFill( realX, realY, m_w, m_h, Colour(255,255,255,100) );
                glColor4f( 1.0f, 1.0f, 1.0f, 0.95f );
            }
        }


        char newTooltip[1024];
        sprintf( newTooltip, LANGUAGEPHRASE("tooltip_worldstatus_x_faster_realtime") );
		LPREPLACEINTEGERFLAG( 'S', m_timeScale.IntValue(), newTooltip );

        if( speedControllable )
        {
            switch( m_timeScale.IntValue() )
            {
                case GAMESPEED_PAUSED:      strcat( newTooltip, LANGUAGEPHRASE("tooltip_worldstatus_key_shortcut_0") );           break;
                case GAMESPEED_REALTIME:    strcat( newTooltip, LANGUAGEPHRASE("tooltip_worldstatus_key_shortcut_1") );           break;
                case GAMESPEED_SLOW:        strcat( newTooltip, LANGUAGEPHRASE("tooltip_worldstatus_key_shortcut_2") );           break;
                case GAMESPEED_MEDIUM:      strcat( newTooltip, LANGUAGEPHRASE("tooltip_worldstatus_key_shortcut_3") );           break;
                case GAMESPEED_FAST:        strcat( newTooltip, LANGUAGEPHRASE("tooltip_worldstatus_key_shortcut_4") );           break;
            }

            strcat( newTooltip, LANGUAGEPHRASE("tooltip_worldstatus_players_request_speed") );
        }
        

        //
        // Render arrows

        int numArrows = 0;

        if( m_timeScale == GAMESPEED_PAUSED )
        {
            float arrowX = realX + m_w/2;
            float arrowY = realY + m_h/2;

            g_renderer->RectFill( arrowX - 5, arrowY-4, 3, 9, White );
            g_renderer->RectFill( arrowX + 2, arrowY-4, 3, 9, White );
        }
        else
        {
            if( m_timeScale == GAMESPEED_REALTIME ) numArrows = 1;
            if( m_timeScale == GAMESPEED_SLOW ) numArrows = 2;
            if( m_timeScale == GAMESPEED_MEDIUM ) numArrows = 3;
            if( m_timeScale == GAMESPEED_FAST ) numArrows = 4;

            float arrowX = realX + m_w/2.0f - numArrows*2.5f;
            float arrowY = realY + m_h/2.0f;

            for( int i = 0; i < numArrows; ++i )
            {
                glBegin( GL_TRIANGLES );
                    glVertex2f( arrowX, arrowY-5 );
                    glVertex2f( arrowX+5, arrowY );
                    glVertex2f( arrowX, arrowY+5 );
                glEnd();

                arrowX += 5;
            }
        }
        
        //
        // Render requested game speeds

        if( speedControllable )
        {
            int validTeams = 0;
            for( int i = 0; i < MAX_TEAMS; ++i )
            {
                Team *team = g_app->GetWorld()->GetTeam(i);
                if( team && 
                    team->m_type != Team::TypeUnassigned &&
                    team->m_type != Team::TypeAI &&
                    team->m_desiredGameSpeed == m_timeScale.IntValue() )
                {
                    Colour col = team->GetTeamColour();
                    int x = realX + 1 + validTeams*2;                
                    g_renderer->Line( x, realY, x, realY+m_h, col, 2 );
                    validTeams++;

                    strcat( newTooltip, "\n- " );                    
                    strcat( newTooltip, team->m_name );
                }
            }

            if( validTeams == 0 )
            {
                strcat( newTooltip, LANGUAGEPHRASE("tooltip_worldstatus_no_team_request_speed") );
            }

            SetTooltip( newTooltip, false );        
        }
        else
        {
            SetTooltip( "tooltip_worldstatus_cannot_request_speed", true );
        }
    }
    
    void MouseUp()
    {
        bool speedSettable = g_app->GetWorld()->CanSetTimeFactor( m_timeScale );

        if( speedSettable )
        {
            g_app->GetClientToServer()->RequestGameSpeed( g_app->GetWorld()->m_myTeamId, m_timeScale.IntValue() );
        }
    }
};


class DefconButton : public InterfaceButton
{
public:
    int m_defcon;
    void Render( int realX, int realY, bool highlighted, bool clicked )
    {
        int defcon = g_app->GetWorld()->GetDefcon();
        if( m_defcon != defcon )
        {
            g_renderer->RectFill( realX, realY, m_w, m_h, Colour(100,100,100,100) );
            g_renderer->Rect( realX, realY, m_w, m_h, Colour(200,200,200,100) );
			if( m_captionIsLanguagePhrase )
			{
				g_renderer->Text( realX + 5, realY + 2, Colour(200,200,200,100), 15, LANGUAGEPHRASE(m_caption) );
			}
			else
			{
				g_renderer->Text( realX + 5, realY + 2, Colour(200,200,200,100), 15, m_caption );
			}
        }
        else
        {
            g_renderer->RectFill( realX, realY, m_w, m_h, Colour(50,50,255,255));
            g_renderer->Rect( realX, realY, m_w, m_h, White);
			if( m_captionIsLanguagePhrase )
			{
				g_renderer->TextSimple( realX + 5, realY + 2, White, 15, LANGUAGEPHRASE(m_caption) );
			}
			else
			{
				g_renderer->TextSimple( realX + 5, realY + 2, White, 15, m_caption );
			}
        }
    }
};


WorldStatusWindow::WorldStatusWindow( char *name )
:   FadingWindow( name ),
    m_lastKnownDefcon(4)
{
    SetSize( 420, 25 );
    SetPosition( g_windowManager->WindowW()/2 - m_w/2, 0 );

    LoadProperties( "WindowWorldStatus" );
}


WorldStatusWindow::~WorldStatusWindow()
{
    SaveProperties( "WindowWorldStatus" );
}


void WorldStatusWindow::Create()
{   
    //
    // Defcon buttons

    InvertedBox *invert = new InvertedBox();
    invert->SetProperties( "DefconBox", 72, 2, 93, 17, " ", " ", false, false );
    RegisterButton( invert );

    for( int i = 5; i >= 1; --i )
    {
        int w = 17;
        int x = 72 + (5-i) * (w+2);
        char number[64];
        sprintf( number, "%d", i );

        char tooltip[64];
        sprintf( tooltip, "tooltip_defcon%d", i );

        DefconButton *button = new DefconButton();
        button->SetProperties( number, x, 2, w, w, number, tooltip, false, true );
        button->m_defcon = i;
        RegisterButton(button);
    }    



    //
    // Time accel buttons

    int x = 250;
    int w = 25;
    int h = 17;
    int g = w+2;

    if( g_app->GetGame()->GetOptionValue("SlowestSpeed") == 0 )
    {
        TimeScaleButton *pause = new TimeScaleButton();
        pause->SetProperties( "Pause", x+=g, 2, w, h, " ", "tooltip_timeaccel0", false, true );
        pause->m_timeScale = GAMESPEED_PAUSED;
        RegisterButton( pause );
    }
    
    TimeScaleButton *realtime = new TimeScaleButton();
    realtime->SetProperties( "RealTime", x+=g, 2, w, h, " ", "tooltip_timeaccel1", false, true );
    realtime->m_timeScale = GAMESPEED_REALTIME;
    RegisterButton( realtime );

    TimeScaleButton *five = new TimeScaleButton();
    five->SetProperties( "Slow", x+=g, 2, w, h, " ", "tooltip_timeaccel2", false, true );
    five->m_timeScale = GAMESPEED_SLOW;
    RegisterButton( five );

    TimeScaleButton *ten = new TimeScaleButton();
    ten->SetProperties( "Medium", x+=g, 2, w, h, " ", "tooltip_timeaccel3", false, true );
    ten->m_timeScale = GAMESPEED_MEDIUM;
    RegisterButton( ten );

    TimeScaleButton *twenty = new TimeScaleButton();
    twenty->SetProperties( "Fast", x+=g, 2, w, h, " ", "tooltip_timeaccel4", false, true );
    twenty->m_timeScale = GAMESPEED_FAST;
    RegisterButton( twenty );
}


void WorldStatusWindow::Render( bool hasFocus )
{
    int transparency = g_renderer->m_alpha * 255;

    FadingWindow::Render( hasFocus, true );
        

    //
    // Render defcon

    g_renderer->SetFont( "kremlin" );
    g_renderer->TextSimple( m_x + 5, m_y + 4,  White, 15, LANGUAGEPHRASE("dialog_worldstatus_defcon") );
    g_renderer->SetFont();
    int defCon = g_app->GetWorld()->GetDefcon();

    if( defCon != m_lastKnownDefcon )
    {
        char msg[128];
        sprintf( msg, LANGUAGEPHRASE("dialog_worldstatus_defcon_x") );
		LPREPLACEINTEGERFLAG( 'D', defCon, msg );
        g_app->GetInterface()->ShowMessage( 0, 0, -1, msg, true );
        m_lastKnownDefcon = defCon;

        if( defCon < 5 )
        {
            g_soundSystem->TriggerEvent( "Interface", "DefconChange" );
        }
    }


    //
    // Render the time

    Date *theDate = &g_app->GetWorld()->m_theDate;

    g_renderer->TextSimple( m_x + 190, m_y + 4, White, 15, theDate->GetTheDate() );


    //
    // Render the real world time remaining

    int maxGameTime = g_app->GetGame()->m_maxGameTime.IntValue();
    if( maxGameTime > 0.0f )
    {
        int minutes = maxGameTime / 60;
        int hours = minutes / 60;
        minutes -= hours * 60;
        int seconds = maxGameTime - hours * 60 * 60 - minutes * 60;

        char caption[256];
        Colour col = White;

        if( hours > 0 )
        {
			sprintf( caption, LANGUAGEPHRASE("dialog_worldstatus_time_remain_hms") );
			LPREPLACEINTEGERFLAG( 'H', hours, caption );
			LPREPLACEINTEGERFLAG( 'M', minutes, caption );
			char number[16];
			sprintf( number, "%02ds", seconds );
			LPREPLACESTRINGFLAG( 'S', number, caption );
        }
        else            
        {
			sprintf( caption, LANGUAGEPHRASE("dialog_worldstatus_time_remain_ms") );
			LPREPLACEINTEGERFLAG( 'M', minutes, caption );
			char number[16];
			sprintf( number, "%02ds", seconds );
			LPREPLACESTRINGFLAG( 'S', number, caption );

            col.Set(255,0,0,255);
        }

        g_renderer->TextCentreSimple( m_x+m_w/2, m_y + 30, col, 14, caption );
    }

}



// ============================================================================


class StatsButton : public InterfaceButton
{    
    void MouseUp()
    {
        if( !EclGetWindow( "Stats" ) )
        {
            EclRegisterWindow( new StatsWindow(), m_parent );
        }
    }
};


ScoresWindow::ScoresWindow()
:   FadingWindow( "Scores" )
{
    SetSize( 170, 150 );
    SetPosition( g_windowManager->WindowW()-m_w, 0);

    //LoadProperties( "WindowScores" );
}


ScoresWindow::~ScoresWindow()
{
    SaveProperties( "WindowScores" );
}


void ScoresWindow::Create()
{
    StatsButton *stats = new StatsButton();
    stats->SetProperties( "Show Stats", 10, m_h - 25, m_w - 20, 18, "dialog_worldstatus_show_stats", " ", true, false );
    RegisterButton( stats );
}


void ScoresWindow::Render( bool _hasFocus )
{
    FadingWindow::Render( _hasFocus, false );

    //
    // Order the teams based on their scores
    // Lowest first

    LList<int> teamOrdering;
    
    for( int i = 0; i <  g_app->GetWorld()->m_teams.Size(); ++i )
    {
        Team *team = g_app->GetWorld()->m_teams[i];
        int teamScore = g_app->GetGame()->GetScore(team->m_teamId);

        bool added = false;
        for( int j = 0; j < teamOrdering.Size(); ++j )
        {
            int thisScore = g_app->GetGame()->GetScore( teamOrdering[j] );

            if( teamScore < thisScore )
            {
                teamOrdering.PutDataAtIndex( team->m_teamId, j );
                added = true;
                break;
            }
        }

        if( !added )
        {
            teamOrdering.PutDataAtEnd(team->m_teamId);
        }
    }


    //
    // Render the scores based on their ordering

    float xPos = m_x + m_w - 10;
    float yPos = m_y + 5;
    float size = 13;

    GameOption *scoreMode = g_app->GetGame()->GetOption("ScoreMode");
    g_renderer->TextSimple( m_x + 10, yPos+2, LightGray, 11, LANGUAGEPHRASE("dialog_worldstatus_scoremode") );
	g_renderer->TextRightSimple( m_x + m_w - 10, yPos+2, White, 11, GameOption::TranslateValue( scoreMode->m_subOptions[scoreMode->m_currentValue] ) );
    yPos += 15;

    g_renderer->Line( m_x + 10, yPos, m_x + m_w - 10, yPos, LightGray, 1.0f );

    yPos += 3;

    for( int i = teamOrdering.Size()-1; i >= 0; --i )
    {
        Team *team = g_app->GetWorld()->GetTeam(teamOrdering[i]);
        Colour col = team->GetTeamColour();
        int score = g_app->GetGame()->GetScore( teamOrdering[i] );

        char teamName[256];
        if( team->m_type == Team::TypeAI )
        {
            if( g_app->GetGame()->m_winner == -1 )
            {
				sprintf( teamName, LANGUAGEPHRASE("dialog_worldstatus_cpu_team_name") );
				LPREPLACESTRINGFLAG( 'T', team->GetTeamName(), teamName );
            }
            else
            {
				sprintf( teamName, LANGUAGEPHRASE("dialog_worldstatus_left_team_name") );
				LPREPLACESTRINGFLAG( 'T', team->GetTeamName(), teamName );
            }
        }
        else
        {
            if( g_app->GetClientToServer()->IsClientDemo( team->m_clientId ) )
            {
				sprintf( teamName, LANGUAGEPHRASE("dialog_worldstatus_demo_team_name") );
				LPREPLACESTRINGFLAG( 'T', team->GetTeamName(), teamName );
            }
            else
            {
                sprintf( teamName, "%s", team->GetTeamName() );
            }
        }

        g_renderer->TextSimple( m_x+10, yPos, col, size, teamName );
        g_renderer->TextRight( xPos, yPos, White, size + 1, "%d", score );
        
        if( team->m_type != Team::TypeAI &&
            !g_app->GetClientToServer()->IsSynchronised(team->m_clientId) )
        {
            g_renderer->Text( xPos - 225, yPos, Colour(255,0,0,255), size, LANGUAGEPHRASE("dialog_worldstatus_out_of_sync") );
        }

#ifdef _DEBUG
        if( g_app->GetServer() && team->m_type != Team::TypeAI )
        {
            ServerToClient *client = g_app->GetServer()->GetClient( team->m_clientId );
            if ( client )
            {
                g_renderer->TextRight( m_x, yPos, Colour(255,255,255,150), size, client->m_system );
            }
        }
#endif

        yPos += 15;
    }


    //
    // Render spectator names

    if( g_app->GetWorld()->m_spectators.Size() > 0 )
    {
        yPos += 15;
        g_renderer->TextSimple( m_x+10, yPos, White, 13, LANGUAGEPHRASE("dialog_spectators") );

        for( int i = 0; i < g_app->GetWorld()->m_spectators.Size(); ++i )
        {
            yPos += 15;

            Spectator *spec = g_app->GetWorld()->m_spectators[i];

            char fullName[256];
            if( strcmp( spec->m_name, "Spectator" ) == 0 ) 
            {
                sprintf( fullName, LANGUAGEPHRASE("dialog_worldstatus_spectator_join") );
            }
            else if( g_app->GetClientToServer()->IsClientDemo( spec->m_clientId ) )
            {
				sprintf( fullName, LANGUAGEPHRASE("dialog_worldstatus_demo_team_name") );
				LPREPLACESTRINGFLAG( 'T', spec->m_name, fullName );
            }
            else 
            {
                sprintf( fullName, "%s", spec->m_name );
            }

            g_renderer->Text( m_x+10, yPos, Colour(255,255,255,128), 12, fullName );

            if( !g_app->GetClientToServer()->IsSynchronised(spec->m_clientId) )
            {
                g_renderer->Text( xPos - 225, yPos, Colour(255,0,0,255), size, LANGUAGEPHRASE("dialog_worldstatus_out_of_sync_2") );
            }

        }
    }


    if( m_y + m_h < yPos + 50 )
    {
        m_h = ( yPos + 50 ) - m_y;
        GetButton( "Show Stats" )->m_y = m_h - 25;
    }
}



// ============================================================================



StatsWindow::StatsWindow()
:   InterfaceWindow( "Stats", "dialog_stats", true )
{
    SetSize( 580, 110 + 20 * g_app->GetWorld()->m_teams.Size() );
}


void StatsWindow::Create()
{
    InterfaceWindow::Create();

    InvertedBox *box = new InvertedBox();
    box->SetProperties( "invert", 10, 50, m_w-20, m_h-80, " ", " ", false, false );
    RegisterButton(box);
}


void StatsWindow::Render( bool hasFocus )
{
    InterfaceWindow::Render( hasFocus );

    int titleSize = 11;
    int textSize = 12;

    int superPowerX = m_x + 15;
    int killsX = superPowerX + 100;
    int deathsX = killsX + 60;
    int collatoralX = deathsX + 60;
    int survivorsX = collatoralX + 80;
    int nukesX = survivorsX + 90;
    int scoreX = nukesX + 100;

    int yPos = m_y + 25;

    g_renderer->TextSimple( superPowerX,  yPos, White, titleSize, LANGUAGEPHRASE("dialog_worldstatus_superpower") );
    g_renderer->TextSimple( killsX,       yPos, White, titleSize, LANGUAGEPHRASE("dialog_worldstatus_kills") );
    g_renderer->TextSimple( deathsX,      yPos, White, titleSize, LANGUAGEPHRASE("dialog_worldstatus_deaths") );
    g_renderer->TextSimple( survivorsX,   yPos, White, titleSize, LANGUAGEPHRASE("dialog_worldstatus_survivors") );
    g_renderer->TextSimple( collatoralX,  yPos, White, titleSize, LANGUAGEPHRASE("dialog_worldstatus_collateral") );
    g_renderer->TextSimple( nukesX,       yPos, White, titleSize, LANGUAGEPHRASE("dialog_worldstatus_nukes") );
    g_renderer->TextSimple( scoreX,       yPos, White, titleSize, LANGUAGEPHRASE("dialog_worldstatus_score") );
        
    yPos = m_y + m_h - 20;

    g_renderer->TextSimple( superPowerX,  yPos, White, textSize, LANGUAGEPHRASE("dialog_worldstatus_points_value") );
    g_renderer->Text( killsX,       yPos, White, textSize, "%d", g_app->GetGame()->m_pointsPerKill );
    g_renderer->Text( deathsX,      yPos, White, textSize, "%d", g_app->GetGame()->m_pointsPerDeath );
    g_renderer->Text( survivorsX,   yPos, White, textSize, "%d", g_app->GetGame()->m_pointsPerSurvivor );
    g_renderer->Text( collatoralX,  yPos, White, textSize, "%d", g_app->GetGame()->m_pointsPerCollatoral );
    g_renderer->Text( nukesX,       yPos, White, textSize, "%d", g_app->GetGame()->m_pointsPerNuke );


    //
    // Order the teams based on their scores
    // Lowest first

    LList<int> teamOrdering;
    
    for( int i = 0; i <  g_app->GetWorld()->m_teams.Size(); ++i )
    {
        Team *team = g_app->GetWorld()->m_teams[i];
        int teamScore = g_app->GetGame()->GetScore(team->m_teamId);

        bool added = false;
        for( int j = 0; j < teamOrdering.Size(); ++j )
        {
            int thisScore = g_app->GetGame()->GetScore( teamOrdering[j] );

            if( teamScore < thisScore )
            {
                teamOrdering.PutDataAtIndex( team->m_teamId, j );
                added = true;
                break;
            }
        }

        if( !added )
        {
            teamOrdering.PutDataAtEnd(team->m_teamId);
        }
    }

    
    //
    // Display the scores in order
    
    yPos = m_y + 60;

    for( int i = teamOrdering.Size()-1; i>=0; --i )
    {
        int teamId = teamOrdering[i];
        Team *team = g_app->GetWorld()->GetTeam(teamId);
        Colour col = team->GetTeamColour();
        
        char teamName[256];
        if( team->m_type == Team::TypeAI )
        {
			sprintf( teamName, LANGUAGEPHRASE("dialog_worldstatus_cpu_team_name") );
			LPREPLACESTRINGFLAG( 'T', team->GetTeamName(), teamName );
        }
        else
        {
            strcpy( teamName, team->GetTeamName() );
        }

        char kills[64];
        char deaths[64];
        char survivors[64];
        char nukes[64];
        char collatoral[64];
        char score[64];

        float startintPop = g_app->GetGame()->GetOptionValue( "PopulationPerTerritory" );
        startintPop *= g_app->GetGame()->GetOptionValue( "TerritoriesPerTeam" );

        float numSurvivors = startintPop - team->m_friendlyDeaths/1000000.0f;
        
		char number[32];

        sprintf( number, "%.1f", team->m_enemyKills / 1000000.0f );
		sprintf( kills, LANGUAGEPHRASE("dialog_worldstatus_pop_in_millions") );
		LPREPLACESTRINGFLAG( 'P', number, kills );

        sprintf( number, "%.1f", team->m_friendlyDeaths / 1000000.0f );
		sprintf( deaths, LANGUAGEPHRASE("dialog_worldstatus_pop_in_millions") );
		LPREPLACESTRINGFLAG( 'P', number, deaths );

        sprintf( number, "%.1f", numSurvivors );
		sprintf( survivors, LANGUAGEPHRASE("dialog_worldstatus_pop_in_millions") );
		LPREPLACESTRINGFLAG( 'P', number, survivors );

        sprintf( number, "%.1f", team->m_collatoralDamage / 1000000.0f );
		sprintf( collatoral, LANGUAGEPHRASE("dialog_worldstatus_pop_in_millions") );
		LPREPLACESTRINGFLAG( 'P', number, collatoral );

        sprintf( score, "%d", g_app->GetGame()->GetScore(teamId) );
        
        if( g_app->GetGame()->m_pointsPerNuke > 0 ||
            g_app->GetGame()->m_winner != -1 ||
            g_app->GetWorld()->m_myTeamId == -1 ||
            g_app->GetWorld()->IsFriend( teamId, g_app->GetWorld()->m_myTeamId ) )
        {
            sprintf( nukes, "%d", g_app->GetGame()->m_nukeCount[teamId]  );
        }
        else
        {
            sprintf( nukes, LANGUAGEPHRASE("dialog_worldstatus_classified") );
        }

        g_renderer->TextSimple( superPowerX,  yPos, col, textSize, teamName );
        g_renderer->TextSimple( killsX,       yPos, col, textSize, kills );
        g_renderer->TextSimple( deathsX,      yPos, col, textSize, deaths );
        g_renderer->TextSimple( survivorsX,   yPos, col, textSize, survivors );
        g_renderer->TextSimple( collatoralX,  yPos, col, textSize, collatoral );
        g_renderer->TextSimple( nukesX,       yPos, col, textSize, nukes );
        g_renderer->TextSimple( scoreX,       yPos, col, textSize, score );

        yPos += 25;
    }    
}



