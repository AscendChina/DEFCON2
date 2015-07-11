#include "lib/universal_include.h"

#include <time.h>

#include "lib/gucci/window_manager.h"
#include "lib/math/vector3.h"
#include "lib/math/random_number.h"
#include "lib/profiler.h"
#include "lib/gucci/input.h"
#include "lib/language_table.h"
#include "lib/resource/resource.h"
#include "lib/preferences.h"
#include "lib/sound/soundsystem.h"
#include "lib/metaserver/authentication.h"
#include "lib/metaserver/metaserver.h"
#include "lib/metaserver/metaserver_defines.h"
#include "lib/eclipse/eclipse.h"

#include "network/ClientToServer.h"
#include "network/Server.h"

#include "app/globals.h"
#include "app/app.h"
#include "lib/multiline_text.h"
#include "app/version_manager.h"

#include "world/world.h"
#include "world/earthdata.h"
#include "world/city.h"

#include "lobby_renderer.h"
#include "map_renderer.h"


LobbyRenderer::LobbyRenderer()
:   m_maxOverlays(0),
    m_currentOverlay(-1),
    m_previousOverlay(0),
    m_overlayTimer(0.0f),
    m_maxStrings(0),
    m_finishTimer(-1.0f),
    m_currentCity(-1),
    m_lastLanguage(NULL)
{
}


LobbyRenderer::~LobbyRenderer()
{
    if( m_lastLanguage )
    {
        delete [] m_lastLanguage;
        m_lastLanguage = NULL;
    }
}


void LobbyRenderer::Initialise()
{
    InitialiseLanguage();

    m_overlayTimer = GetHighResTime() + 6.0f;
}


void LobbyRenderer::InitialiseLanguage()
{
    // Overlay

    while( true )
    {
        char stringId[256];
        sprintf( stringId, "overlay_%d_1", m_maxOverlays );

        if( !g_languageTable->DoesPhraseExist( stringId ) && !g_languageTable->DoesTranslationExist( stringId ) )
        {
            break;
        }

        ++m_maxOverlays;
    }


    // Motd

    MetaServer_RemoveData( NET_METASERVER_DATA_MOTD );
}


void LobbyRenderer::Render()
{
    START_PROFILE( "LobbyRenderer" );

    if( g_languageTable->m_lang && ( !m_lastLanguage || stricmp( g_languageTable->m_lang->m_name, m_lastLanguage ) != 0 ) )
    {
        if( m_lastLanguage )
        {
            delete [] m_lastLanguage;
        }
        m_lastLanguage = newStr( g_languageTable->m_lang->m_name );

        InitialiseLanguage();
    }

    if( g_preferences->GetInt(PREFS_GRAPHICS_LOBBYEFFECTS) == 1 )
    {
        SetupCamera3d();
        
        START_PROFILE( "Globe" );
        RenderGlobe();
        END_PROFILE( "Globe" );

        //START_PROFILE( "CityDetails" );
        //RenderCityDetails();
        //END_PROFILE( "CityDetails" );
    }

    g_renderer->Reset2DViewport();

    START_PROFILE( "Border" );
    RenderBorder();
    END_PROFILE( "Border" );

    START_PROFILE( "Title" );
    RenderTitle();
    END_PROFILE( "Title" );

    #ifdef SHOW_TEST_HOURS
        START_PROFILE( "Test Hours" );
        RenderTestHours();
        END_PROFILE( "Test Hours" );
    #endif

    START_PROFILE( "VersionInfo" );
    RenderVersionInfo();
    END_PROFILE( "VersionInfo" );
    START_PROFILE( "AuthStatus" );
    RenderAuthStatus();
    END_PROFILE( "AuthStatus" );
    START_PROFILE( "Motd" );
    RenderMotd();
    END_PROFILE( "Motd" );
    START_PROFILE( "DemoLimits" );
    RenderDemoLimits();
    END_PROFILE( "DemoLimits" );


    //RenderNetworkIdentity();

    if( g_preferences->GetInt(PREFS_GRAPHICS_LOBBYEFFECTS) == 1 )
    {
        START_PROFILE( "Overlay" );
        RenderOverlay();
        END_PROFILE( "Overlay" );
    }
    
    END_PROFILE( "LobbyRenderer" );
}


void LobbyRenderer::RenderTitle()
{
    g_renderer->SetFont( "kremlin" );
    g_renderer->TextRight( g_windowManager->WindowW() - 64,
                           50,
                           Colour(0,230,0,220),
                           100,
                           LANGUAGEPHRASE("dialog_lobby_defcon") );

// Mac version has its own website
#ifndef TARGET_OS_MACOSX
	char website[1024];
	memset( website, 0, sizeof(website) );

	char *tempwebsite = NULL;
#if defined(RETAIL_BRANDING_UK)
	tempwebsite = LANGUAGEPHRASE("website_main_retail_uk");
#elif defined(RETAIL_BRANDING)
	tempwebsite = LANGUAGEPHRASE("website_main_retail");
#elif defined(RETAIL_BRANDING_MULTI_LANGUAGE)
	tempwebsite = LANGUAGEPHRASE("website_main_retail_multi_language");
#else
	tempwebsite = LANGUAGEPHRASE("website_main");
#endif

	int begin = 0;
	int iwebsite = 0;
	for( int i = 0; i < strlen( tempwebsite ) && iwebsite < sizeof(website) - 2; i++ )
	{
		if( begin >= 2 )
		{
			if( iwebsite > 0 )
			{
				website[iwebsite] = ' ';
				iwebsite++;
			}
			website[iwebsite] = tempwebsite[i];
			iwebsite++;
		}
		else if( tempwebsite[i] == '/' )
		{
			begin++;
		}
	}

    strupr(tempwebsite);

    g_renderer->TextRight( g_windowManager->WindowW() - 64,
                           130,
                           Colour(0,200,0,220),
                           23,
                           website );
#else
    g_renderer->Text( g_windowManager->WindowW() - 410,
        130,
        Colour(0,200,0,220),
        20,
        LANGUAGEPHRASE("dialog_created_by_introversion") );
#endif

    g_renderer->SetFont();

}


void LobbyRenderer::RenderOverlay()
{
    float timeNow = GetHighResTime();

    //
    // Figure out which Overlay we are rendering

    if( m_currentOverlay == -1 )
    {
        if( timeNow > m_overlayTimer &&
            m_maxOverlays > 0 )
        {
            m_currentOverlay = m_previousOverlay;
            while( m_currentOverlay == m_previousOverlay )
            {
                m_currentOverlay = AppRandom() % m_maxOverlays;
            }
            m_overlayTimer = timeNow;
            m_previousOverlay = m_currentOverlay;
        }

        return;
    }
    

    //
    // Is the current overlay done?

    float timeSoFar = timeNow - m_overlayTimer;

    if( m_finishTimer > 0.0f && timeNow > m_finishTimer )
    {
        m_currentOverlay = -1;
        m_overlayTimer = timeNow + 5.0f;
        m_finishTimer = -1.0f;
        return;
    }


    //
    // How many strings are in this overlay?
    // Which is the widest?

    g_renderer->SetFont( "kremlin", false, false, true );
	float kremlinFontSpacingOld = g_renderer->GetFontSpacing( "kremlin" );
    g_renderer->SetFontSpacing( "kremlin", 0.2f );

    static int s_previousMaxChars=0;
    int maxChars = (timeSoFar*1000) / 12.0f;        
    int currentChars = 0;
    int lastLineChars = -1;

    int currentStringId = 1;
    m_maxStrings = 0;

    while( true )
    {
        char stringId[256];
        sprintf( stringId, "overlay_%d_%d", m_currentOverlay, currentStringId );

        if( !g_languageTable->DoesPhraseExist( stringId ) && !g_languageTable->DoesTranslationExist( stringId ) )
        {
            break;
        }

        char *string = LANGUAGEPHRASE(stringId);

        currentChars += strlen(string);

        if( currentChars <= maxChars )
        {
            ++m_maxStrings;            
        }  
        else if( lastLineChars == -1 )
        {
            //lastLineChars = strlen(string) - (currentChars - maxChars);
            lastLineChars = maxChars - (currentChars - strlen(string));
            Clamp( lastLineChars, 0, (int) strlen(string) );
        }
                
        ++currentStringId;
    }

    if( lastLineChars == -1 && m_finishTimer < 0.0f )
    {
        m_finishTimer = timeNow + 5.0f;
    }
    
    if( lastLineChars != -1 &&
        fabs(float(s_previousMaxChars - maxChars)) > 4 )
    {
        g_soundSystem->TriggerEvent( "Interface", "Text" );
        s_previousMaxChars = maxChars;
    }

    //
    // Render the current overlay
    
    float xPos = 60;
    float yPos = g_windowManager->WindowH() - 70;

    float gapSize = 2;
    bool cursor = false;
    
    g_renderer->SetClip( 0, 0, g_windowManager->WindowW() - 420, g_windowManager->WindowH() );

    for( int i = m_maxStrings+1; i > 0; --i )
    {
        char stringId[256];
        sprintf( stringId, "overlay_%d_%d", m_currentOverlay, i );

        if( g_languageTable->DoesPhraseExist(stringId) || g_languageTable->DoesTranslationExist( stringId ) )
        {
            char *string = LANGUAGEPHRASE(stringId);

            g_renderer->SetFont( "kremlin", false, false, true );

			MultiLineText wrapped( string, 10, 15, false );

            for( int j = wrapped.Size()-1; j >= 0; --j )
            {
                char *thisLine = wrapped[j];
                char thisLineCopy[512];
                strcpy( thisLineCopy, thisLine );
                char *thisLineCopyP = thisLineCopy;

                Colour thisCol(0,155,0,255);
                float fontSize = 15;

                if( thisLineCopy[0] == '-' ) 
                {               
                    thisLineCopyP++;
                    thisCol.Set(0,255,0,255);
                    fontSize += 1;
                }

                if( i == m_maxStrings+1 && j == 0 && lastLineChars != -1 )
                {                    
                    if( lastLineChars == 0 )        thisLineCopy[1] = '\x0';
                    else                            thisLineCopy[lastLineChars] = '\x0';                    
                }

                g_renderer->TextSimple( xPos, yPos, thisCol, fontSize, thisLineCopyP );

                if( !cursor )
                {
                    cursor = true;
                    if( lastLineChars != -1 || fmod(timeNow,1.0f) < 0.5f )
                    {
                        g_renderer->RectFill( xPos+g_renderer->TextWidth(thisLineCopyP, fontSize)+5,
                                              yPos, 10, 15, Colour(0,255,0,255) );
                    }
                }
                
                yPos -= fontSize;
                yPos -= gapSize;
            }
        }
    }

    g_renderer->SetFontSpacing( "kremlin", kremlinFontSpacingOld );
    g_renderer->SetFont();    
    g_renderer->ResetClip();
}


void LobbyRenderer::RenderBorder()
{
    float windowW = g_windowManager->WindowW();
    float windowH = g_windowManager->WindowH();
    float borderSize = 50;

    Colour fillCol(0,20,0,220);
    Colour lineCol(0,255,0,150);

    g_renderer->SetBlendMode( Renderer::BlendModeNormal );
    
    g_renderer->RectFill( 0, 0, windowW, borderSize, fillCol );
    g_renderer->RectFill( 0, windowH-borderSize, windowW, borderSize, fillCol );
    g_renderer->RectFill( 0, borderSize, borderSize, windowH-borderSize*2, fillCol );
    g_renderer->RectFill( windowW-borderSize, borderSize, borderSize, windowH-borderSize*2, fillCol );

    g_renderer->BeginLines( lineCol, 1 );
    g_renderer->Line( borderSize, borderSize );
    g_renderer->Line( windowW-borderSize, borderSize );
    g_renderer->Line( windowW-borderSize, windowH-borderSize );
    g_renderer->Line( borderSize, windowH-borderSize );
    g_renderer->Line( borderSize, borderSize );
    g_renderer->EndLines();
}


void LobbyRenderer::SetupCamera3d()
{
    float fov = 60.0f;
    float nearPlane = 0.1f;
    float farPlane = 10000.0f;
    float screenW = g_windowManager->WindowW();
    float screenH = g_windowManager->WindowH();

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(fov, screenW / screenH, nearPlane, farPlane);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    static float timeVal = 0.0f;
    timeVal += g_advanceTime * 1;
    float timeNow = timeVal;
    float camDist = 1.7f;       //+sinf(timeNow*0.2f)*0.5f;
    float camHeight = 0.5f + cosf(timeNow*0.2f) * 0.2f;
    Vector3<float> pos(0.0f,camHeight, camDist);
    pos.RotateAroundY( timeNow * -0.1f );
	Vector3<float> requiredFront = pos * -1.0f;
        
    static Vector3<float> front = Vector3<float>::ZeroVector();
    float timeFactor = g_advanceTime * 0.3f;
    front = requiredFront * timeFactor + front * (1-timeFactor);    
    
	Vector3<float> right = front ^ Vector3<float>::UpVector();
    Vector3<float> up = right ^ front;
    Vector3<float> forwards = pos + front * 100;
    
    m_camFront = front;
    m_camUp = up;
    
    m_camFront.Normalise();
    m_camUp.Normalise();
    
    gluLookAt(pos.x, pos.y, pos.z,
              forwards.x, forwards.y, forwards.z,
              up.x, up.y, up.z);

    glDisable( GL_CULL_FACE );    
    glDisable( GL_TEXTURE_2D );
    glEnable( GL_BLEND );
    glBlendFunc( GL_SRC_ALPHA, GL_ONE );
    glDisable( GL_LIGHTING );
    glDisable( GL_DEPTH_TEST );
   
    float fogCol[] = {  0, 0, 0, 0 };

	glHint		( GL_FOG_HINT, GL_DONT_CARE );
    glFogf      ( GL_FOG_DENSITY, 1.0f );
    glFogf      ( GL_FOG_START, camDist/2.0f );
    glFogf      ( GL_FOG_END, camDist*2.0f );
    glFogfv     ( GL_FOG_COLOR, fogCol );
    glFogi      ( GL_FOG_MODE, GL_LINEAR );
    glEnable    ( GL_FOG );
}


void LobbyRenderer::RenderGlobe()
{        
    unsigned int globeDisplayList;
    bool generated = g_resource->GetDisplayList( "LobbyGlobe", globeDisplayList );

    if( generated )
    {    
        glNewList(globeDisplayList, GL_COMPILE);
    
        //
        // Guide lines

        glColor4f( 0.3f, 1.0f, 0.3f, 0.2f );
        
        for( float x = -180; x < 180; x += 10 )
        {
            glBegin( GL_LINE_STRIP );
            for( float y = -90; y < 90; y += 10 )
            {
                Vector3<float> thisPoint(0,0,1);
                thisPoint.RotateAroundY( x/180.0f * M_PI );
                Vector3<float> right = thisPoint ^ Vector3<float>::UpVector();  
                right.Normalise();
                thisPoint.RotateAround( right * y/180.0f * M_PI );

                glVertex3fv( thisPoint.GetData() );
            }
            glEnd();
        }


        for( float y = -90; y < 90; y += 10 )
        {
            glBegin( GL_LINE_STRIP );
            for( float x = -180; x <= 180; x += 10 )
            {
                Vector3<float> thisPoint(0,0,1);
                thisPoint.RotateAroundY( x/180.0f * M_PI );
                Vector3<float> right = thisPoint ^ Vector3<float>::UpVector();   
                right.Normalise();
                thisPoint.RotateAround( right * y/180.0f * M_PI );

                glVertex3fv( thisPoint.GetData() );
            }
            glEnd();
        }

    
        //
        // Coastlines

        glColor4f( 0.0f, 1.0f, 0.0f, 1.0f );

        for( int i = 0; i < g_app->GetEarthData()->m_islands.Size(); ++i )
        {
            Island *island = g_app->GetEarthData()->m_islands[i];
            AppDebugAssert( island );

            glBegin( GL_LINE_STRIP );
            for( int j = 0; j < island->m_points.Size(); j++ )
            {
                Vector3<float> *thePoint = island->m_points[j];
            
                Vector3<float> thisPoint(0,0,1);
                thisPoint.RotateAroundY( thePoint->x/180.0f * M_PI );
                Vector3<float> right = thisPoint ^ Vector3<float>::UpVector();   
                right.Normalise();
                thisPoint.RotateAround( right * thePoint->y/180.0f * M_PI );
            
                glVertex3fv( thisPoint.GetData() );
            }
            glEnd();
        }

        //
        // Borders

        if( g_preferences->GetInt( PREFS_GRAPHICS_BORDERS ) == 1 )
        {
            glColor4f( 0.0f, 1.0f, 0.0f, 0.3f );

            for( int i = 0; i < g_app->GetEarthData()->m_borders.Size(); ++i )
            {
                Island *island = g_app->GetEarthData()->m_borders[i];
                AppDebugAssert( island );

                glBegin( GL_LINE_STRIP );
                for( int j = 0; j < island->m_points.Size(); j++ )
                {
                    Vector3<float> *thePoint = island->m_points[j];
                
                    Vector3<float> thisPoint(0,0,1);
                    thisPoint.RotateAroundY( thePoint->x/180.0f * M_PI );
                    Vector3<float> right = thisPoint ^ Vector3<float>::UpVector();   
                    right.Normalise();
                    thisPoint.RotateAround( right * thePoint->y/180.0f * M_PI );
                
                    glVertex3fv( thisPoint.GetData() );
                }
                glEnd();
            }
        }

        glEndList();
    }


    glCallList(globeDisplayList);
    
    glDisable( GL_FOG );
}


void LobbyRenderer::RenderVersionInfo()
{
    char currentVersion[256] = APP_NAME "  " APP_VERSION;
    strupr( currentVersion );

    float xPos = 50.0f;
    float yPos = g_windowManager->WindowH() - 47;

    Colour fontBold( 0, 255, 0, 255 );
    Colour fontNormal( 0, 255, 0, 155 );
    g_renderer->SetFont( "kremlin" );

    g_renderer->TextSimple( xPos, yPos, fontBold, 20, LANGUAGEPHRASE("dialog_lobby_version") );        
    g_renderer->TextSimple( xPos + 100, yPos, fontNormal, 20, currentVersion );

    g_renderer->SetFont();


    Directory *latestVersion = MetaServer_RequestData( NET_METASERVER_DATA_LATESTVERSION );
    if( latestVersion )
    {
        char *latestVersionString = latestVersion->GetDataString( NET_METASERVER_DATA_LATESTVERSION );
        
        Directory *updateUrlDir = MetaServer_RequestData( NET_METASERVER_DATA_UPDATEURL );
        char *updateUrl = updateUrlDir ? updateUrlDir->GetDataString( NET_METASERVER_DATA_UPDATEURL ) : NULL;

		float latestVersionFloat = VersionManager::VersionStringToNumber( latestVersionString );
		float curVersionFloat = VersionManager::VersionStringToNumber( APP_VERSION );
        if( strcmp( latestVersionString, APP_VERSION ) == 0 || curVersionFloat >= latestVersionFloat )
        {
            g_renderer->TextSimple( xPos, yPos + 33, fontNormal, 12, LANGUAGEPHRASE("dialog_lobby_have_latest_version") );
        }
        else
        {
            Colour col = fontBold;
            if( fmodf( GetHighResTime() * 2, 2 ) > 1.0f )
            {
                col.m_a = 50;
            }
			char caption[512];
			sprintf( caption, LANGUAGEPHRASE("dialog_lobby_newer_version") );
			LPREPLACESTRINGFLAG( 'V', latestVersionString, caption );
            g_renderer->TextSimple( xPos, yPos + 20, col, 12, caption );
            if( updateUrl ) 
            {
                g_renderer->TextSimple( xPos, yPos + 33, col, 12, LANGUAGEPHRASE("dialog_lobby_click_to_download") );

                float areaWidth = 250;
                bool mouseInArea = g_inputManager->m_mouseX > xPos &&
                                   g_inputManager->m_mouseX < xPos + areaWidth &&
                                   g_inputManager->m_mouseY > yPos &&
								   !EclGetWindow( g_inputManager->m_mouseX, g_inputManager->m_mouseY );

                //
                // If the user clicks in this area, take him to the download website

                if( mouseInArea )
                {
                    g_renderer->RectFill( xPos, yPos, areaWidth, 45, Colour(0,255,0,20) );
                    g_renderer->Rect( xPos, yPos, areaWidth, 45, fontBold );
                    
                    if( g_inputManager->m_lmbUnClicked )
                    {
                        int windowed = g_preferences->GetInt( "ScreenWindowed", 1 );
                        if( !windowed )
                        {
                            // Switch to windowed mode if required
                            g_preferences->SetInt( "ScreenWindowed", 1 );
                            g_preferences->SetInt( "ScreenWidth", 1024 );
                            g_preferences->SetInt( "ScreenHeight", 768 );

                            g_app->ReinitialiseWindow();

                            g_preferences->Save();        
                        }

                        g_windowManager->OpenWebsite( updateUrl );
                    }
                }
            }
        }

        delete latestVersion;
		if ( updateUrlDir )
			delete updateUrlDir;
    }
}


void LobbyRenderer::RenderAuthStatus()
{
    Colour fontBold( 0, 255, 0, 255 );
    Colour fontNormal( 0, 255, 0, 155 );
    g_renderer->SetFont( "kremlin" );

    char authKey[256];
    Authentication_GetKey( authKey );
    int authStatus = Authentication_GetStatus(authKey);
    char authStatusString[256];
    sprintf( authStatusString,"%s", LANGUAGEPHRASE(Authentication_GetStatusStringLanguagePhrase(authStatus)) );
    strupr( authStatusString );

    if( Authentication_IsDemoKey(authKey) &&
        authStatus >= AuthenticationUnknown )
    {
        sprintf( authStatusString, LANGUAGEPHRASE("dialog_auth_status_demo_user") );
    }
    else if( authStatus == AuthenticationUnknown )
    {
        fontNormal.m_a = 50;
    }

    float xPos = g_windowManager->WindowW()/2.0f;
    float yPos = g_windowManager->WindowH() - 47;

    g_renderer->TextCentreSimple( xPos, yPos, fontBold, 20, LANGUAGEPHRASE("dialog_auth_status") );
    g_renderer->TextCentreSimple( xPos, yPos+20, fontNormal, 30, authStatusString );

	g_renderer->SetFont();
}


void LobbyRenderer::RenderMotd()
{
    Directory *motd = MetaServer_RequestData( NET_METASERVER_DATA_MOTD );

    if( motd )
    {        
        float yPos = g_windowManager->WindowH() - 90;
        float xPos = g_windowManager->WindowW() - 400;

        Colour fillCol(0,20,0,150);
        Colour lineCol(0,255,0,150);

        float boxX = xPos - 10;
        static float boxY = 99999;
        float boxH = g_windowManager->WindowH() - 60 - boxY;
        float boxW = 350;
        g_renderer->RectFill( boxX, boxY, boxW, boxH, fillCol );
        g_renderer->Rect( boxX, boxY, boxW, boxH, lineCol );
		
		char *fullString = motd->GetDataString( NET_METASERVER_DATA_MOTD );
        g_renderer->SetFont("zerothre");
		MultiLineText wrapped( fullString, boxW-10, 15 );
        
        for( int i = wrapped.Size()-1; i >= 0; i-- )
        {
            char *thisLine = wrapped[i];

            Colour col(50,255,50,200);
            float fontSize = 15;
            if( thisLine[0] == '-' )
            {
                col.m_a = 255;
                thisLine++;
                fontSize = 20;
            }

            g_renderer->TextSimple( xPos, yPos, col, fontSize, thisLine );

            boxY = yPos-10;

            yPos -= fontSize;
            yPos -= 3;
        }
		g_renderer->SetFont();

        delete motd;
    }
}


void LobbyRenderer::RenderDemoLimits()
{
    if( g_app->GetClientToServer()->AmIDemoClient() )
    {
        int maxDemoGameSize;
        int maxDemoPlayers;
        bool allowDemoServers;

        g_app->GetClientToServer()->GetDemoLimitations( maxDemoGameSize, maxDemoPlayers, allowDemoServers );

        Colour fontBold( 0, 255, 0, 255 );
        Colour fontNormal( 0, 255, 0, 155 );

        float xPos = g_windowManager->WindowW() - 300;
        float yPos = g_windowManager->WindowH() - 47;

        g_renderer->SetFont( "kremlin" );
        g_renderer->TextSimple( xPos, yPos, fontBold, 15, LANGUAGEPHRASE("dialog_lobby_demo_limitations") );

        g_renderer->SetFont();
        g_renderer->TextSimple( xPos, yPos+15, fontNormal, 10, LANGUAGEPHRASE("dialog_lobby_max_demo_game_size") );
        g_renderer->TextSimple( xPos, yPos+25, fontNormal, 10, LANGUAGEPHRASE("dialog_lobby_max_demo_players") );
        g_renderer->TextSimple( xPos, yPos+35, fontNormal, 10, LANGUAGEPHRASE("dialog_lobby_demo_servers_permitted") );

		char caption[512];
		sprintf( caption, LANGUAGEPHRASE("dialog_lobby_number_players") );
		LPREPLACEINTEGERFLAG( 'P', maxDemoGameSize, caption );
        g_renderer->TextSimple( xPos+200, yPos+15, fontNormal, 10, caption );

		sprintf( caption, LANGUAGEPHRASE("dialog_lobby_number_players") );
		LPREPLACEINTEGERFLAG( 'P', maxDemoPlayers, caption );
        g_renderer->TextSimple( xPos+200, yPos+25, fontNormal, 10, caption );

        g_renderer->TextSimple( xPos+200, yPos+35, fontNormal, 10, allowDemoServers ? LANGUAGEPHRASE("dialog_yes_l") : LANGUAGEPHRASE("dialog_no_l") );


        //
        // If we are a demo, request price and buy-url from the metaserver
        // We don't use it here, but we want the data in advance of the buy screen appearing

        g_app->GetClientToServer()->GetPurchasePrice(NULL);
        g_app->GetClientToServer()->GetPurchaseUrl(NULL);
    }
}


void LobbyRenderer::RenderTestHours()
{
    int timeHours;
    int timeMinutes;
    
    bool success = false;
    Directory *result = MetaServer_RequestData( NET_METASERVER_DATA_TIME );
    if( result )
    {
        timeHours = result->GetDataInt( NET_METASERVER_TIME_HOURS );
        timeMinutes = result->GetDataInt( NET_METASERVER_TIME_MINUTES );
        success = true;
        delete result;
    }


    if( success )
    {
        //
        // Metaserver time

        float xPos = 50.0f;
        float yPos = g_windowManager->WindowH() - 47;
        
        char *amPm = (timeHours >= 12 ? LANGUAGEPHRASE("dialog_time_pm") : LANGUAGEPHRASE("dialog_time_am") );
        if( timeHours > 12 ) timeHours -= 12;

        Colour fontBold( 0, 255, 0, 255 );
        Colour fontNormal( 0, 255, 0, 155 );

        g_renderer->SetFont( "kremlin" );
        g_renderer->TextSimple( xPos, yPos, fontBold, 20, LANGUAGEPHRASE("dialog_lobby_metaserver_time") );

		char caption[512];
		sprintf( caption, LANGUAGEPHRASE("dialog_lobby_time_gmt") );
		LPREPLACEINTEGERFLAG( 'H', timeHours, caption );
		char number[32];
		sprintf( number, "%02d", timeMinutes );
		LPREPLACESTRINGFLAG( 'M', number, caption );
		LPREPLACESTRINGFLAG( 'A', amPm, caption );

        g_renderer->TextSimple( xPos, yPos+20, fontNormal, 30, caption );

        time_t timeNow = time(NULL);
        tm *localTime = localtime(&timeNow);


        //
        // Local testing hours

        xPos = g_windowManager->WindowW()/2 - 100;
        
        g_renderer->TextSimple( xPos, yPos, fontBold, 20, LANGUAGEPHRASE("dialog_lobby_test_hours_start_at") );

        int testHour = 0;
        int closestHour = 99;
        int closestMinute = 99;

        for( int i = 0; i < 4; ++i )
        {
            int hourDif = testHour - timeHours - 1;
            if( hourDif < 0 ) hourDif = 24 + hourDif;
            int minuteDif = 60 - timeMinutes;
            
            int thisTestHour = localTime->tm_hour + hourDif + 1;
            if( thisTestHour > 24 ) thisTestHour -= 24;
            
            const char *amPm = (thisTestHour >= 12 ? LANGUAGEPHRASE("dialog_time_pm") : LANGUAGEPHRASE("dialog_time_am") );
            if( thisTestHour > 12 ) thisTestHour -= 12;
            g_renderer->Text( xPos + 60*i, yPos+20, fontNormal, 30, "%d%s", thisTestHour, amPm );

            if( hourDif < closestHour ) 
            {
                closestHour = hourDif;
                closestMinute = minuteDif;
            }
            
            testHour += 6;
        }


        //
        // Time to next test phase

        xPos = g_windowManager->WindowW() - 50;
        
        if( closestHour >= 5 )
        {
            g_renderer->TextRightSimple( xPos, yPos, fontBold, 20, LANGUAGEPHRASE("dialog_lobby_scheduled_test_hour") );
            g_renderer->TextRightSimple( xPos, yPos+20, fontNormal, 30, LANGUAGEPHRASE("dialog_lobby_occurring_now") );
        }
        else
        {
            g_renderer->TextRightSimple( xPos, yPos, fontBold, 20, LANGUAGEPHRASE("dialog_lobby_next_test_hour_begins") );
            if( closestHour == 0 )
            {
				char caption[512];
				sprintf( caption, LANGUAGEPHRASE("dialog_lobby_minutes") );
				LPREPLACEINTEGERFLAG( 'M', closestMinute, caption );
                g_renderer->TextRightSimple( xPos, yPos+20, fontNormal, 30, caption );
            }
            else
            {
				char caption[512];
				sprintf( caption, LANGUAGEPHRASE("dialog_lobby_hours_minutes") );
				LPREPLACEINTEGERFLAG( 'H', closestHour, caption );
				LPREPLACEINTEGERFLAG( 'M', closestMinute, caption );
                g_renderer->TextRightSimple( xPos, yPos+20, fontNormal, 30, caption );
            }
        }

		g_renderer->SetFont();
    }
}


void LobbyRenderer::RenderCityDetails()
{
    //
    // Pick a city

    while( m_currentCity == -1 )
    {
        m_currentCity = AppRandom() % g_app->GetEarthData()->m_cities.Size();

        City *city = g_app->GetEarthData()->m_cities[m_currentCity];
        if( city->m_population < 1000000 && !city->m_capital )
        {
            m_currentCity = -1;
        }
    }

    
    City *city = g_app->GetEarthData()->m_cities[m_currentCity];


    //
    // Render a highlight
    
    Vector3<float> thisPoint(0,0,1);
    thisPoint.RotateAroundY( city->m_longitude.DoubleValue()/180.0f * M_PI );
    Vector3<float> right = thisPoint ^ Vector3<float>::UpVector();   
    right.Normalise();
    thisPoint.RotateAround( right * city->m_latitude.DoubleValue()/180.0f * M_PI );

    Vector3<float> camright = m_camUp ^ m_camFront;
    Vector3<float> camup = m_camUp;

    glColor4f( 0.0f, 1.0f, 0.0f, 1.0f );

    float boxSize = 0.1f;

    glBegin( GL_LINE_LOOP );
        glVertex3fv( (thisPoint - camright*boxSize - camup*boxSize).GetData() );
        glVertex3fv( (thisPoint + camright*boxSize - camup*boxSize).GetData() );
        glVertex3fv( (thisPoint + camright*boxSize + camup*boxSize).GetData() );
        glVertex3fv( (thisPoint - camright*boxSize + camup*boxSize).GetData() );
    glEnd();



    //
    // Choose another city now and then

    
}


void LobbyRenderer::RenderNetworkIdentity()
{
    g_renderer->SetFont( "kremlin" );


    //
    // Authentication status

    char authKey[256];
    Authentication_GetKey( authKey );
    int authStatus = Authentication_GetStatus(authKey);
    char *authStatusString = LANGUAGEPHRASE(Authentication_GetStatusStringLanguagePhrase(authStatus));

    char caption[256];
    sprintf( caption, LANGUAGEPHRASE("dialog_lobby_authentication") );
	LPREPLACESTRINGFLAG( 'A', authStatusString, caption );
    strupr( caption );

    Colour col;
    if( authStatus < 0 )        col.Set(255,0,0,255);
    else if( authStatus == 0 )  col.Set(200,200,30,255);
    else                        col.Set(0,255,0,255);

    g_renderer->TextSimple( 60, 60, col, 14, caption );


    //
    // Client identity

    if( g_app->GetClientToServer() )
    {
        char publicIP[256];
        int publicPort = -1;
        bool identityKnown = g_app->GetClientToServer()->GetIdentity( publicIP, &publicPort );

        if( identityKnown )
        {
			sprintf( caption, LANGUAGEPHRASE("dialog_lobby_client_ident") );
			LPREPLACESTRINGFLAG( 'I', publicIP, caption );
			LPREPLACEINTEGERFLAG( 'P', publicPort, caption );
            col.Set(0,255,0,255);
        }
        else
        {
            sprintf( caption, LANGUAGEPHRASE("dialog_lobby_client_ident_unknown") );
            col.Set(200,200,30,255);
        }

        g_renderer->TextSimple( 60, 75, col, 14, caption );
    }


    //
    // Server identity

    if( g_app->GetServer() )
    {
        char publicIP[256];
        int publicPort = -1;
        bool identityKnown = g_app->GetServer()->GetIdentity( publicIP, &publicPort );

        char caption[512];
        Colour col;

        if( identityKnown )
        {
			sprintf( caption, LANGUAGEPHRASE("dialog_lobby_server_ident") );
			LPREPLACESTRINGFLAG( 'I', publicIP, caption );
			LPREPLACEINTEGERFLAG( 'P', publicPort, caption );
            col.Set(0,255,0,255);
        }
        else
        {
            sprintf( caption, LANGUAGEPHRASE("dialog_lobby_server_ident_unknown") );
            col.Set(200,200,30,255);
        }

        g_renderer->TextCentreSimple( 60, 85, col, 14, caption );
    }

    g_renderer->SetFont();   
}
