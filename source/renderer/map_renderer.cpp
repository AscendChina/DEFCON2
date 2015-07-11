#include "lib/universal_include.h"

#include <math.h>

#include "lib/eclipse/eclipse.h"
#include "lib/gucci/input.h"
#include "lib/gucci/window_manager.h"
#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/render/renderer.h"
#include "lib/render/colour.h"
#include "lib/render/styletable.h"
#include "lib/profiler.h"
#include "lib/language_table.h"
#include "lib/preferences.h"
#include "lib/math/math_utils.h"
#include "lib/math/random_number.h"
#include "lib/hi_res_time.h"
#include "lib/sound/soundsystem.h"
#include "lib/resource/image.h"
#include "lib/resource/bitmap.h"

#include "app/app.h"
#include "app/globals.h"
#include "lib/multiline_text.h"
#include "app/tutorial.h"
#include "app/game.h"
#ifdef TARGET_OS_MACOSX
#include "app/macutil.h"
#endif

#include "network/ClientToServer.h"

#include "interface/components/core.h"
#include "interface/interface.h"
#include "interface/worldobject_window.h"

#include "renderer/map_renderer.h"
#include "renderer/animated_icon.h"

#include "world/world.h"
#include "world/earthdata.h"
#include "world/worldobject.h"
#include "world/city.h"
#include "world/nuke.h"
#include "world/node.h"
#include "world/blip.h"
#include "world/fleet.h"
#include "world/whiteboard.h"


MapRenderer::MapRenderer()
:   m_middleX(0.0f),
    m_middleY(0.0f),
    m_zoomFactor(1),
    m_currentHighlightId(-1),
    m_currentSelectionId(-1),
    m_showRadar(false),
    m_showNodes(true),
    m_showPopulation(false),
    m_showOrders(false),
    bmpRadar(NULL),
    m_highlightTime(0.0f),
    m_totalZoom(0),
    m_oldMouseX(0.0f),
    m_oldMouseY(0.0f),
    m_mouseIdleTime(0.0f),
    m_renderEverything(false),
    m_radarLocked(false),
    m_stateRenderTime(0.0f),
    m_highlightUnit(-1),
    m_showAllTeams(false),
    m_cameraLongitude(0.0f),
    m_cameraLatitude(0.0f),
    m_speedRatioX(0.0f),
    m_speedRatioY(0.0f),
    m_cameraIdleTime(0.0f),
    m_cameraZoom(0),
    m_autoCam(false),
    m_autoCamCounter(0),
    m_camSpeed(0),
    m_camAtTarget(true),
    m_autoCamTimer(20.0f),
    m_lockCamControl(false),
    m_showNukeUnits(false),
    m_currentFrames(0),
    m_framesPerSecond(0),
    m_frameCountTimer(1.0f),
    m_showFps(false),
    m_lockCommands(false),
    m_draggingCamera(false),
    m_tooltip(NULL),
    m_tooltipTimer(0.0f),
	m_showWhiteBoard(false),
	m_editWhiteBoard(false),
	m_showPlanning(false),
	m_showAllWhiteBoards(true),
	m_drawingPlanning(false),
	m_erasingPlanning(false),
	m_drawingPlanningTime(0.0f),
	m_longitudePlanningOld(0.0f),
	m_latitudePlanningOld(0.0f)
{
    for( int i = 0; i < MAX_TEAMS; ++i )
    {
        m_showTeam[i] = false;
    }

    for( int i = 0; i < World::NumTerritories; ++i )
    {
        m_territories[i] = NULL;
    }
}

static void DeleteAndDestroyDisplayList( const char *name )
{
    unsigned int displayListId = 0;
	if( !g_resource->GetDisplayList( name, displayListId ) )
	{
		glDeleteLists( displayListId, 1 );
		g_resource->DeleteDisplayList( name );
	}
}

MapRenderer::~MapRenderer()
{
	DeleteAndDestroyDisplayList( "MapCoastlines" );
	DeleteAndDestroyDisplayList( "MapBorders" );
    for( int i = 0; i < MAX_TEAMS; ++i )
    {
		char whiteboardname[32];
		snprintf( whiteboardname, sizeof(whiteboardname), "WhiteBoard%d", i );
		DeleteAndDestroyDisplayList( whiteboardname );
    }
}

void MapRenderer::Init()
{
    bmpRadar        = g_resource->GetImage( "graphics/radar.bmp" );
    bmpBlur         = g_resource->GetImage( "graphics/blur.bmp" );
    bmpWater        = g_resource->GetImage( "graphics/water.bmp" );
    bmpExplosion    = g_resource->GetImage( "graphics/explosion.bmp" );
    
    m_territories[World::TerritoryNorthAmerica] = g_resource->GetImage( "earth/northamerica.bmp" );
    m_territories[World::TerritoryRussia]       = g_resource->GetImage( "earth/russia.bmp" );
    m_territories[World::TerritorySouthAsia]    = g_resource->GetImage( "earth/southasia.bmp" );
    m_territories[World::TerritorySouthAmerica] = g_resource->GetImage( "earth/southamerica.bmp" );
    m_territories[World::TerritoryEurope]       = g_resource->GetImage( "earth/europe.bmp" );
    m_territories[World::TerritoryAfrica]       = g_resource->GetImage( "earth/africa.bmp" );    

    bmpTravelNodes = g_resource->GetImage( "earth/travel_nodes.bmp");
    bmpSailableWater = g_resource->GetImage( "earth/sailable.bmp" );
  
	sprintf(m_imageFiles[WorldObject::TypeSilo], "graphics/silo.bmp");
	sprintf(m_imageFiles[WorldObject::TypeRadarStation], "graphics/radarstation.bmp");
	sprintf(m_imageFiles[WorldObject::TypeSub], "graphics/sub.bmp");
	sprintf(m_imageFiles[WorldObject::TypeAirBase], "graphics/airbase.bmp");
	sprintf(m_imageFiles[WorldObject::TypeCarrier], "graphics/carrier.bmp");
	sprintf(m_imageFiles[WorldObject::TypeBattleShip], "graphics/battleship.bmp");
	sprintf(m_imageFiles[WorldObject::TypeFighter], "graphics/fighter.bmp");
	sprintf(m_imageFiles[WorldObject::TypeBomber], "graphics/bomber.bmp");
    sprintf(m_imageFiles[WorldObject::TypeNuke], "graphics/nuke.bmp");
}

void MapRenderer::Render()
{
    START_PROFILE( "MapRenderer" );

    if( m_showFps )
    {
        static double s_lastRender = 0;
        static double s_biggest = 0;
        double timeNow = GetHighResTime();
        double lastFrame = timeNow - s_lastRender;        
        int lastFrameSize = int(lastFrame * 1000) * 0.2f;
        s_lastRender = timeNow;
        if( lastFrame > s_biggest ) s_biggest = lastFrame;
        
        m_frameCountTimer -= g_advanceTime;
        m_currentFrames++;
        if( m_frameCountTimer <= 0.0f )
        {
            m_frameCountTimer = 1.0f;
            m_framesPerSecond = m_currentFrames;
            m_currentFrames = 0;

            //AppDebugOut( "%dms\n", int(s_biggest * 1000) );
            s_biggest = 0;
        }
        char fps[64];
        sprintf( fps, LANGUAGEPHRASE("dialog_mapr_fps") );
		LPREPLACEINTEGERFLAG( 'F', m_framesPerSecond, fps );
        g_renderer->TextSimple( 150, 5, White, 12.0f, fps );
    }


    //
    // Render it!

    float mapColourFader = 1.0f;
    Team *myTeam = g_app->GetWorld()->GetMyTeam();
    if( myTeam ) mapColourFader = myTeam->m_teamColourFader;

    float popupScale = g_preferences->GetFloat(PREFS_INTERFACE_POPUPSCALE);
    m_drawScale = g_app->GetMapRenderer()->m_zoomFactor / (1.0f-popupScale*0.1f);

    float resScale = g_windowManager->WindowH() / 800.0f;
    m_drawScale /= resScale;

#ifdef _DEBUG
    if( myTeam && myTeam->m_type == Team::TypeAI )
    {
        g_renderer->Text( 10, 10, White, 13, "Visible = %d", myTeam->m_targetsVisible );
        g_renderer->Text( 10, 30, White, 13, "MaxVisible = %d", myTeam->m_maxTargetsSeen );

        char *state = "";
        switch( myTeam->m_currentState )
        {
            case Team::StatePlacement:      state = "Placement";    break;
            case Team::StateScouting:       state = "Scouting";     break;
            case Team::StateAssault:        state = "Assault";      break;
            case Team::StateStrike:         state = "Strike";       break;
            case Team::StatePanic:          state = "Panic";        break;
            case Team::StateFinal:          state = "Final";        break;
        }

        g_renderer->TextSimple( 10, 50, White, 13, state );
    }
#endif

    float left, right, top, bottom;
    GetWindowBounds( &left, &right, &top, &bottom );

    //static Image *s_nodeVisImage = NULL;
    //if( !s_nodeVisImage )
    //{
    //    Bitmap nodeVis( 512, 285 );
    //    nodeVis.Clear( Colour(0,0,0,255) );

    //    for( int x = 0; x < 512; x++ )
    //    {
    //        for( int y = 0; y < 285; ++y )
    //        {
    //            Fixed longitude = (x / Fixed::FromDouble(512.0f)) * 360 - 180;
    //            Fixed latitude = (y / Fixed::FromDouble(285.0)) * 200 - 100;

    //            if( IsValidTerritory( -1, longitude, latitude, true ) &&
    //                g_app->GetWorld()->GetClosestNode(longitude, latitude) == -1 )
    //            {
    //                nodeVis.PutPixel( x, y, White );                 
    //            }
    //        }
    //    }

    //    s_nodeVisImage = new Image(&nodeVis);
    //    s_nodeVisImage->MakeTexture(true,false);
    //}


    // Render the landscape first to avoid object clipping at map edges
    for( int x = 0; x < 2; ++x )
    {
        g_renderer->Set2DViewport( left, right, bottom, top, m_pixelX, m_pixelY, m_pixelW, m_pixelH );
        Colour blurColour = g_styleTable->GetPrimaryColour( STYLE_WORLD_LAND );
        Colour desaturatedColour = g_styleTable->GetSecondaryColour( STYLE_WORLD_LAND );
        blurColour = blurColour * mapColourFader + desaturatedColour * (1-mapColourFader);

        g_renderer->SetBlendMode( Renderer::BlendModeAdditive );
        g_renderer->Blit( bmpBlur, -180, 100, 360, -200, blurColour );
        //g_renderer->Blit( g_resource->GetImage("earth/sailable.bmp"), -180, 100, 360, -200, Colour(255,255,255,50) );
        //g_renderer->Blit( s_nodeVisImage, -180, 100, 360, -200, Colour(255,0,0,255) );
        g_renderer->Line( -540, 100, 1080, 100, blurColour );
        g_renderer->Line( -540, -100, 1080, -100, blurColour );
           
        g_renderer->SetBlendMode( Renderer::BlendModeNormal );

        RenderCoastlines();
        RenderCountryControl();

        bool showBorders    = g_preferences->GetInt( PREFS_GRAPHICS_BORDERS );

        if( showBorders ) RenderBorders();
		
        left += GetLongitudeMod();
        right += GetLongitudeMod();
    }

    // Now go through and render objects on top of the landscape.
    GetWindowBounds( &left, &right, &top, &bottom );
    for( int x = 0; x < 2; ++x )
    {
        g_renderer->Set2DViewport( left, right, bottom, top, m_pixelX, m_pixelY, m_pixelW, m_pixelH );

        if( m_showPopulation )          RenderPopulationDensity();
        if( m_showNukeUnits )           RenderNukeUnits();
        
        RenderObjects();
        RenderGunfire();    
        RenderCities(); 
        RenderBlips();        
        RenderWorldMessages();

        if( m_showRadar )               RenderRadar();
        RenderNodes();

        g_renderer->SetBlendMode( Renderer::BlendModeAdditive );

        RenderExplosions();
        RenderAnimations();
        
        bool showRadiation  = g_preferences->GetInt( PREFS_GRAPHICS_RADIATION );
        if( showRadiation )
        {
            Image *boom = g_resource->GetImage( "graphics/population.bmp" );
            for( int i = 0; i < g_app->GetWorld()->m_radiation.Size(); ++i )
            {
                Colour col = Colour(40+sinf(g_gameTime+i*.14)*30,
                                    100+sinf(g_gameTime+i*1.2)*30,
                                    40+cosf(g_gameTime+i*1.5)*30,
                                    20+sinf(g_gameTime+i*1.1)*5);
                Vector3<Fixed> *pos = (Vector3<Fixed> *)g_app->GetWorld()->m_radiation[i];
                float angle = (g_gameTime+i) * 0.01f;
                angle = 0;
                g_renderer->Blit( boom, pos->x.DoubleValue(), pos->y.DoubleValue(), 15.0f, 15.0f, col, angle );
            }
        }

        if( m_highlightUnit != -1 )
        {
            RenderUnitHighlight( m_highlightUnit );
        }

        RenderWhiteBoard();

        left += GetLongitudeMod();
        right += GetLongitudeMod();
    }    

    // render mouse seperately to prevent object info boxes being covered by territory areas
    GetWindowBounds( &left, &right, &top, &bottom );
    for( int x = 0; x < 2; ++x )
    {
        g_renderer->Set2DViewport( left, right, bottom, top, m_pixelX, m_pixelY, m_pixelW, m_pixelH );
        if( IsMouseInMapRenderer() )
        {
            RenderMouse();
        }
        else
        {
            EclWindow *window = EclGetWindow();
            if( stricmp( window->m_name, "Placement" ) == 0 )
            {
                RenderPlacementDetails();
            }
        }
        left += GetLongitudeMod();
        right += GetLongitudeMod();
    }

    END_PROFILE( "MapRenderer" );
}


void MapRenderer::RenderPlacementDetails()
{
    float mouseX;
    float mouseY;    
    ConvertPixelsToAngle( g_inputManager->m_mouseX, g_inputManager->m_mouseY, &mouseX, &mouseY );

    int tooltipsEnabled = g_preferences->GetInt( PREFS_INTERFACE_TOOLTIPS );
    if( tooltipsEnabled &&
        m_tooltip &&
        m_tooltip->Size() )
    {
        float titleSize, textSize, gapSize;
        GetStatePositionSizes( &titleSize, &textSize, &gapSize );

        float boxH = (textSize+gapSize);
        float boxW = 18.0f * m_drawScale;
        float boxX = mouseX - (boxW/3);
        float boxY = mouseY - titleSize - titleSize - titleSize*0.5f;        
        float boxSep = gapSize;


        //
        // Fill box

        Colour windowColPrimary   = g_styleTable->GetPrimaryColour( STYLE_POPUP_BACKGROUND );
        Colour windowColSecondary = g_styleTable->GetSecondaryColour( STYLE_POPUP_BACKGROUND );
        Colour windowBorder       = g_styleTable->GetPrimaryColour( STYLE_POPUP_BORDER );    
        Colour fontCol            = g_styleTable->GetPrimaryColour( FONTSTYLE_POPUP_TITLE );
        bool windowOrientation    = g_styleTable->GetStyle( STYLE_POPUP_BACKGROUND )->m_horizontal;

        float alpha = (m_mouseIdleTime-1.0f);
        Clamp( alpha, 0.0f, 1.0f );

        windowColPrimary.m_a    *= alpha;
        windowColSecondary.m_a  *= alpha;
        windowBorder.m_a        *= alpha;
        fontCol.m_a             *= alpha;

        boxY += boxH;
        float totalBoxH = 0.0f;
        totalBoxH += m_tooltip->Size() * -boxH * 0.6f;


        g_renderer->SetBlendMode( Renderer::BlendModeNormal );
        g_renderer->RectFill( boxX, boxY, boxW, totalBoxH, windowColPrimary, windowColSecondary, windowOrientation );
        g_renderer->Rect( boxX, boxY, boxW, totalBoxH, windowBorder );


        g_renderer->SetFont( "kremlin", true );

        float tooltipY = boxY - 0.5f * m_drawScale;
        RenderTooltip( &boxX, &tooltipY, &boxW, &boxH, &boxSep, alpha );

		g_renderer->SetFont();

        m_tooltip->Empty();
    }    
}


void MapRenderer::RenderExplosions()
{
    START_PROFILE( "Explosions" );
    int myTeamId = g_app->GetWorld()->m_myTeamId;
    
    for( int i = 0; i < g_app->GetWorld()->m_explosions.Size(); ++i )
    {
        if( g_app->GetWorld()->m_explosions.ValidIndex(i) )
        {
            Explosion *explosion = g_app->GetWorld()->m_explosions[i];
            if( IsOnScreen( explosion->m_longitude.DoubleValue(), explosion->m_latitude.DoubleValue() ) )
            {
                if( myTeamId == -1 ||
                    explosion->m_visible[myTeamId] || 
                    m_renderEverything )
                {
                    g_app->GetWorld()->m_explosions[i]->Render();
                }
            }
        }
    }
    END_PROFILE( "Explosions" );
}


void MapRenderer::RenderAnimations()
{
    for( int i = 0; i < m_animations.Size(); ++i )
    {
        if( m_animations.ValidIndex(i) )
        {
            AnimatedIcon *anim = m_animations[i];
            if( anim->Render() )
            {
                m_animations.RemoveData(i);
                delete anim;
            }
        }
    }
}


void MapRenderer::RenderCountryControl()
{
    START_PROFILE( "Country Control" );
    
    g_renderer->SetBlendMode( Renderer::BlendModeAdditive );

    float worldHeight = 200;
    float topY = worldHeight / 2.0f;

    //
    // Render Water

    if( g_preferences->GetInt( PREFS_GRAPHICS_WATER ) > 0 )
    {
        float mapColourFader = 1.0f;
        Team *myTeam = g_app->GetWorld()->GetMyTeam();
        if( myTeam ) mapColourFader = myTeam->m_teamColourFader;

        Colour waterColour = g_styleTable->GetPrimaryColour( STYLE_WORLD_WATER );
        Colour desaturatedColour = g_styleTable->GetSecondaryColour( STYLE_WORLD_WATER );
        waterColour = waterColour * mapColourFader + desaturatedColour * (1-mapColourFader);

        bmpWater = g_resource->GetImage( "graphics/water.bmp" );
        if( g_preferences->GetInt( PREFS_GRAPHICS_WATER ) == 2 &&
            g_preferences->GetInt( PREFS_SCREEN_COLOUR_DEPTH ) > 16 )
        {
            bmpWater = g_resource->GetImage( "graphics/water_shaded.bmp" );
        }

        g_renderer->Blit( bmpWater, -180, topY, 360, -worldHeight, waterColour );
    }


    //
    // Render each teams country

    for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
    {
        Team *team = g_app->GetWorld()->m_teams[i];
        bool showTeam = m_showTeam[team->m_teamId] && g_app->GetWorld()->GetDefcon() > 3;

        if( showTeam || m_showAllTeams )
        {
            Colour col = team->GetTeamColour();
            col.m_a = 120;
            for( int j = 0; j < team->m_territories.Size(); ++j )
            {
                Image *img = m_territories[team->m_territories[j]];
                g_renderer->SetBlendMode( Renderer::BlendModeAdditive );
                g_renderer->Blit( img, -180, topY, 360, -worldHeight, col );

                if( m_showAllTeams )
                {
                    Vector3<float> populationCentre;
					populationCentre = g_app->GetWorld()->m_populationCenter[team->m_territories[j]];

                    char teamName[256];
                    sprintf( teamName, "%s", team->m_name );
                    strupr(teamName);

                    g_renderer->SetFont( "kremlin", true );
                    g_renderer->TextCentreSimple( populationCentre.x, populationCentre.y, White, 7, teamName );
                    g_renderer->SetFont();
                }              
            }


            //
            // Render all our objects to the depth buffer first
            
            if( g_app->GetWorld()->GetDefcon() > 3 &&
                team->m_teamId == g_app->GetWorld()->m_myTeamId )
            {
                g_renderer->SetDepthBuffer( true, true );
                g_renderer->SetBlendMode( Renderer::BlendModeNormal );
                float maxDistance = 5.0f / g_app->GetWorld()->GetGameScale().DoubleValue();

                for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
                {
                    if( g_app->GetWorld()->m_objects.ValidIndex(i) )
                    {
                        WorldObject *wobj = g_app->GetWorld()->m_objects[i];
                        if( wobj && 
                            wobj->m_teamId == team->m_teamId &&
                            !wobj->IsMovingObject() )
                        {
                            g_renderer->CircleFill( wobj->m_longitude.DoubleValue(), wobj->m_latitude.DoubleValue(),
													maxDistance, 40, Colour(0,0,0,30) );
                        }
                    }
                }
                g_renderer->SetDepthBuffer( false, false );
            }
        }
    }

    END_PROFILE( "Country Control" );
}

void MapRenderer::RenderGunfire()
{
    START_PROFILE( "Gunfire" );

    int myTeamId = g_app->GetWorld()->m_myTeamId;

    for( int i = 0; i < g_app->GetWorld()->m_gunfire.Size(); ++i )
    {
        if( g_app->GetWorld()->m_gunfire.ValidIndex(i) )
        {
            GunFire *gunFire = g_app->GetWorld()->m_gunfire[i];
            if( IsOnScreen( gunFire->m_longitude.DoubleValue(), gunFire->m_latitude.DoubleValue() ) &&
                (gunFire->m_teamId == myTeamId ||
                 myTeamId == -1 ||
                 gunFire->m_visible[myTeamId] ||
                 m_renderEverything ) )
            {
                g_app->GetWorld()->m_gunfire[i]->Render();
            }
        }
    }

    END_PROFILE( "Gunfire" );
}

void MapRenderer::RenderWorldMessages()
{
    g_renderer->SetFont( "kremlin", true );

    for( int i = 0; i < g_app->GetWorld()->m_messages.Size(); ++i )
    {
        WorldMessage *msg = g_app->GetWorld()->m_messages[i];
        Colour col(100,100,100,155);
        if( g_app->GetWorld()->GetTeam( msg->m_teamId ) )
        {
            col = g_app->GetWorld()->GetTeam( msg->m_teamId )->GetTeamColour();
        }

        float predictedStateTimer = msg->m_timer.DoubleValue() - g_predictionTime;
        float fractionDone = (3.0f - predictedStateTimer) / 0.5f;
        char caption[512];
        strcpy( caption, msg->m_message );
        if( fractionDone < 1.0f && !msg->m_renderFull )
        {
            int clipIndex = int( strlen(caption) * fractionDone - 1);
            if( clipIndex < 0 ) clipIndex = 0;
            caption[ clipIndex ] = '\x0';
        }

        float longitude = msg->m_longitude.DoubleValue();
        float latitude = msg->m_latitude.DoubleValue();    
        float size = 3.0f;           
        float textWidth = g_renderer->TextWidth( msg->m_message, size );
       
        g_renderer->TextSimple( longitude - (textWidth/2), latitude+3.0f, White, size, caption );       
        
        if( msg->m_messageType == WorldMessage::TypeLaunch )
        {
            g_renderer->Circle( longitude, latitude, size, 30, col, 2.0f );
        }
    }

    g_renderer->SetFont();
}

void MapRenderer::RenderWorldObjectDetails( WorldObject *wobj )
{
    float titleSize, textSize, gapSize;
    GetStatePositionSizes( &titleSize, &textSize, &gapSize );

    //
    // Draw basic box with our allegiance and object type
    
    int numStates = wobj->m_states.Size();

    ////
    //// Render the title bar

    float boxX, boxY, boxW, boxH;
    GetWorldObjectStatePosition( wobj, -2, &boxX, &boxY, &boxW, &boxH );


    char *titleFont = g_styleTable->GetStyle( FONTSTYLE_POPUP_TITLE )->m_fontName;
    
    g_renderer->SetFont( titleFont, true );

    boxY -= boxH * 1.0f;


    //
    // Render more details
        
    if( wobj->m_type == WorldObject::TypeCity )
    {
        RenderCityObjectDetails( wobj, &boxX, &boxY, &boxW, &boxH, &gapSize );
    }
    else if( wobj->m_teamId == g_app->GetWorld()->m_myTeamId && 
             numStates > 0 )
    {
        if( m_stateRenderTime <= 0.0f )
        {
            RenderFriendlyObjectDetails( wobj, &boxX, &boxY, &boxW, &boxH, &gapSize );
        }
        else
        {
            RenderFriendlyObjectMenu( wobj, &boxX, &boxY, &boxW, &boxH, &gapSize );
        }
    }
    else if( wobj->m_teamId != g_app->GetWorld()->m_myTeamId )
    {
        RenderEnemyObjectDetails( wobj, &boxX, &boxY, &boxW, &boxH, &gapSize );
    }

    g_renderer->SetFont();
}


void MapRenderer::RenderCityObjectDetails( WorldObject *wobj, float *boxX, float *boxY, float *boxW, float *boxH, float *boxSep )
{
    City *city = (City *) wobj;

	float originalAlpha = g_renderer->m_alpha * (1.0f - ( m_highlightTime / 1.0f ));
    int alpha = originalAlpha * 255;

    float titleSize, textSize, gapSize;
    GetStatePositionSizes( &titleSize, &textSize, &gapSize );
	
    float totalBoxH = 1.6 * -(*boxH);
    if( city->m_dead > 0 )
    {
        totalBoxH += 0.4f * -(*boxH);
    }

    *boxW -= 5 * m_drawScale;

    int tooltipsEnabled = g_preferences->GetInt( PREFS_INTERFACE_TOOLTIPS );
    if( tooltipsEnabled && m_tooltip )
    {
        totalBoxH += m_tooltip->Size() * -(*boxH) * 0.6f;
    }

    Colour windowColPrimary   = g_styleTable->GetPrimaryColour( STYLE_POPUP_BACKGROUND );
    Colour windowColSecondary = g_styleTable->GetSecondaryColour( STYLE_POPUP_BACKGROUND );
    Colour windowBorder       = g_styleTable->GetPrimaryColour( STYLE_POPUP_BORDER );    
    Colour fontCol            = g_styleTable->GetPrimaryColour( FONTSTYLE_POPUP );

    bool windowOrientation    = g_styleTable->GetStyle( STYLE_POPUP_BACKGROUND )->m_horizontal;

    windowColPrimary    *= originalAlpha;
    windowColSecondary  *= originalAlpha;
    windowBorder        *= originalAlpha;
    fontCol             *= originalAlpha;

    *boxY += *boxH;

    g_renderer->RectFill( *boxX, *boxY, *boxW, totalBoxH, windowColPrimary, windowColSecondary, windowOrientation );
    g_renderer->Rect( *boxX, *boxY, *boxW, totalBoxH, windowBorder );
    float timeSize = 1.5f * m_drawScale;


    //
    // Render title

    *boxY -= *boxH;

    char *titleFont = g_styleTable->GetStyle( FONTSTYLE_POPUP )->m_fontName;
    g_renderer->SetFont( titleFont, true );

    char *objName = WorldObject::GetName( wobj->m_type );
    Team *team = g_app->GetWorld()->GetTeam(wobj->m_teamId);
    const char *teamName = team ? team->GetTeamName() : "";

    char caption[256];
    sprintf( caption, "%s  %s", teamName, objName );
    strupr( caption );

    g_renderer->Text( *boxX + gapSize, 
        *boxY + (*boxH-textSize)/2.0f, 
        fontCol, 
        textSize, 
        caption );

    
    //
    // 1. Population

    *boxY -= *boxH * 0.5f;

    float textYPos = *boxY + (*boxH - textSize)/2;

	char number[32];
	sprintf( caption, LANGUAGEPHRASE("dialog_mapr_city_population") );
	sprintf( number, "%2.1f", city->m_population / 1000000.0f );
	LPREPLACESTRINGFLAG( 'P', number, caption );
    g_renderer->TextSimple( *boxX + *boxSep, textYPos, fontCol, textSize*0.6f, caption );

    *boxY -= *boxH * 0.5f;

    //
    // 2. Num Dead

    if( city->m_dead > 0 )
    {
        float textYPos = *boxY + (*boxH - textSize)/2;

		char caption[128];
		char number[32];
		sprintf( caption, LANGUAGEPHRASE("dialog_mapr_city_dead") );
		sprintf( number, "%2.1f", city->m_dead / 1000000.0f );
		LPREPLACESTRINGFLAG( 'D', number, caption );
        g_renderer->TextSimple( *boxX + *boxSep, textYPos, fontCol, textSize*0.6f, caption );

        *boxY -= *boxH * 0.4f;
    }
    

    //
    // 3. Tooltips

    if( tooltipsEnabled && m_tooltip )
    {
        *boxY += *boxH * 0.4f;
        
        RenderTooltip( boxX, boxY, boxW, boxH, boxSep, originalAlpha );
    }

	g_renderer->SetFont();
}


void MapRenderer::RenderTooltip( float *boxX, float *boxY, float *boxW, float *boxH, float *boxSep, float alpha )
{
    if( m_tooltip )
    {
        float titleSize, textSize, gapSize;
        GetStatePositionSizes( &titleSize, &textSize, &gapSize );

        Colour fontCol = g_styleTable->GetPrimaryColour( FONTSTYLE_POPUP );
        fontCol.m_a *= alpha;


        for( int i = 0; i < m_tooltip->Size(); ++i )
        {
            char *thisLine = m_tooltip->GetData(i);
            *boxY -= *boxH * 0.4f;
            if( i > 0 ) *boxY -= *boxH * 0.2f;

			char *phraseLMB = LANGUAGEPHRASE("tooltip_lmb");
			char *phraseRMB = LANGUAGEPHRASE("tooltip_rmb");

            Image *img = NULL;
			if( phraseLMB && strncmp( thisLine, phraseLMB, strlen( phraseLMB ) ) == 0 )
			{
				img = g_resource->GetImage( "gui/lmb.bmp" );
			}
			else if( phraseRMB && strncmp( thisLine, phraseRMB, strlen( phraseRMB ) ) == 0 )
			{
				img = g_resource->GetImage( "gui/rmb.bmp" );
			}

            if( img )
            {
                float iconSize = textSize * 0.9f;
                float iconY = *boxY + iconSize - 0.3f * m_drawScale ;

                g_renderer->SetBlendMode( Renderer::BlendModeAdditive );
                g_renderer->Blit( img, *boxX + *boxSep, iconY, iconSize, -iconSize, Colour(255,255,255,255*alpha) );
                thisLine += 5;

                g_renderer->TextSimple( *boxX + *boxSep + 2 * m_drawScale, *boxY, fontCol, textSize*0.6f, thisLine );
            }
            else
            {
                g_renderer->TextSimple( *boxX + *boxSep, *boxY, fontCol, textSize*0.6f, thisLine );
            }
        }
    }
}


void MapRenderer::RenderFriendlyObjectDetails( WorldObject *wobj, float *boxX, float *boxY, float *boxW, float *boxH, float *boxSep )
{
    float originalAlpha = g_renderer->m_alpha * (1.0f - ( m_highlightTime / 1.0f ));
    originalAlpha = min( originalAlpha, 0.8f );
    int alpha = originalAlpha * 255;

    float titleSize, textSize, gapSize;
    GetStatePositionSizes( &titleSize, &textSize, &gapSize );

    *boxY += *boxH;
    float inset = 0.3f * m_drawScale;

    Colour windowColPrimary   = g_styleTable->GetPrimaryColour( STYLE_POPUP_BACKGROUND );
    Colour windowColSecondary = g_styleTable->GetSecondaryColour( STYLE_POPUP_BACKGROUND );
    Colour windowBorder       = g_styleTable->GetPrimaryColour( STYLE_POPUP_BORDER );    
    Colour fontCol            = g_styleTable->GetPrimaryColour( FONTSTYLE_POPUP );

    bool windowOrientation    = g_styleTable->GetStyle( STYLE_POPUP_BACKGROUND )->m_horizontal;

    if( wobj->m_stateTimer <= Fixed::Hundredths(10) )
    {
        *boxW -= 7 * m_drawScale;
    }

    windowColPrimary.m_a    *= originalAlpha;
    windowColSecondary.m_a  *= originalAlpha;
    windowBorder.m_a        *= originalAlpha;
    fontCol.m_a             *= originalAlpha;

    //
    // Spawn object counters

    int numFighters = -1;
    int numBombers = -1;
    int numNukes = -1;

    switch( wobj->m_type )
    {
        case WorldObject::TypeSilo:
            //numNukes = wobj->m_states[0]->m_numTimesPermitted;
            break;

        case WorldObject::TypeSub:
            //numNukes = wobj->m_states[2]->m_numTimesPermitted;
            break;

        case WorldObject::TypeBomber:
            //numNukes = wobj->m_states[1]->m_numTimesPermitted;
            break;

        case WorldObject::TypeAirBase:
        case WorldObject::TypeCarrier:
            numFighters = wobj->m_states[0]->m_numTimesPermitted;
            numBombers = wobj->m_states[1]->m_numTimesPermitted;
            numNukes = wobj->m_nukeSupply;
            break;
    }


    float totalBoxH = 1 * -(*boxH);
    if( numFighters != -1 || numBombers != -1 || numNukes != -1 )
    {
        totalBoxH += 0.9f * -(*boxH);
    }

    int tooltipsEnabled = g_preferences->GetInt( PREFS_INTERFACE_TOOLTIPS );
    if( tooltipsEnabled && m_tooltip )
    {
        totalBoxH += m_tooltip->Size() * -(*boxH) * 0.6f;
    }

    //
    // Render main box

    g_renderer->RectFill( *boxX, *boxY, *boxW, totalBoxH, windowColPrimary, windowColSecondary, windowOrientation );
    g_renderer->Rect( *boxX, *boxY, *boxW, totalBoxH, windowBorder );


    *boxY -= *boxH;

    //
    // Render current state details

    WorldObjectState *state = wobj->m_states[wobj->m_currentState];

    char stateName[256];
    sprintf( stateName, LANGUAGEPHRASE("dialog_mapr_mode") );
	LPREPLACESTRINGFLAG( 'M', state->GetStateName(), stateName );

    float textYPos = *boxY + (*boxH - textSize)/2;
    g_renderer->TextSimple( *boxX + *boxSep, textYPos, fontCol, textSize, stateName );
        
    if( wobj->m_stateTimer > 0 && 
        state->m_numTimesPermitted != 0 )
    {
        char stimeTodo[64];
		sprintf( stimeTodo, LANGUAGEPHRASE("dialog_mapr_secs") );
		LPREPLACEINTEGERFLAG( 'S', wobj->m_stateTimer.IntValue(), stimeTodo );

        g_renderer->TextRight( *boxX + *boxSep + *boxW - *boxSep - (1.0f * m_drawScale), 
                               textYPos, fontCol, textSize, stimeTodo );
    }


    //
    // tooltip

    if( tooltipsEnabled )
    {
        RenderTooltip( boxX, boxY, boxW, boxH, boxSep, originalAlpha );
    }


    //
    // Spawn object counters

    *boxY -= *boxH;
    float xPos = *boxX;
    float yPos = *boxY + 1.4f * m_drawScale;
    float objSize = 1.0f * m_drawScale;
    float gap = objSize * 2.0f;

    Colour col(255,255,255,alpha);

    float totalSize = 0;
    if( numFighters != -1 ) totalSize += gap * numFighters + gap * 0.5f;
    if( numBombers != -1 ) totalSize += gap * numBombers + gap * 0.5f;
    if( numNukes != -1 ) totalSize += gap * numNukes * 0.5f + gap * 0.25f;

    float maxSize = *boxW;
    if( totalSize > maxSize )
    {
        float fractionMore = totalSize / maxSize;
        gap /= fractionMore;
    }


    if( numFighters == 0 && numBombers == 0 && numNukes == 0 )
    {
        col.m_a = 150;
        g_renderer->TextSimple( xPos + gap/2, yPos - gap/2, col, textSize, LANGUAGEPHRASE("dialog_mapr_empty") );
    }
    else
    {
        g_renderer->SetBlendMode( Renderer::BlendModeAdditive );

        //
        // Fighters

        if( numFighters != -1 )
        {
            Image *img = g_resource->GetImage( "graphics/fighter.bmp" );
            for( int i = 0; i < numFighters; ++i )
            {
                g_renderer->Blit( img, xPos+=gap, yPos, objSize, objSize, col, 0 );
            }
            
            if( numFighters > 0 ) xPos += gap*0.5f;
        }


        //
        // Bombers
        
        if( numBombers != -1 )
        {
            Image *img = g_resource->GetImage( "graphics/bomber.bmp" );
            Image *nuke = g_resource->GetImage( "graphics/nuke.bmp" );
            for( int i = 0; i < numBombers; ++i )
            {
                g_renderer->Blit( img, xPos+=gap, yPos, objSize*1.2f, objSize*1.2f, col, 0 );
                if( (i+1) <= numNukes )
                {
                    g_renderer->Blit( nuke, xPos, yPos, objSize*0.65f, objSize*0.65f, col, 0 );
                }
            }
            if( numBombers > 0 ) xPos += gap*0.5f;
        }


        //
        // Nukes

        if( numNukes != -1 )
        {
            if( numBombers != -1 ) numNukes -= numBombers;
            Image *img = g_resource->GetImage( "graphics/nuke.bmp" );
            for( int i = 0; i < numNukes; ++i )
            {
                g_renderer->Blit( img, xPos+=gap*0.5f, yPos, objSize, objSize, col, 0 );
            }
        }
    }
}


void MapRenderer::RenderFriendlyObjectMenu( WorldObject *wobj, float *boxX, float *boxY, float *boxW, float *boxH, float *boxSep )
{
#ifdef NON_PLAYABLE
    RenderEnemyObjectDetails( wobj, boxX, boxY, boxW, boxH, boxSep );
    return;
#endif

	int numStates = wobj->m_states.Size();
    float originalAlpha = g_renderer->m_alpha * (1.0f - ( m_highlightTime / 1.0f ));
    int alpha = originalAlpha * 255;

    if( m_stateRenderTime <= 0.0f )
    {
        alpha *= 0.5f;
    }

    float titleSize, textSize, gapSize;
    GetStatePositionSizes( &titleSize, &textSize, &gapSize );

    //*boxW += 7 * m_drawScale;

	*boxY += *boxH;
    float totalBoxH = numStates * -(*boxH);

    float inset = 0.3f * m_drawScale;

    bool actionQueue = wobj->IsActionQueueable() &&
                       wobj->m_actionQueue.Size() > 0;

    if( actionQueue )
    {
        totalBoxH -= *boxH;
    }

    Colour windowColPrimary   = g_styleTable->GetPrimaryColour( STYLE_POPUP_BACKGROUND );
    Colour windowColSecondary = g_styleTable->GetSecondaryColour( STYLE_POPUP_BACKGROUND );
    Colour windowBorder       = g_styleTable->GetPrimaryColour( STYLE_POPUP_BORDER );    
    Colour highlightPrimary   = g_styleTable->GetPrimaryColour( STYLE_POPUP_HIGHLIGHT );
    Colour highlightSecondary = g_styleTable->GetSecondaryColour( STYLE_POPUP_HIGHLIGHT );
    Colour selectedPrimary    = g_styleTable->GetPrimaryColour( STYLE_POPUP_SELECTION );
    Colour selectedSecondary  = g_styleTable->GetSecondaryColour( STYLE_POPUP_SELECTION );
    Colour fontCol            = g_styleTable->GetPrimaryColour( FONTSTYLE_POPUP );

    bool windowOrientation    = g_styleTable->GetStyle( STYLE_POPUP_BACKGROUND )->m_horizontal;
    bool highlightOrientation = g_styleTable->GetStyle( STYLE_POPUP_HIGHLIGHT )->m_horizontal;
    bool selectOrientation    = g_styleTable->GetStyle( STYLE_POPUP_BACKGROUND )->m_horizontal;

    windowColPrimary.m_a    *= originalAlpha;
    windowColSecondary.m_a  *= originalAlpha;
    windowBorder.m_a        *= originalAlpha;
    highlightPrimary.m_a    *= originalAlpha;
    highlightSecondary.m_a  *= originalAlpha;
    selectedPrimary.m_a     *= originalAlpha;
    selectedSecondary.m_a   *= originalAlpha;
    fontCol.m_a             *= originalAlpha;


    //
    // Render main box

    g_renderer->RectFill( *boxX, *boxY, *boxW, totalBoxH, windowColPrimary, windowColSecondary, windowOrientation );
    g_renderer->Rect( *boxX, *boxY, *boxW, totalBoxH, windowBorder );
    

    // Render FUEL CAPACITY
    //
    //if( wobj->m_type == WorldObject::TypeFighter ||
    //    wobj->m_type == WorldObject::TypeBomber )
    //{
    //    MovingObject *mobj = (MovingObject *) wobj;
    //    
    //    float fuelX = *boxX + *boxW - textSize;
    //    float fuelSize = textSize * 0.8f;
    //    
    //    g_renderer->TextRight( fuelX, 
    //                           *boxY+fuelSize, 
    //                           Colour(255,255,255,alpha), 
    //                           fuelSize, 
    //                           LANGUAGEPHRASE("details_fuel") );

    //    g_renderer->TextRight( fuelX, 
    //                           *boxY, 
    //                           Colour(255,255,255,alpha), 
    //                           fuelSize, 
    //                           "%.2f", mobj->m_range );
    //}

    //
    // Render Clear ActionQueue button

    if( actionQueue )
    {
        float stateX, stateY, stateW, stateH;
        GetWorldObjectStatePosition( wobj, -1, &stateX, &stateY, &stateW, &stateH );

        if( m_currentStateId == CLEARQUEUE_STATEID )
        {
            g_renderer->RectFill( stateX+inset, 
                                  stateY+inset, 
                                  stateW-inset*2, 
                                  stateH-inset*2, 
                                  highlightSecondary, 
                                  highlightPrimary, 
                                  highlightOrientation );
        }

        float textYPos = stateY + (stateH - textSize)/2;

        g_renderer->TextSimple( stateX + *boxSep, textYPos, fontCol, textSize, LANGUAGEPHRASE("dialog_mapr_cancel_all_orders") );
    }

    //
    // Render our state buttons
    for( int i = 0; i < numStates; ++i )
    {
        WorldObjectState *state = wobj->m_states[i];
        float stateX, stateY, stateW, stateH;
        GetWorldObjectStatePosition( wobj, i, &stateX, &stateY, &stateW, &stateH );
            
        char *caption = NULL;
        float timeTodo = 0.0f;

		if( i == m_currentStateId &&
            state->m_numTimesPermitted != 0 &&
            state->m_defconPermitted >= g_app->GetWorld()->GetDefcon())
        {
            g_renderer->RectFill( stateX+inset, 
                                  stateY+inset, 
                                  stateW-inset*2, 
                                  stateH-inset*2, 
                                  highlightSecondary, 
                                  highlightPrimary, 
                                  highlightOrientation );            
        }
        
        if( i == wobj->m_currentState )
        {
            g_renderer->RectFill( stateX+inset, 
                                  stateY+inset, 
                                  stateW-inset*2, 
                                  stateH-inset*2, 
                                  selectedSecondary, 
                                  selectedPrimary, 
                                  selectOrientation );
            g_renderer->Rect( stateX+inset, 
                              stateY+inset, 
                              stateW-inset*2, 
                              stateH-inset*2,
                              windowBorder );
        }
                        
        if( i == wobj->m_currentState && wobj->m_stateTimer <= 0 )
        {
            caption = state->GetStateName();
            timeTodo = 0.0f;
        }
        else if( i == wobj->m_currentState && wobj->m_stateTimer > 0 )
        {
            caption = state->GetPreparingName();
            timeTodo = wobj->m_stateTimer.DoubleValue();
        }
        else
        {
            caption = state->GetPrepareName();
            timeTodo = state->m_timeToPrepare.DoubleValue();
        }    
        
        if( state->m_numTimesPermitted != -1 )
        {
            sprintf(caption, "%s (%u)", caption, state->m_numTimesPermitted );
        }       

        Colour textCol;
        if( state->m_defconPermitted < g_app->GetWorld()->GetDefcon() ||
            state->m_numTimesPermitted == 0 )
        {
            textCol = Colour( 100, 100, 100, alpha*0.5f );
        }
        else
        {
            textCol = fontCol;
        }

        float textYPos = stateY + (stateH - textSize)/2;
        g_renderer->TextSimple( stateX + *boxSep, textYPos, textCol, textSize, caption );

        if( state->m_defconPermitted < g_app->GetWorld()->GetDefcon() )
        {
            char defconAllowed[64];
            sprintf( defconAllowed, LANGUAGEPHRASE("dialog_worldstatus_defcon_x") );
			LPREPLACEINTEGERFLAG( 'D', state->m_defconPermitted, defconAllowed );
            g_renderer->TextRight( stateX + stateW - (1.0f * m_drawScale), 
                                   textYPos, textCol, textSize, defconAllowed );
        }
        else if( timeTodo > 0.0f && state->m_numTimesPermitted != 0 )
        {
            char stimeTodo[64];
            sprintf( stimeTodo, LANGUAGEPHRASE("dialog_mapr_secs") );
			LPREPLACEINTEGERFLAG( 'S', int(timeTodo), stimeTodo );
            g_renderer->TextRight( stateX + stateW - (1.0f * m_drawScale), 
                                   textYPos, textCol, textSize, stimeTodo );
		}

        //
        // Render problems with the current highlighted state 

        if( i == m_currentStateId )
        {
            if( state->m_numTimesPermitted == 0 )
            {
                g_renderer->RectFill( stateX, stateY, stateW, stateH, Colour(0,0,0,100) );
                sprintf( caption, LANGUAGEPHRASE("dialog_mapr_empty") );
                g_renderer->TextCentre( stateX + stateW/2, textYPos, Colour(255,0,0,255), textSize, caption );
            }
            else if( state->m_defconPermitted < g_app->GetWorld()->GetDefcon())
            {                
                g_renderer->RectFill( stateX, stateY, stateW, stateH, Colour(0,0,0,100) );
                sprintf( caption, LANGUAGEPHRASE("dialog_mapr_not_before_defcon_x") );
				LPREPLACEINTEGERFLAG( 'D', state->m_defconPermitted, caption );
                g_renderer->TextCentre( stateX + stateW/2, textYPos, Colour(255,0,0,255), textSize, caption );
            }
        }
    }
}

void MapRenderer::GetWorldObjectStatePosition( WorldObject *wobj, int state, float *screenX, float *screenY, float *screenW, float *screenH )
{
    float titleSize, textSize, gapSize;
    GetStatePositionSizes( &titleSize, &textSize, &gapSize );
 
    float predictedLongitude = ( wobj->m_longitude + wobj->m_vel.x * Fixed::FromDouble(g_predictionTime)
													 * g_app->GetWorld()->GetTimeScaleFactor() ).DoubleValue();
    float predictedLatitude  = ( wobj->m_latitude + wobj->m_vel.y * Fixed::FromDouble(g_predictionTime)
													* g_app->GetWorld()->GetTimeScaleFactor() ).DoubleValue();

    *screenH = (textSize+gapSize);
    *screenW = 27.0f;
    *screenX = predictedLongitude - (*screenW/4 * m_drawScale);

    if( wobj->m_teamId == g_app->GetWorld()->m_myTeamId &&
        wobj->m_type != WorldObject::TypeCity )
    {
        *screenW = 34.0f;
    }

    if( state == -1 )
    {
        // Clear Queue state
        int numStates = wobj->m_states.Size();
        *screenY = predictedLatitude - titleSize - (numStates+1) * (*screenH);
    }
    else if( state == -2 )
    {
        // Title bar state
        *screenY = predictedLatitude - titleSize;
        *screenH = titleSize;
    }
    else
    {
        // Ordinary state
        *screenY = predictedLatitude - titleSize - ( state +1 ) * (*screenH);
    }

    *screenW = *screenW * m_drawScale;
}


void MapRenderer::GetStatePositionSizes( float *titleSize, float *textSize, float *gapSize )
{
    *titleSize = 3.0f * m_drawScale;
    *textSize = 1.8f * m_drawScale;
    *gapSize = 1.0f * m_drawScale;
}


void MapRenderer::RenderEnemyObjectDetails( WorldObject *wobj, float *boxX, float *boxY, float *boxW, float *boxH, float *boxSep )
{
	// This is an ENEMY vessel
 
	if( wobj->m_teamId != TEAMID_SPECIALOBJECTS )
	{
        float titleSize, textSize, gapSize;
        GetStatePositionSizes( &titleSize, &textSize, &gapSize );

        float originalAlpha = g_renderer->m_alpha * (1.0f - ( m_highlightTime / 1.0f ));
        int alpha = originalAlpha * 255;

        Colour windowColPrimary   = g_styleTable->GetPrimaryColour( STYLE_POPUP_BACKGROUND );
        Colour windowColSecondary = g_styleTable->GetSecondaryColour( STYLE_POPUP_BACKGROUND );
        Colour windowBorder       = g_styleTable->GetPrimaryColour( STYLE_POPUP_BORDER );    
        Colour fontCol            = g_styleTable->GetPrimaryColour( FONTSTYLE_POPUP );

        bool windowOrientation    = g_styleTable->GetStyle( STYLE_POPUP_BACKGROUND )->m_horizontal;

        windowColPrimary.m_a    *= originalAlpha;
        windowColSecondary.m_a  *= originalAlpha;
        windowBorder.m_a        *= originalAlpha;
        fontCol.m_a             *= originalAlpha;

        if( m_currentSelectionId == -1 )
        {
            //
            // Render title

            g_renderer->SetBlendMode( Renderer::BlendModeNormal );
            g_renderer->RectFill( *boxX, *boxY, *boxW, *boxH, windowColPrimary, windowColSecondary, windowOrientation );
            g_renderer->Rect    ( *boxX, *boxY, *boxW, *boxH, windowBorder );
            
            char *titleFont = g_styleTable->GetStyle( FONTSTYLE_POPUP )->m_fontName;
            g_renderer->SetFont( titleFont, true );

            char *objName = WorldObject::GetName( wobj->m_type );
            Team *team = g_app->GetWorld()->GetTeam(wobj->m_teamId);
            const char *teamName = team ? team->GetTeamName() : "";

            char caption[256];
            sprintf( caption, "%s  %s", teamName, objName );
            strupr( caption );

            g_renderer->Text( *boxX + gapSize, 
                              *boxY + (*boxH-textSize)/2.0f, 
                              fontCol, 
                              textSize, 
                              caption );
        }

        *boxY += *boxH;

        //
        // If we are targeting it then bring up some statistics

        WorldObject *ourObj = g_app->GetWorld()->GetWorldObject( m_currentSelectionId );
		if( ourObj)
		{
			float totalBoxH = 1 * -(*boxH);

            int tooltipsEnabled = g_preferences->GetInt( PREFS_INTERFACE_TOOLTIPS );
            if( tooltipsEnabled && m_tooltip )
            {
                totalBoxH += m_tooltip->Size() * -(*boxH) * 0.6f;
            }

			g_renderer->RectFill( *boxX, *boxY, *boxW, totalBoxH, windowColPrimary, windowColSecondary, windowOrientation );
			g_renderer->Rect( *boxX, *boxY, *boxW, totalBoxH, windowBorder );
            
			int attackOdds = g_app->GetWorld()->GetAttackOdds( ourObj->m_type, wobj->m_type, ourObj->m_objectId );
				        
			char odds[256];
            Colour col;
            if( attackOdds == 0 ) 
            {
                col = Colour(150,150,150,alpha);
                sprintf( odds, "%s", LANGUAGEPHRASE("odds_zero"));
            }
            else if( attackOdds < 5 ) 
            {
                col = Colour(255,0,0,alpha);
                sprintf( odds, "%s", LANGUAGEPHRASE("odds_vlow") );
            }
            else if( attackOdds < 10 ) 
            {
                col = Colour(255,150,0,alpha );
                sprintf( odds, "%s", LANGUAGEPHRASE("odds_low") );
            }
            else if( attackOdds < 20 ) 
            {
                col = Colour(255,255,0,alpha );
                sprintf( odds, "%s", LANGUAGEPHRASE("odds_med") );
            }
            else if( attackOdds <= 25 )
            {
                col = Colour(150,255,0,alpha);
                sprintf( odds, "%s", LANGUAGEPHRASE("odds_high") );
            }
            else
            {
                col = Colour(0,255,0,alpha);
                sprintf( odds, "%s", LANGUAGEPHRASE("odds_vhigh") );
            }

            strupr(odds);
            
			g_renderer->TextSimple( *boxX + *boxSep, *boxY-textSize-(0.5f * m_drawScale), col, textSize, odds );


            //
            // tooltip

            if( tooltipsEnabled && m_tooltip )
            {
                *boxY -= *boxH;

                RenderTooltip( boxX, boxY, boxW, boxH, boxSep, originalAlpha );
            }
		}
    }

	g_renderer->SetFont();
}

void MapRenderer::RenderMouse()
{
    float longitude;
    float latitude;    
    ConvertPixelsToAngle( g_inputManager->m_mouseX, g_inputManager->m_mouseY, &longitude, &latitude );
    
    // render long/lat on mouse

#ifdef _DEBUG
    if( g_keys[KEY_L] )
    {
        char pos[128];
        sprintf( pos, "long %2.1f lat %2.1f", longitude, latitude );
        g_renderer->SetFont( "zerothre", true );
        g_renderer->TextSimple( longitude, latitude, White, 3.0f, pos );
        g_renderer->SetFont();
    }
#endif

    g_renderer->SetBlendMode( Renderer::BlendModeNormal );


    if( m_draggingCamera )
    {
        Image *move = g_resource->GetImage( "gui/move.bmp" );
        float size = 4.0f * m_drawScale;
        g_renderer->Blit( move, longitude - size/2, latitude - size/2, size, size, White );
    }


    WorldObject *selection = g_app->GetWorld()->GetWorldObject( m_currentSelectionId );
    WorldObject *highlight = g_app->GetWorld()->GetWorldObject( m_currentHighlightId );

    if( selection && selection->m_teamId == TEAMID_SPECIALOBJECTS ) selection = NULL;
    if( highlight && highlight->m_teamId == TEAMID_SPECIALOBJECTS ) highlight = NULL;

    float timeScale = g_app->GetWorld()->GetTimeScaleFactor().DoubleValue();


    //
    // Get new the tooltip ready

    LList<char *> *tooltip = new LList<char *>();


    //
    // Render our current selection
#ifndef NON_PLAYABLE
    if( selection )
    {
        float predictedLongitude = selection->m_longitude.DoubleValue() + selection->m_vel.x.DoubleValue() * g_predictionTime * timeScale;
        float predictedLatitude = selection->m_latitude.DoubleValue() + selection->m_vel.y.DoubleValue() * g_predictionTime  * timeScale;
        float size = 3.6f;
        if( GetZoomFactor() <= 0.25f )
        {
            size *= GetZoomFactor() * 4;
        }

        Team *team = g_app->GetWorld()->GetTeam(selection->m_teamId);
        Colour col(128,128,128);
        if( team ) col = team->GetTeamColour();
        col.m_a = 250;


        //
        // Render action cursor from our selection

        float actionCursorLongitude = longitude;
        float actionCursorLatitude = latitude;
        Colour actionCursorCol( 50,50,50,100 );
        float actionCursorSize = 2.0f;
        float actionCursorAngle = 0;
        Colour spawnUnitCol( 100,100,100,100 );
        float spawnUnitSize = 3.0f;

        if( GetZoomFactor() <=0.25f )
        {
            actionCursorSize *= GetZoomFactor() * 4;
            spawnUnitSize *= GetZoomFactor() * 4;
        }

        int validCombatTarget = selection->IsValidCombatTarget( m_currentHighlightId );
        int validMovementTarget = selection->IsValidMovementTarget( Fixed::FromDouble(longitude), Fixed::FromDouble(latitude) );

        bool fleetValidMovement = true;
        Fleet *fleet = team->GetFleet(selection->m_fleetId);
        if( fleet ) fleetValidMovement = fleet->IsValidFleetPosition( Fixed::FromDouble(longitude), Fixed::FromDouble(latitude) );
        
        if( highlight && validCombatTarget > 0 )
        {
            // Highlighted a valid enemy unit
            actionCursorLongitude = (highlight->m_longitude + highlight->m_vel.x 
															  * Fixed::FromDouble(g_predictionTime)
															  * Fixed::FromDouble(timeScale)).DoubleValue();
            actionCursorLatitude = highlight->m_latitude.DoubleValue() + highlight->m_vel.y.DoubleValue() * g_predictionTime  * timeScale;            
            actionCursorCol.Set( 255, 0, 0, 255 );
            actionCursorSize *= 1.5f;
            actionCursorAngle = g_gameTime * -1;

            if( validCombatTarget == WorldObject::TargetTypeLand )
            {
                actionCursorCol.Set( 0, 0, 255, 255 );                
            }
            else if( validCombatTarget > WorldObject::TargetTypeValid )
            {
                spawnUnitCol.Set( 255, 0, 0, 255 );
            }
        }
        else if( fleet )
        {
            if( fleetValidMovement )
            {
                actionCursorCol.Set( 0, 0, 255, 255 );
                actionCursorSize *= 1.5f;
                actionCursorAngle = g_gameTime * -0.5f;
            }

            if( validMovementTarget > WorldObject::TargetTypeValid )
            {
                spawnUnitCol.Set( 0, 0, 255, 255 );
            }
        }
        else if( validMovementTarget > 0 )
        {
            // Highlighted a valid movement target
            actionCursorCol.Set( 0, 0, 255, 255 );
            actionCursorSize *= 1.5f;
            actionCursorAngle = g_gameTime * -0.5f;

            if( validMovementTarget > WorldObject::TargetTypeValid )
            {
                spawnUnitCol.Set( 0, 0, 255, 255 );
            }

            // check for landing
            if( highlight && 
                ( highlight->m_type == WorldObject::TypeCarrier || highlight->m_type == WorldObject::TypeAirBase ) &&
                ( selection->m_type == WorldObject::TypeFighter || selection->m_type == WorldObject::TypeBomber ) )
            {   
                actionCursorLongitude = highlight->m_longitude.DoubleValue() + highlight->m_vel.x.DoubleValue() * g_predictionTime * timeScale;
                actionCursorLatitude = highlight->m_latitude.DoubleValue() + highlight->m_vel.y.DoubleValue() * g_predictionTime  * timeScale;
            }
        }

        g_renderer->SetBlendMode( Renderer::BlendModeAdditive );

        Image *img = g_resource->GetImage( "graphics/cursor_target.bmp" );
        g_renderer->Blit( img, actionCursorLongitude, actionCursorLatitude, 
                            actionCursorSize, actionCursorSize, 
                            actionCursorCol, actionCursorAngle );

        //
        // Render problem messages with target

        int validTarget = validCombatTarget;
        if( validTarget <= WorldObject::TargetTypeInvalid ) validTarget = validMovementTarget;

        if( validTarget < WorldObject::TargetTypeInvalid )
        {
            g_renderer->SetFont( "kremlin", true );
            switch( validTarget )
            {
                case WorldObject::TargetTypeDefconRequired:
                {
                    int defconRequired = selection->m_states[selection->m_currentState]->m_defconPermitted;
					char caption[128];
					sprintf( caption, LANGUAGEPHRASE("dialog_mapr_defcon_x_required") );
					LPREPLACEINTEGERFLAG( 'D', defconRequired, caption );
                    g_renderer->TextCentreSimple( actionCursorLongitude, actionCursorLatitude+actionCursorSize, White, 2, caption );
                    break;
                }

                case WorldObject::TargetTypeOutOfRange:
                    g_renderer->TextCentreSimple( actionCursorLongitude, actionCursorLatitude+actionCursorSize, White, 2, LANGUAGEPHRASE("dialog_mapr_out_of_range") );
                    break;

                case WorldObject::TargetTypeOutOfStock:
                    g_renderer->TextCentreSimple( actionCursorLongitude, actionCursorLatitude+actionCursorSize, White, 2, LANGUAGEPHRASE("dialog_mapr_empty") );
                    break;
            }
            g_renderer->SetFont();
        }


        //
        // We can possibly spawn a unit to this location
        
        int spawnType = validCombatTarget;
        if( spawnType <= WorldObject::TargetTypeInvalid ) spawnType = validMovementTarget;    
        if( spawnType > WorldObject::TargetTypeValid)
        {
            switch( spawnType )
            {
                case WorldObject::TargetTypeLaunchFighter:      
                    img = g_resource->GetImage( "graphics/fighter.bmp" );       
                    tooltip->PutData( LANGUAGEPHRASE("tooltip_spawnfighter") );
                    break;

                case WorldObject::TargetTypeLaunchBomber:       
                    img = g_resource->GetImage( "graphics/bomber.bmp" );        
                    tooltip->PutData( LANGUAGEPHRASE("tooltip_spawnbomber") );
                    break;
                                                                
                case WorldObject::TargetTypeLaunchNuke:         
                    img = g_resource->GetImage( "graphics/nuke.bmp" );          
                    tooltip->PutData( LANGUAGEPHRASE("tooltip_spawnnuke") );
                    break;
            }
            
            float lineX = ( actionCursorLongitude - predictedLongitude );
            float lineY = ( actionCursorLatitude - predictedLatitude );

            float angle = atan( -lineX / lineY );
            if( lineY < 0.0f ) angle += M_PI;

            g_renderer->SetBlendMode( Renderer::BlendModeAdditive );
            g_renderer->Blit( img, actionCursorLongitude, actionCursorLatitude, 
                              spawnUnitSize/2.0f, spawnUnitSize/2.0f, 
                              spawnUnitCol, angle );           
        }


        if( validCombatTarget == WorldObject::TargetTypeValid )
        {
            tooltip->PutData( LANGUAGEPHRASE("tooltip_attacK") );
        }

        if( selection->IsMovingObject() )
        {
            if( validMovementTarget == WorldObject::TargetTypeValid )
            {
                if( highlight && validCombatTarget == WorldObject::TargetTypeLand )
                {
                    tooltip->PutData( LANGUAGEPHRASE("tooltip_landobject") );
                }

                if( !fleet )
                {
                    tooltip->PutData( LANGUAGEPHRASE("tooltip_moveobject") );
                }
            }

            if( fleet && fleetValidMovement )
            {
                tooltip->PutData( LANGUAGEPHRASE("tooltip_movefleet") );                
            }

            if( fleet && !fleetValidMovement )
            {
                tooltip->PutData( LANGUAGEPHRASE("tooltip_cantmovehere") );
            }
        }


        if( selection )
        {
            tooltip->PutData( LANGUAGEPHRASE("tooltip_spacetodeselect") );
        }

        //
        // How many do we have left?

        if( spawnType > WorldObject::TargetTypeValid)
        {
            int numRemaining = -1;
            if( selection->m_states[selection->m_currentState]->m_numTimesPermitted > -1 )
            {
                numRemaining = selection->m_states[selection->m_currentState]->m_numTimesPermitted - selection->m_actionQueue.Size();
            }
            
            if( numRemaining >= 0 )
            {
                g_renderer->SetFont( "kremlin", true );
                g_renderer->Text( actionCursorLongitude-2, actionCursorLatitude, White, 2, "%d", numRemaining );
                g_renderer->SetFont();
            }
        }
        
        g_renderer->SetBlendMode( Renderer::BlendModeNormal );

        RenderWorldObjectTargets(selection);

        bool animateActionLine = !fleet || fleetValidMovement || validCombatTarget >= WorldObject::TargetTypeValid;

        RenderActionLine( predictedLongitude, predictedLatitude, 
                          actionCursorLongitude, actionCursorLatitude, 
                          actionCursorCol, 2, animateActionLine );

        if( highlight && validCombatTarget )
        {
            RenderWorldObjectDetails( highlight );
        }

        if( selection->m_fleetId != -1 )
        {
            Fleet *fleet = team->GetFleet(selection->m_fleetId);
            for( int i = 0; i < fleet->m_fleetMembers.Size(); ++i )
            {
                WorldObject *obj = g_app->GetWorld()->GetWorldObject( fleet->m_fleetMembers[i] );
                if( obj &&
                    obj->m_objectId != selection->m_objectId )
                {
                    RenderWorldObjectTargets( obj, false );
                }
            }
        }


        //if( fleet )
        //{
        //    float sailDistance = g_app->GetWorld()->GetSailDistance( fleet->m_longitude, fleet->m_latitude, actionCursorLongitude, actionCursorLatitude );
        //    g_renderer->SetFont( NULL, true );
        //    g_renderer->Text( actionCursorLongitude, actionCursorLatitude, White, 3, "%2.2f", sailDistance );
        //    g_renderer->SetFont();
        //}
    }
    
    
    //
    // Render our current highlight

    if( highlight && !selection )
    {
        float predictedLongitude = highlight->m_longitude.DoubleValue() + highlight->m_vel.x.DoubleValue() * g_predictionTime * timeScale;
        float predictedLatitude = highlight->m_latitude.DoubleValue() + highlight->m_vel.y.DoubleValue() * g_predictionTime  * timeScale;
        float size = 3.0f;
        if( GetZoomFactor() <= 0.25f )
        {
            size *= GetZoomFactor() * 4;
        }

        Team *team = g_app->GetWorld()->GetTeam(highlight->m_teamId);
        Colour col(128,128,128);
        if( team ) col = team->GetTeamColour();
        col.m_a = 150;

        g_renderer->Circle( predictedLongitude, predictedLatitude, size, 30, col, 2.0f );
        g_renderer->Circle( predictedLongitude, predictedLatitude, size, 30, Colour(255,255,255,100), 2.0f );

        RenderWorldObjectTargets(highlight);
        RenderWorldObjectDetails(highlight);


        if( highlight->m_teamId == g_app->GetWorld()->m_myTeamId &&
            highlight->m_type != WorldObject::TypeCity &&
            highlight->m_type != WorldObject::TypeRadarStation )
        {
            bool lmbDone = false;

            if( highlight->m_fleetId != -1 )
            {
                Fleet *fleet = g_app->GetWorld()->GetTeam(highlight->m_teamId)->GetFleet(highlight->m_fleetId);
                if( fleet && fleet->m_fleetMembers.Size() > 1 )
                {
                    tooltip->PutData( LANGUAGEPHRASE("tooltip_selectfleet") );
                    lmbDone = true;
                }
            }

            if( !lmbDone && highlight->m_type != WorldObject::TypeNuke )
            {
                tooltip->PutData( LANGUAGEPHRASE("tooltip_selectobject") );
            }

            if( highlight->m_type != WorldObject::TypeBattleShip &&
                highlight->m_type != WorldObject::TypeFighter )
            {
                tooltip->PutData( LANGUAGEPHRASE("tooltip_setobjectstate") );
            }
        }
    }
#endif


    if( !highlight)
    {
        RenderEmptySpaceDetails(longitude, latitude);
    }


    //
    // Finish the tooltip swap

    if( m_tooltip )
    {
        delete m_tooltip;
        m_tooltip = NULL;
    }

    m_tooltip = tooltip;
}


void MapRenderer::RenderEmptySpaceDetails( float _mouseX, float _mouseY )
{
    int tooltipsEnabled = g_preferences->GetInt( PREFS_INTERFACE_TOOLTIPS );
    if( tooltipsEnabled && 
        m_tooltip &&
        m_tooltip->Size() )
    {
        float titleSize, textSize, gapSize;
        GetStatePositionSizes( &titleSize, &textSize, &gapSize );

        float boxH = (textSize+gapSize);
        float boxW = 18.0f * m_drawScale;
        float boxX = _mouseX - (boxW/3);
        float boxY = _mouseY - titleSize - titleSize;        
        float boxSep = gapSize;


        //
        // Fill box

        Colour windowColPrimary   = g_styleTable->GetPrimaryColour( STYLE_POPUP_BACKGROUND );
        Colour windowColSecondary = g_styleTable->GetSecondaryColour( STYLE_POPUP_BACKGROUND );
        Colour windowBorder       = g_styleTable->GetPrimaryColour( STYLE_POPUP_BORDER );    
        Colour fontCol            = g_styleTable->GetPrimaryColour( FONTSTYLE_POPUP_TITLE );
        bool windowOrientation    = g_styleTable->GetStyle( STYLE_POPUP_BACKGROUND )->m_horizontal;

        float alpha = (m_mouseIdleTime-1.0f);
        Clamp( alpha, 0.0f, 1.0f );

        windowColPrimary.m_a    *= alpha;
        windowColSecondary.m_a  *= alpha;
        windowBorder.m_a        *= alpha;
        fontCol.m_a             *= alpha;

        boxY += boxH;
        float totalBoxH = 0.0f;
        totalBoxH += m_tooltip->Size() * -boxH * 0.6f;


        g_renderer->RectFill( boxX, boxY, boxW, totalBoxH, windowColPrimary, windowColSecondary, windowOrientation );
        g_renderer->Rect( boxX, boxY, boxW, totalBoxH, windowBorder );


        //
        // Tooltip

        g_renderer->SetFont( "kremlin", true );

        float tooltipY = boxY - 0.5f * m_drawScale;
        RenderTooltip( &boxX, &tooltipY, &boxW, &boxH, &boxSep, alpha );

		g_renderer->SetFont();
    }
}


void MapRenderer::RenderTooltip( char *_tooltip )
{ 
    static char *s_previousTooltip = NULL;

    if( !s_previousTooltip ||
        strcmp(s_previousTooltip, _tooltip) != 0 )
    {
        // The tooltip has changed
        delete s_previousTooltip;
        s_previousTooltip = strdup(_tooltip);
        m_tooltipTimer = GetHighResTime();
    }

    

    int showTooltips = g_preferences->GetInt( PREFS_INTERFACE_TOOLTIPS );
    float timer = GetHighResTime() - m_tooltipTimer;

    if( showTooltips && 
        timer > 0.5f && 
        strlen(_tooltip) > 1 &&
        m_stateObjectId == -1 )
    {
        float alpha = (timer-0.5f);
        Clamp( alpha, 0.0f, 0.8f );

    
        //
        // Word wrap the tooltip

        float textH = 4 * m_zoomFactor;
        float maxW = 60 * m_zoomFactor;
		g_renderer->SetFont( "zerothre" );
        MultiLineText wrapped( _tooltip, maxW, textH, false );
    
        float widestLine = 0.0f;
        int numLines = wrapped.Size();
        for( int i = 0; i < numLines; ++i )
        {
            float thisWidth = g_renderer->TextWidth( wrapped[i], textH );
            widestLine = max( widestLine, thisWidth );
        }


        //
        // Render the box

        float longitude;
        float latitude;    
        ConvertPixelsToAngle( g_inputManager->m_mouseX, g_inputManager->m_mouseY, &longitude, &latitude );

        float boxX = longitude + 5 * m_zoomFactor;
        float boxY = latitude - 2 * m_zoomFactor;
        float boxH = textH * numLines;
        float boxW = widestLine + 4 * m_zoomFactor;

        //g_renderer->RectFill( boxX, boxY, boxW, boxH, Colour(130,130,50,alpha*255) );

        Colour windowColP  = g_styleTable->GetPrimaryColour( STYLE_TOOLTIP_BACKGROUND );
        Colour windowColS  = g_styleTable->GetSecondaryColour( STYLE_TOOLTIP_BACKGROUND );
        Colour borderCol   = g_styleTable->GetPrimaryColour( STYLE_TOOLTIP_BORDER );
    
        bool alignment = g_styleTable->GetStyle(STYLE_TOOLTIP_BACKGROUND)->m_horizontal;

        windowColP.m_a *= alpha;
        windowColS.m_a *= alpha;
        borderCol.m_a *= alpha;

        g_renderer->RectFill ( boxX, boxY, boxW, boxH, windowColS, windowColP, alignment );
        g_renderer->Rect     ( boxX, boxY, boxW, boxH, borderCol);

    
        //
        // Render the text
    
        g_renderer->SetFont( NULL, true );

        float textX = boxX + 2 * m_zoomFactor;
        float y = boxY - textH;
        for( int i = numLines-1; i >= 0; --i )
        {
            g_renderer->Text( textX, y+=textH, Colour(255,255,255,alpha*255), textH, wrapped[i] );
        }

        g_renderer->SetFont();
    }
}


void MapRenderer::RenderActionLine( float fromLong, float fromLat, float toLong, float toLat, Colour col, float width, bool animate )
{
#ifndef NON_PLAYABLE
    float distance      = g_app->GetWorld()->GetDistance( Fixed::FromDouble(fromLong), Fixed::FromDouble(fromLat),
														  Fixed::FromDouble(toLong), Fixed::FromDouble(toLat), true ).DoubleValue();
    float distanceLeft  = g_app->GetWorld()->GetDistance( Fixed::FromDouble(fromLong), Fixed::FromDouble(fromLat),
														  Fixed::FromDouble(toLong - 360), Fixed::FromDouble(toLat), true ).DoubleValue();
    float distanceRight = g_app->GetWorld()->GetDistance( Fixed::FromDouble(fromLong), Fixed::FromDouble(fromLat),
														  Fixed::FromDouble(toLong + 360), Fixed::FromDouble(toLat), true ).DoubleValue();
    if( distanceLeft < distance )
    {
        if( fromLong < 0.0f && toLong > 0.0f )
        {
            toLong -= 360.0f;
        }
    }
    else if( distanceRight < distance )
    {
        if( fromLong > 0.0f && toLong < 0.0f )
        {
            toLong += 360.0f;
        }
    }

    g_renderer->Line( fromLong, fromLat, toLong, toLat, col, width );
    g_renderer->Line( fromLong+GetLongitudeMod(), fromLat, toLong+GetLongitudeMod(), toLat, col, width );


    if( animate )
    {
        Vector3<float> lineVector( toLong - fromLong, toLat - fromLat, 0 );
        float factor1 = fmodf(GetHighResTime(), 1.0f );
        float factor2 = fmodf(GetHighResTime(), 1.0f ) + 0.2f;
        Clamp( factor1, 0.0f, 1.0f );
        Clamp( factor2, 0.0f, 1.0f );
        Vector3<float> fromVector = Vector3<float>(fromLong,fromLat,0) + lineVector * factor1;
        Vector3<float> toVector =  Vector3<float>(fromLong,fromLat,0) + lineVector * factor2;

        g_renderer->Line( fromVector.x, fromVector.y, toVector.x, toVector.y, col, width*2 );
    }


    // Render sail distances
    //double timeNow = GetHighResTime();
    //Fixed sailDistance = g_app->GetWorld()->GetSailDistance( Fixed::FromDouble(fromLong), Fixed::FromDouble(fromLat), Fixed::FromDouble(toLong), Fixed::FromDouble(toLat) );
    //double fastTime = GetHighResTime() - timeNow;
    //timeNow = GetHighResTime();
    //Fixed sailDistanceSlow = g_app->GetWorld()->GetSailDistanceSlow( Fixed::FromDouble(fromLong), Fixed::FromDouble(fromLat), Fixed::FromDouble(toLong), Fixed::FromDouble(toLat) );
    //double slowTime = GetHighResTime() - timeNow;

    //g_renderer->SetFont(NULL,true);
    //g_renderer->Text( toLong + 2, toLat - 2, White, 4, "f: %d (%dms)", sailDistance.IntValue(), int(fastTime*1000) );
    //g_renderer->Text( toLong + 2, toLat - 6, White, 4, "s: %d (%dms)", sailDistanceSlow.IntValue(), int(slowTime*1000)  );
    //g_renderer->SetFont();


    // Render issailable check

    //glPointSize( 3.0f );
    //glBegin( GL_POINTS );
    //glColor4f( 1.0f, 0.0f, 0.0f, 1.0f );

    //double timeNow = GetHighResTime();
    //bool sailableFast = g_app->GetWorld()->IsSailable( Fixed::FromDouble(fromLong), Fixed::FromDouble(fromLat), Fixed::FromDouble(toLong), Fixed::FromDouble(toLat) );
    //double fastTime = GetHighResTime() - timeNow;

    //glEnd();

    //g_renderer->SetFont(NULL,true);
    //g_renderer->Text( toLong + 2, toLat - 2, White, 4, "f: %s (%2.1fms)", sailableFast ? "true" : "false", (fastTime*1000) );
    //g_renderer->SetFont();



    ////
    //// Make the line move

    //Vector3 line( toLong - fromLong, toLat - fromLat, 0 );
    //Vector3 up( 0, 0, 1 );
    //Vector3 rightAngle = line ^ up;
    //rightAngle.SetLength( width * m_zoomFactor * 0.5f );

    //glColor4ubv( col.GetData() );

    //Image *img = g_resource->GetImage( "graphics/actionline.bmp" );
    //glBindTexture( GL_TEXTURE_2D, img->m_textureID );
    //glEnable( GL_TEXTURE_2D );
    //glTexParameteri	    (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
    //glTexParameteri	    (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );

    //g_renderer->SetBlendMode( Renderer::BlendModeAdditive );

    //float texX = -g_gameTime;
    //float texW = distance / 2;

    //glBegin( GL_QUADS );
    //glTexCoord2f(texX,0);       glVertex2f( fromLong-rightAngle.x, fromLat-rightAngle.y );    
    //glTexCoord2f(texX,1);  glVertex2f( fromLong+rightAngle.x, fromLat+rightAngle.y );
    //glTexCoord2f(texX+texW,1);  glVertex2f( toLong+rightAngle.x, toLat+rightAngle.y );
    //glTexCoord2f(texX+texW,0);       glVertex2f( toLong-rightAngle.x, toLat-rightAngle.y );
    //glEnd();

    //glTexParameteri	    (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
    //glTexParameteri	    (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );
    //glDisable( GL_TEXTURE_2D );

    //g_renderer->SetBlendMode( Renderer::BlendModeNormal );


#endif
}


void MapRenderer::RenderWorldObjectTargets( WorldObject *wobj, bool maxRanges )
{
#ifndef NON_PLAYABLE
    if( wobj->m_teamId == g_app->GetWorld()->m_myTeamId ||
        g_app->GetWorld()->m_myTeamId == -1 )
    {
        float predictedLongitude = wobj->m_longitude.DoubleValue() +
								   wobj->m_vel.x.DoubleValue() * g_predictionTime * g_app->GetWorld()->GetTimeScaleFactor().DoubleValue();
        float predictedLatitude = wobj->m_latitude.DoubleValue() +
								  wobj->m_vel.y.DoubleValue() * g_predictionTime * g_app->GetWorld()->GetTimeScaleFactor().DoubleValue();

        if( maxRanges )
        {
            int renderTooltip = g_preferences->GetInt( PREFS_INTERFACE_TOOLTIPS );

            //
            // Render red circle for our action range

            float range = wobj->GetActionRange().DoubleValue();
            if( m_currentStateId != -1 )
            {
                WorldObject *obj = g_app->GetWorld()->GetWorldObject( m_currentHighlightId );
                if( obj )
                {
                    if( obj->m_states.ValidIndex( m_currentStateId ) )
                    {
                        range = obj->m_states[m_currentStateId]->m_actionRange.DoubleValue();
                    }
                }

            }
            if( range <= 180.0f && range > 0.0f )
            {
                Colour col(255,0,0,255);
                g_renderer->Circle( predictedLongitude, predictedLatitude, range, 40, col, 1.0f );
                
                if( renderTooltip )
                {
                    g_renderer->SetFont( "kremlin", true );
                    g_renderer->TextCentreSimple( predictedLongitude, predictedLatitude+range, col, 1, LANGUAGEPHRASE("dialog_mapr_combat_range") );
                    g_renderer->SetFont();
                }
            }


            //
            // Render blue circle for our movement range

            if( wobj->IsMovingObject() )
            {
                MovingObject *mobj = (MovingObject *) wobj;
                if( mobj->m_range <= 180 && mobj->m_range > 0 )
                {                    
                    float predictedRange = mobj->m_range.DoubleValue();

                    Colour col(0,0,255,255);
                    g_renderer->Circle( predictedLongitude, predictedLatitude, predictedRange, 50, col, 2.0f );

                    if( renderTooltip )
                    {
                        g_renderer->SetFont( "kremlin", true );
                        g_renderer->TextCentreSimple( predictedLongitude, predictedLatitude+predictedRange, col, 1, LANGUAGEPHRASE("dialog_mapr_fuel_range") );
                        g_renderer->SetFont();
                    }
                }
            }   
        }
        
        
        //
        // Render red action line to our combat target

        int targetObjectId = wobj->GetTargetObjectId();
        WorldObject *targetObject = g_app->GetWorld()->GetWorldObject( targetObjectId );
        if( targetObject )
        {
            float TpredictedLongitude = targetObject->m_longitude.DoubleValue() + 
										targetObject->m_vel.x.DoubleValue() * g_predictionTime *
										g_app->GetWorld()->GetTimeScaleFactor().DoubleValue();
            float TpredictedLatitude = targetObject->m_latitude.DoubleValue() + 
									   targetObject->m_vel.y.DoubleValue() * g_predictionTime * 
									   g_app->GetWorld()->GetTimeScaleFactor().DoubleValue();

            Colour actionCursorCol( 255,0,0,150 );
            float actionCursorSize = 2.0f;
            float actionCursorAngle = g_gameTime * -1.0f;

            g_renderer->SetBlendMode( Renderer::BlendModeAdditive );

            Image *img = g_resource->GetImage( "graphics/cursor_target.bmp" );
            g_renderer->Blit( img, TpredictedLongitude, TpredictedLatitude, 
                                actionCursorSize, actionCursorSize, 
                                actionCursorCol, actionCursorAngle );

            g_renderer->SetBlendMode( Renderer::BlendModeNormal );            

            RenderActionLine( predictedLongitude, predictedLatitude, 
                              TpredictedLongitude, TpredictedLatitude, 
                              actionCursorCol, 0.5f );
        }
        

        //
        // Render blue action line to our movement target

        if( !targetObject && wobj->IsMovingObject() )
        {
            MovingObject *mobj = (MovingObject *) wobj;
            float actionCursorLongitude = mobj->m_targetLongitude.DoubleValue();
            float actionCursorLatitude = mobj->m_targetLatitude.DoubleValue();

            if( mobj->m_fleetId != -1 )
            {
                Fleet *fleet = g_app->GetWorld()->GetTeam(mobj->m_teamId)->GetFleet(mobj->m_fleetId);
                if( fleet && 
                    fleet->m_targetLongitude != 0 && 
                    fleet->m_targetLatitude != 0 )
                {
                    //predictedLongitude = fleet->m_longitude;
                    //predictedLatitude = fleet->m_latitude;
                    actionCursorLongitude = fleet->m_targetLongitude.DoubleValue();
                    actionCursorLatitude = fleet->m_targetLatitude.DoubleValue();                    
                }
            }
            if( actionCursorLongitude != 0.0f || actionCursorLatitude != 0.0f )
            {
                Colour actionCursorCol( 0,0,255,150 );
                float actionCursorSize = 2.0f;
                float actionCursorAngle = 0;

                g_renderer->SetBlendMode( Renderer::BlendModeAdditive );

                if( mobj->m_type == WorldObject::TypeNuke )
                {
                    actionCursorCol.Set( 255,0,0,150 );
                }

                if( mobj->m_type == WorldObject::TypeBomber &&
                    mobj->m_currentState == 1 )
                {
                    actionCursorCol.Set( 255,0,0,150 );
                }

                Image *img = g_resource->GetImage( "graphics/cursor_target.bmp" );
                g_renderer->Blit( img, actionCursorLongitude, actionCursorLatitude, 
                                    actionCursorSize, actionCursorSize, 
                                    actionCursorCol, actionCursorAngle );

                g_renderer->SetBlendMode( Renderer::BlendModeNormal );            

                RenderActionLine( predictedLongitude, predictedLatitude, 
                                  actionCursorLongitude, actionCursorLatitude, 
                                  actionCursorCol, 0.5f );
            }
        }


        //
        // Render our action queue

        if( wobj->m_actionQueue.Size() )
        {
            Image *img = NULL;
            float size = 1.0f;
            Colour col(255,255,255,100);

            switch( wobj->m_type )
            {
                case WorldObject::TypeAirBase:  
                case WorldObject::TypeCarrier:
                    if( wobj->m_currentState == 0 ) img = g_resource->GetImage( "graphics/fighter.bmp" );
                    if( wobj->m_currentState == 1 ) img = g_resource->GetImage( "graphics/bomber.bmp" );
                    break;

                case WorldObject::TypeSilo:
                    if( wobj->m_currentState == 0 ) img = g_resource->GetImage( "graphics/nuke.bmp" );
                    break;

                case WorldObject::TypeSub:
                    if( wobj->m_currentState == 2 ) img = g_resource->GetImage( "graphics/nuke.bmp" );
                    break;
            }
            
            if( img )
            {
                for( int i = 0; i < wobj->m_actionQueue.Size(); ++i )
                {
                    ActionOrder *order = wobj->m_actionQueue[i];
                    float targetLongitude = order->m_longitude.DoubleValue();
                    float targetLatitude = order->m_latitude.DoubleValue();

                    WorldObject *targetObject = g_app->GetWorld()->GetWorldObject( order->m_targetObjectId );
                    if( targetObject )
                    {
                        targetLongitude = targetObject->m_longitude.DoubleValue() +
										   targetObject->m_vel.x.DoubleValue() * g_predictionTime *
										   g_app->GetWorld()->GetTimeScaleFactor().DoubleValue();
                        targetLatitude = targetObject->m_latitude.DoubleValue() +
										 targetObject->m_vel.y.DoubleValue() * g_predictionTime *
										 g_app->GetWorld()->GetTimeScaleFactor().DoubleValue();
                    }

                    float lineX = ( targetLongitude - predictedLongitude );
                    float lineY = ( targetLatitude - predictedLatitude );

                    float angle = atan( -lineX / lineY );
                    if( lineY < 0.0f ) angle += M_PI;
                
                    g_renderer->SetBlendMode( Renderer::BlendModeAdditive );
                    g_renderer->Blit( img, targetLongitude, targetLatitude, size, size, col, angle );
                    g_renderer->SetBlendMode( Renderer::BlendModeNormal );
                    RenderActionLine( predictedLongitude, predictedLatitude,
                                      targetLongitude, targetLatitude,
                                      col, 0.5f );
                }
            }
        }
    }
#endif
}

void MapRenderer::RenderUnitHighlight( int _objectId )
{
    WorldObject *obj = g_app->GetWorld()->GetWorldObject( _objectId );
    if( obj )
    {
        float predictedLongitude = obj->m_longitude.DoubleValue() + 
								   obj->m_vel.x.DoubleValue() * g_predictionTime * g_app->GetWorld()->GetTimeScaleFactor().DoubleValue();
        float predictedLatitude = obj->m_latitude.DoubleValue() + 
							      obj->m_vel.y.DoubleValue() * g_predictionTime  * g_app->GetWorld()->GetTimeScaleFactor().DoubleValue();
        
        float size = 5.0f;
        if( GetZoomFactor() <=0.25f )
        {
            size *= GetZoomFactor() * 4;
        }

        Colour col = g_app->GetWorld()->GetTeam( obj->m_teamId )->GetTeamColour();
        col.m_a = 255;
        if( fmod((float)g_gameTime*2, 2.0f) < 1.0f ) col.m_a = 100;

        g_renderer->Line(predictedLongitude - size/2 - 5.0f, predictedLatitude, 
                         predictedLongitude - size/2, predictedLatitude, col );
        g_renderer->Line(predictedLongitude + size/2 + 5.0f, predictedLatitude, 
                         predictedLongitude + size/2, predictedLatitude, col );
        g_renderer->Line(predictedLongitude, predictedLatitude + size/2 + 5.0f, 
                         predictedLongitude, predictedLatitude + size/2, col );
        g_renderer->Line(predictedLongitude, predictedLatitude - size/2 - 5.0f, 
                         predictedLongitude, predictedLatitude - size/2, col );
        g_renderer->Rect( predictedLongitude - size/2, 
                          predictedLatitude - size/2, size, size, col, 2.0f );
    }
}

void MapRenderer::RenderNukeUnits()
{
    Team *team = g_app->GetWorld()->GetMyTeam();
    if( team )
    {
        for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
        {
            if( g_app->GetWorld()->m_objects.ValidIndex(i) )
            {
                WorldObject *obj = g_app->GetWorld()->m_objects[i];
                if( obj->m_teamId == team->m_teamId )
                {
                    int nukeCount = 0;

                    switch( obj->m_type )
                    {
                        case WorldObject::TypeSilo:
                            nukeCount = obj->m_states[0]->m_numTimesPermitted;
                            break;

                        case WorldObject::TypeSub:
                            nukeCount = obj->m_states[2]->m_numTimesPermitted;
                            break;

                        case WorldObject::TypeBomber:
                            nukeCount = obj->m_states[1]->m_numTimesPermitted;
                            break;

                        case WorldObject::TypeAirBase:
                        case WorldObject::TypeCarrier:
                            if( obj->m_nukeSupply > 0 )
                            {
                                nukeCount = obj->m_states[1]->m_numTimesPermitted;
                            }
                            break;
                    }

                    if( obj->UsingNukes() )
                    {
                        nukeCount -= obj->m_actionQueue.Size();
                    }

                    if( nukeCount > 0 )
                    {
                        RenderUnitHighlight( obj->m_objectId );
                    }
                }
            }
        }
    }
}


void MapRenderer::RenderLowDetailCoastlines()
{
    START_PROFILE( "Coastlines" );

    Image *bmpWorld = g_resource->GetImage( "graphics/map.bmp" );
    Colour col = White;

    g_renderer->Blit( bmpWorld, -180, 100, 360, -200, col );

    END_PROFILE( "Coastlines" );
}


void MapRenderer::RenderCoastlines()
{
    START_PROFILE( "Coastlines" );

    float mapColourFader = 1.0f;
    Team *myTeam = g_app->GetWorld()->GetMyTeam();
    if( myTeam ) mapColourFader = myTeam->m_teamColourFader;

    Colour lineColour           = g_styleTable->GetPrimaryColour( STYLE_WORLD_COASTLINES );
    Colour desaturatedColour    = g_styleTable->GetSecondaryColour( STYLE_WORLD_COASTLINES );
    lineColour = lineColour * mapColourFader + desaturatedColour * (1-mapColourFader);

    glColor4ub( lineColour.m_r, lineColour.m_g, lineColour.m_b, lineColour.m_a );
    glLineWidth( 1.5f );

    unsigned int displayListId;
    bool generated = g_resource->GetDisplayList( "MapCoastlines", displayListId );
    
    if( generated )
    {
        glNewList(displayListId, GL_COMPILE);

        LList<Island *> *list = &g_app->GetEarthData()->m_islands;
        if( g_preferences->GetInt(PREFS_GRAPHICS_LOWRESWORLD) == 1 )
        {
        }

        for( int i = 0; i < list->Size(); ++i )
        {
            Island *island = list->GetData(i);
            AppDebugAssert( island );

            glBegin( GL_LINE_STRIP );

            for( int j = 0; j < island->m_points.Size(); j++ )
            {
                Vector3<float> thePoint = *island->m_points[j];
                glVertex2f( thePoint.x, thePoint.y );
            }

            glEnd();
        }

        glEndList();
    }

    glCallList(displayListId);

    END_PROFILE( "Coastlines" );
}


void MapRenderer::RenderBorders()
{
    START_PROFILE( "Borders" );

    float mapColourFader = 1.0f;
    Team *myTeam = g_app->GetWorld()->GetMyTeam();
    if( myTeam ) mapColourFader = myTeam->m_teamColourFader;
    
    Colour lineColour = g_styleTable->GetPrimaryColour( STYLE_WORLD_BORDERS );
    Colour desaturatedColour = g_styleTable->GetSecondaryColour( STYLE_WORLD_BORDERS );
    lineColour = lineColour * mapColourFader + desaturatedColour * (1-mapColourFader);

    glColor4ub( lineColour.m_r, lineColour.m_g, lineColour.m_b, lineColour.m_a );
    glLineWidth( 0.5f );

    g_renderer->SetBlendMode( Renderer::BlendModeNormal );
    
    unsigned int displayListId;
    bool generated = g_resource->GetDisplayList( "MapBorders", displayListId );

    if( generated )
    {
        glNewList(displayListId, GL_COMPILE);

        for( int i = 0; i < g_app->GetEarthData()->m_borders.Size(); ++i )
        {
            Island *island = g_app->GetEarthData()->m_borders[i];
            AppDebugAssert( island );

            glBegin( GL_LINE_STRIP );

            for( int j = 0; j < island->m_points.Size(); j++ )
            {
                Vector3<float> thePoint = *island->m_points[j];
                glVertex2f( thePoint.x, thePoint.y );
            }

            glEnd();
        }

        glEndList();
    }

    glCallList( displayListId );

    END_PROFILE( "Borders" );
}

void MapRenderer::RenderObjects()
{
    START_PROFILE( "Objects" );

    int myTeamId = g_app->GetWorld()->m_myTeamId;
     
    for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
    {
        if( g_app->GetWorld()->m_objects.ValidIndex(i) )
        {            
            WorldObject *wobj = g_app->GetWorld()->m_objects[i];
            START_PROFILE( WorldObject::GetName(wobj->m_type) );

            g_renderer->SetBlendMode( Renderer::BlendModeAdditive );

            bool onScreen = IsOnScreen( wobj->m_longitude.DoubleValue(), wobj->m_latitude.DoubleValue() );
            if( onScreen || wobj->m_type == WorldObject::TypeNuke )
            {
                if( myTeamId == -1 ||
                    wobj->m_teamId == myTeamId ||
                    wobj->m_visible[myTeamId] ||
                    m_renderEverything )
                {
                    wobj->Render();
                }
                else
                {
                    wobj->RenderGhost(myTeamId);
                }
            }

#ifndef NON_PLAYABLE
            if( m_showOrders )
            {
                if(myTeamId == -1 || wobj->m_teamId == myTeamId )                
                {
                    RenderWorldObjectTargets(wobj, false);                // This shows ALL object orders on screen at once
                }
            }


            //
            // Render num nukes on the way

            if( onScreen )
            {
                if( wobj->m_numNukesInFlight || wobj->m_numNukesInQueue )
                {
                    Colour col(255,0,0,255);
                    if( !wobj->m_numNukesInFlight ) col.m_a = 100;
                    float iconSize = 2.0f;
                    float textSize = 0.75f;

                    if( wobj->m_numNukesInFlight ) iconSize += sinf(g_gameTime*10) * 0.2f;

                    Image *img = g_resource->GetImage( "graphics/nukesymbol.bmp" );
                    g_renderer->Blit( img, wobj->m_longitude.DoubleValue(), wobj->m_latitude.DoubleValue(), iconSize, iconSize, col, 0 );

                    g_renderer->SetFont( "kremlin", true );

                    float yPos = wobj->m_latitude.DoubleValue()+1.6f;
                    if( wobj->m_numNukesInQueue )
                    {
                        col.m_a = 100;
						char caption[128];
						sprintf( caption, LANGUAGEPHRASE("dialog_mapr_nukes_in_queue") );
						LPREPLACEINTEGERFLAG( 'N', wobj->m_numNukesInQueue, caption );
                        g_renderer->TextCentreSimple( wobj->m_longitude.DoubleValue(), yPos, col, textSize, caption );
                        yPos += 0.5f;
                    }

                    if( wobj->m_numNukesInFlight )
                    {
                        col.m_a = 255;
						char caption[128];
						sprintf( caption, LANGUAGEPHRASE("dialog_mapr_nukes_in_flight") );
						LPREPLACEINTEGERFLAG( 'N', wobj->m_numNukesInFlight, caption );
                        g_renderer->TextCentreSimple( wobj->m_longitude.DoubleValue(), yPos, col, textSize, caption );
                    }

                    g_renderer->SetFont();                
                }
            }
#endif
            END_PROFILE( WorldObject::GetName(wobj->m_type) );
        }
    }

#ifndef NON_PLAYABLE
    WorldObject *selection = g_app->GetWorld()->GetWorldObject(m_currentSelectionId);

    if( selection  )
    {
        if( selection->m_teamId == myTeamId )
        {
            if( selection->IsMovingObject() )
            {
                float longitude, latitude;
                ConvertPixelsToAngle( g_inputManager->m_mouseX, g_inputManager->m_mouseY, &longitude, &latitude );
                MovingObject *mobj = (MovingObject *)selection;
                if( mobj->m_movementType == MovingObject::MovementTypeSea &&
                    mobj->IsValidPosition( Fixed::FromDouble(longitude), Fixed::FromDouble(latitude) ) &&
                    m_currentSelectionId != m_currentHighlightId)
                {
                    Fleet *fleet = g_app->GetWorld()->GetTeam( mobj->m_teamId )->GetFleet( mobj->m_fleetId );
                    if( fleet )
                    {
                        fleet->CreateBlip();
                    }
                }
            }
        }
    }

    WorldObject *highlight = g_app->GetWorld()->GetWorldObject( m_currentHighlightId );
 
    if( highlight  )
    {
        if(m_currentHighlightId != m_currentSelectionId )
        {
            if( highlight->m_teamId == myTeamId )
            {
                if( highlight->IsMovingObject() )
                {
                    MovingObject *mobj = (MovingObject *)highlight;
                    if( mobj->m_movementType == MovingObject::MovementTypeSea )
                    {
                        Fleet *fleet = g_app->GetWorld()->GetTeam( mobj->m_teamId )->GetFleet( mobj->m_fleetId );
                        if( fleet )
                        {
                            fleet->CreateBlip();
                        }
                    }
                }
            }
        }
    }
#endif
 


    END_PROFILE( "Objects" );
}

void MapRenderer::RenderBlips()
{
    g_renderer->SetBlendMode( Renderer::BlendModeAdditive );
    
    int teamId = g_app->GetWorld()->m_myTeamId;
    Team *team = g_app->GetWorld()->GetTeam( teamId );
    if( team )
    {
        for( int i = 0; i < team->m_fleets.Size(); ++i )
        {
            Fleet *fleet = team->GetFleet(i);
            if( fleet )
            {
                for( int j = 0; j < fleet->m_movementBlips.Size(); ++j )
                {
                    Blip *blip = fleet->m_movementBlips[j];
                    if( blip->Update() )
                    {
                        fleet->m_movementBlips.RemoveData(j);
                        j--;
                        delete blip;
                    }
                }
            }
        }
    }
}

void MapRenderer::RenderCities()
{
    START_PROFILE( "Cities" );

    g_renderer->SetFont( "kremlin", true );

    g_renderer->SetBlendMode( Renderer::BlendModeAdditive );
    Image *cityImg = g_resource->GetImage( "graphics/city.bmp" );


    //
    // City icons

    for( int i = 0; i < g_app->GetWorld()->m_cities.Size(); ++i )
    {
        City *city = g_app->GetWorld()->m_cities.GetData(i);

        if( city->m_capital || city->m_teamId != -1 )
        {            
            if( IsOnScreen( city->m_longitude.DoubleValue(), city->m_latitude.DoubleValue() ) )
            {
                float size = sqrtf( sqrtf((float) city->m_population) ) / 25.0f;
                float textSize = size;
                if( m_zoomFactor <= 0.2f )
                {
                    size *= m_zoomFactor * 5;
                }

                Colour col(100,100,100,100);
                if( city->m_teamId > -1 ) 
                {
                    col = g_app->GetWorld()->GetTeam(city->m_teamId)->GetTeamColour();
                }                            
                col.m_a = 200.0f * ( 1.0f - min(0.8f, m_zoomFactor) );
                            
                g_renderer->Blit( cityImg,
                                  city->m_longitude.DoubleValue()-size/2, 
                                  city->m_latitude.DoubleValue()-size/2,
                                  size, size, col );

            
                //
                // Nuke icons

                if( city->m_numNukesInFlight || city->m_numNukesInQueue )
                {
                    Colour col(255,0,0,255);
                    if( !city->m_numNukesInFlight ) col.m_a = 100;
                    float iconSize = 2.0f;
                    float textSize = 0.75f;

                    if( city->m_numNukesInFlight ) iconSize += sinf(g_gameTime*10) * 0.2f;

                    Image *img = g_resource->GetImage( "graphics/nukesymbol.bmp" );
                    g_renderer->Blit( img, city->m_longitude.DoubleValue(), city->m_latitude.DoubleValue(), iconSize, iconSize, col, 0 );

                    float yPos = city->m_latitude.DoubleValue()+1.6f;
                    if( city->m_numNukesInQueue )
                    {
                        col.m_a = 100;
						char caption[128];
						sprintf( caption, LANGUAGEPHRASE("dialog_mapr_nukes_in_queue") );
						LPREPLACEINTEGERFLAG( 'N', city->m_numNukesInQueue, caption );
                        g_renderer->TextCentreSimple( city->m_longitude.DoubleValue(), yPos, col, textSize, caption );
                        yPos += 0.5f;
                    }

                    if( city->m_numNukesInFlight )
                    {
                        col.m_a = 255;
						char caption[128];
						sprintf( caption, LANGUAGEPHRASE("dialog_mapr_nukes_in_flight") );
						LPREPLACEINTEGERFLAG( 'N', city->m_numNukesInFlight, caption );
                        g_renderer->TextCentreSimple( city->m_longitude.DoubleValue(), yPos, col, textSize, caption );
                    }
                }
            }            
        }
    }
        
    
    //
    // City / country names

    bool showCityNames      = g_preferences->GetInt( PREFS_GRAPHICS_CITYNAMES );
    bool showCountryNames   = g_preferences->GetInt( PREFS_GRAPHICS_COUNTRYNAMES );

    if( showCityNames || showCountryNames )
    {
        Colour cityColour       (120,180,255,255);
        Colour countryColour    (150,255,150,0);

        cityColour.m_a      = 200.0f * (1.0f - sqrtf(m_zoomFactor));
        countryColour.m_a   = 200.0f * (1.0f - sqrtf(m_zoomFactor));

        for( int i = 0; i < g_app->GetWorld()->m_cities.Size(); ++i )
        {
            City *city = g_app->GetWorld()->m_cities.GetData(i);

            if( city->m_capital || city->m_teamId != -1 )
            {            
                if( IsOnScreen( city->m_longitude.DoubleValue(), city->m_latitude.DoubleValue() ) )
                {
                    float textSize = sqrtf( sqrtf((float) city->m_population) ) / 25.0f;

                    if( showCityNames )
                    {                                                  
                        textSize *= textSize * 0.5;
                        textSize *= sqrtf( sqrtf( m_zoomFactor ) ) * 0.8f;

                        g_renderer->TextCentreSimple( city->m_longitude.DoubleValue(), city->m_latitude.DoubleValue(), cityColour, textSize, LANGUAGEPHRASEADDITIONAL(city->m_name) );
                    }                

                    if( showCountryNames && city->m_capital )
                    {
                        float countrySize = textSize;
                        g_renderer->TextCentreSimple( city->m_longitude.DoubleValue(), city->m_latitude.DoubleValue()-textSize, countryColour, countrySize, LANGUAGEPHRASEADDITIONAL(city->m_country) );
                    }
                }
            }
        }
    }

	g_renderer->SetFont();

    END_PROFILE( "Cities" );
}

void MapRenderer::RenderPopulationDensity()
{
    START_PROFILE( "Population Density" );

    g_renderer->SetBlendMode( Renderer::BlendModeAdditive );
    
    for( int i = 0; i < g_app->GetWorld()->m_cities.Size(); ++i )
    {
        if( g_app->GetWorld()->m_cities.ValidIndex(i) )
        {
            City *city = g_app->GetWorld()->m_cities.GetData(i);

            if( city->m_teamId != -1 )
            {            
                float size = sqrtf( sqrtf((float) city->m_population) ) / 2.0f;

                Colour col = g_app->GetWorld()->GetTeam(city->m_teamId)->GetTeamColour();
                col.m_a = 255.0f * min( 1.0f, city->m_population / 10000000.0f );
                                    
                g_renderer->Blit( g_resource->GetImage( "graphics/explosion.bmp" ),
                                            city->m_longitude.DoubleValue()-size/2, city->m_latitude.DoubleValue()-size/2,
                                            size, size, col );
            }
        }
    }

    END_PROFILE( "Population Density" );
}


void MapRenderer::RenderRadar( bool _allies, bool _outlined )
{
    int myTeamId = g_app->GetWorld()->m_myTeamId;
    float timeFactor = g_predictionTime * g_app->GetWorld()->GetTimeScaleFactor().DoubleValue();

    for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
    {
        if( g_app->GetWorld()->m_objects.ValidIndex(i) )
        {
            WorldObject *wobj = g_app->GetWorld()->m_objects[i];            
            Team *team = g_app->GetWorld()->GetTeam(wobj->m_teamId);

            if( (_allies && team->m_sharingRadar[myTeamId]) ||
                (!_allies && wobj->m_teamId == myTeamId) )
            {
                float size = wobj->GetRadarRange().DoubleValue();      
                if( size > 0.0f )
                {
                    float predictedLongitude = wobj->m_longitude.DoubleValue() + wobj->m_vel.x.DoubleValue() * timeFactor;
                    float predictedLatitude = wobj->m_latitude.DoubleValue() + wobj->m_vel.y.DoubleValue() * timeFactor;
                    
                    if( _outlined )
                    {
                        Colour col(150,150,255,255);
                        float lineWidth = 3.0f;
                        if( wobj->m_teamId != myTeamId )
                        {
                            col.m_a = 50;
                            lineWidth = 1.0f;
                        }
                        g_renderer->Circle( predictedLongitude-360, predictedLatitude, size+0.1f, 50, col, lineWidth );
                        g_renderer->Circle( predictedLongitude,     predictedLatitude, size+0.1f, 50, col, lineWidth );
                        g_renderer->Circle( predictedLongitude+360, predictedLatitude, size+0.1f, 50, col, lineWidth );
                    }
                    else
                    {
                        Colour col(150,150,200,30);
                        if( wobj->m_teamId != myTeamId )
                        {
                            col.m_a = 0;
                        }

                        g_renderer->CircleFill( predictedLongitude-360, predictedLatitude, size, 50, col );
                        g_renderer->CircleFill( predictedLongitude,     predictedLatitude, size, 50, col );
                        g_renderer->CircleFill( predictedLongitude+360, predictedLatitude, size, 50, col ) ;
                    }
                }
            }
        }
    }
}


void MapRenderer::RenderRadar()
{
    START_PROFILE( "Radar" );

    float timeFactor = g_predictionTime * g_app->GetWorld()->GetTimeScaleFactor().DoubleValue();

    g_renderer->SetBlendMode( Renderer::BlendModeAdditive );
    g_renderer->SetDepthBuffer( true, true );

    //
    // Render our Radar first

    RenderRadar( false, false );
    RenderRadar( false, true );


    //
    // Now Render allied radar if we are sharing

    int myTeamId = g_app->GetWorld()->m_myTeamId;
    bool sharingRadar = false;
    for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
    {
        Team *team = g_app->GetWorld()->m_teams[i];
        if( team->m_teamId != myTeamId &&
            myTeamId != -1 &&
            team->m_sharingRadar[myTeamId] )
        {
            sharingRadar = true;
            break;
        }
    }

    if( sharingRadar )
    {
        RenderRadar( true, false );
        RenderRadar( true, true );
    }


    //
    // Now darken the whole world not covered by Radar

    g_renderer->SetBlendMode( Renderer::BlendModeNormal );        
    g_renderer->RectFill( -180, -100, 360, 200, Colour(0,0,0,150) );
    g_renderer->SetDepthBuffer( false, false );
    
    END_PROFILE( "Radar" );
}

void MapRenderer::RenderNodes()
{
    if( g_preferences->GetInt( "RenderNodes" ) != 1 )
    {
        return;
    }


    //
    // Placement points
     
    for( int i = 0; i < g_app->GetWorld()->m_aiPlacementPoints.Size(); ++i )
    {
        Vector3<Fixed> *thisPoint = g_app->GetWorld()->m_aiPlacementPoints[i];

        Colour col(0,255,0,55);

        g_renderer->CircleFill( thisPoint->x.DoubleValue(), thisPoint->y.DoubleValue(), 0.5, 20, col );
    }


    //
    // Target points

    for( int i = 0; i < g_app->GetWorld()->m_aiTargetPoints.Size(); ++i )
    {
        Vector3<Fixed> *thisPoint = g_app->GetWorld()->m_aiTargetPoints[i];

        Fixed attackRange = 60;

        bool inRange = false;
        for( int j = 0; j < World::NumTerritories; ++j )
        {
            int owner = g_app->GetWorld()->GetTerritoryOwner(j);
            if( owner != -1 &&
                g_app->GetWorld()->m_myTeamId != -1 &&
                !g_app->GetWorld()->IsFriend( owner, g_app->GetWorld()->m_myTeamId ) )
            {
                Fixed popCenterLong = g_app->GetWorld()->m_populationCenter[j].x;
                Fixed popCenterLat = g_app->GetWorld()->m_populationCenter[j].y;
                if( g_app->GetWorld()->GetDistance( thisPoint->x, thisPoint->y, popCenterLong, popCenterLat ) < attackRange )
                {
                    inRange = true;
                    break;
                }
            }
        }

        Colour col(255,0,0,255);
        
        if( inRange )
        {
            g_renderer->CircleFill( thisPoint->x.DoubleValue(), thisPoint->y.DoubleValue(), 1, 20, col );
        }
        else
        {
            g_renderer->Circle( thisPoint->x.DoubleValue(), thisPoint->y.DoubleValue(), 1, 20, col );
        }
    }


    //
    // Travel node graph

    g_renderer->SetFont( NULL, true );

    for( int i = 0; i < g_app->GetWorld()->m_nodes.Size(); ++i )
    {
        if( g_app->GetWorld()->m_nodes.ValidIndex(i) )
        {
            Node *node = g_app->GetWorld()->m_nodes[i];
            g_renderer->CircleFill( node->m_longitude.DoubleValue(), node->m_latitude.DoubleValue(), 1.0f, 20, Colour(255,255,255,150) );
            char num[4];
            sprintf(num, "%d", i );

            g_renderer->Text( node->m_longitude.DoubleValue() + 0.5f, 
                              node->m_latitude.DoubleValue() + 0.5f, White, 2.0f, num );
            
            for( int j = 0; j < node->m_routeTable.Size(); ++j )
            {
                Node *nextNode = g_app->GetWorld()->m_nodes[node->m_routeTable[j]->m_nextNodeId];
                g_renderer->Line(node->m_longitude.DoubleValue(), 
                                 node->m_latitude.DoubleValue(), 
                                 nextNode->m_longitude.DoubleValue(), 
                                 nextNode->m_latitude.DoubleValue(), 
                                 Colour(255,255,255,35));
            }
        }
    }

    g_renderer->SetFont();
}


void MapRenderer::UpdateCameraControl( float longitude, float latitude )
{
    //
    // Handle zooming

    m_totalZoom += g_inputManager->m_mouseVelZ * g_preferences->GetFloat(PREFS_INTERFACE_ZOOM_SPEED, 1.0);

    Clamp( m_totalZoom, 0.0f, 30.0f );
    float zoomFactor = pow(0.9f, m_totalZoom );

    if( zoomFactor < 0.05f ) zoomFactor = 0.05f;
    if( zoomFactor > 1.0f ) zoomFactor = 1.0f;    

    if( fabs(zoomFactor - m_zoomFactor) > 0.01f )
    {        
        float factor1 = g_advanceTime * 5.0f;
        float factor2 = 1.0f - factor1;
        m_zoomFactor = zoomFactor * factor1 + m_zoomFactor * factor2;

        float factor3 = g_advanceTime * 0.5f;
        float factor4 = 1.0f - factor3;

        int x = longitude;
        int y = latitude;

        m_middleX = x * factor3 + m_middleX * factor4;
        m_middleY = y * factor3 + m_middleY * factor4;        
    }

    if( longitude < -180.0f ) longitude += 360.0f;
    if( longitude >180.0f ) longitude -= 360.0f;


    //
    // Quake key support

    float left, right, top, bottom, width, height;
    GetWindowBounds( &left, &right, &top, &bottom );
    width = right - left;
    height = bottom - top;

    //MoveCam();

    bool ignoreKeys = g_app->GetInterface()->UsingChatWindow();
    if( !m_lockCamControl )
    {
        float oldX = m_middleX;
        float oldY = m_middleY;
        float scrollSpeed = 150.0f * m_zoomFactor * g_advanceTime;
        int edgeSize = 10;

        // only scroll if we're the foreground app, and the user allows us to
        bool sideScrolling = g_inputManager->m_windowHasFocus && g_preferences->GetInt( PREFS_INTERFACE_SIDESCROLL );
        int keyboardLayout = g_preferences->GetInt( PREFS_INTERFACE_KEYBOARDLAYOUT );

		if ( keyboardLayout == -1 ) // Not yet defined
		{
#ifdef TARGET_OS_MACOSX
			char* layout = GetKeyboardLayout();
			if ( !strcmp( layout, "French" ) || !strcmp( layout, "Belgian" ) )
				keyboardLayout = 2;
			else if ( !strcmp( layout, "Italian" ) )
				keyboardLayout = 3;
			else if ( !strcmp( layout, "Dvorak" ) )
				keyboardLayout = 4;
			else
				keyboardLayout = 1;
			delete layout;
				
			g_preferences->SetInt( PREFS_INTERFACE_KEYBOARDLAYOUT, keyboardLayout );
#endif
		}

        if( m_draggingCamera ) sideScrolling = false;
		int KeyQ = KEY_Q, KeyW = KEY_W, KeyE = KEY_E, KeyA = KEY_A, KeyS = KEY_S, KeyD = KEY_D;
		
		switch ( keyboardLayout) {
		case 2:			// AZERTY
			KeyQ = KEY_A;
			KeyW = KEY_Z;
			KeyA = KEY_Q;
			break;
		case 3:			// QZERTY
			KeyW = KEY_Z;
			break;
		case 4:			// DVORAK
			KeyQ = KEY_QUOTE;
			KeyW = KEY_COMMA;
			KeyE = KEY_STOP;
			KeyS = KEY_O;
			KeyD = KEY_E;
			break;
		default:		// QWERTY/QWERTZ (1) or invaild pref - do nothing
			break;
		}
        if( ((g_keys[KeyA] || g_keys[KEY_LEFT]) && !ignoreKeys)   || (g_inputManager->m_mouseX <= edgeSize && sideScrolling ) )			   m_middleX -= scrollSpeed;
        if( ((g_keys[KeyD] || g_keys[KEY_RIGHT]) && !ignoreKeys)  || (g_inputManager->m_mouseX >= m_pixelW-edgeSize  && sideScrolling ))   m_middleX += scrollSpeed;
        if( ((g_keys[KeyW] || g_keys[KEY_UP]) && !ignoreKeys)     || (g_inputManager->m_mouseY <= edgeSize && sideScrolling ))             m_middleY += scrollSpeed;
        if( ((g_keys[KeyS] || g_keys[KEY_DOWN]) && !ignoreKeys)   || (g_inputManager->m_mouseY >= m_pixelH-edgeSize && sideScrolling ) )   m_middleY -= scrollSpeed;

		if( g_keys[KeyE] && !ignoreKeys) m_totalZoom -= 20 * g_advanceTime;
		if( g_keys[KeyQ] && !ignoreKeys) m_totalZoom += 20 * g_advanceTime;

        if( m_middleX == oldX && 
            m_middleY == oldY )
        {
            m_cameraIdleTime += SERVER_ADVANCE_PERIOD.DoubleValue();
        }
        else
        {
            m_cameraIdleTime = 0.0f;
        }

        if( g_preferences->GetInt( PREFS_INTERFACE_CAMDRAGGING ) == 1 )
        {
            DragCamera();
        }
        else
        {
            if( !g_app->MousePointerIsVisible() )
            {
                g_app->SetMousePointerVisible ( true );
            }
        }

    }


    float maxWidth = 360.0f;    
    float aspect = (float) m_pixelH / (float) m_pixelW;
    float maxHeight = (360.0f * aspect);

    if( m_middleX > maxWidth/2 ) m_middleX = -maxWidth/2 + m_middleX-maxWidth/2;
    if( m_middleX < -maxWidth/2 ) m_middleX = 180 - m_middleX/-maxWidth/2;
    if( m_middleY + height/2 < -maxHeight/2 ) m_middleY = -maxHeight/2 - height/2;
    if( m_middleY - height/2 > maxHeight/2 ) m_middleY = maxHeight/2 + height/2;
}


int MapRenderer::GetNearestObjectToMouse( float _mouseX, float _mouseY )
{
    if( _mouseX > 180 ) _mouseX = -180 + ( _mouseX - 180 );
    if( _mouseX < -180 ) _mouseX = 180 + ( _mouseX + 180 );

    int underMouseId = -1;

    Fixed nearest = 5;
    if( m_currentSelectionId != -1 )
    {
        nearest = 2;
    }

    if( g_app->GetMapRenderer()->GetZoomFactor() <= 0.25f )
    {
        nearest *= Fixed::FromDouble(g_app->GetMapRenderer()->GetZoomFactor() * 4);
    }

    //
    // Find the nearest object to the Mouse

    for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
    {
        if( g_app->GetWorld()->m_objects.ValidIndex(i) )
        {
            WorldObject *wobj = g_app->GetWorld()->m_objects[i];
            if( g_app->GetWorld()->m_myTeamId == -1 ||
                wobj->m_visible[g_app->GetWorld()->m_myTeamId] ||
                wobj->m_lastSeenTime[g_app->GetWorld()->m_myTeamId] > 0 ||
                g_app->GetGame()->m_winner != -1 )
            {
                Fixed distance = g_app->GetWorld()->GetDistance( wobj->m_longitude, wobj->m_latitude,
																 Fixed::FromDouble(_mouseX), Fixed::FromDouble(_mouseY) );
                if( distance < nearest )
                {
                    underMouseId = wobj->m_objectId;
                    nearest = distance;
                }
            }
        }
    }

    //
    // Any cities nearer?

    for( int i = 0; i < g_app->GetWorld()->m_cities.Size(); ++i )
    {
        City *city = g_app->GetWorld()->m_cities[i];
        Fixed distance = g_app->GetWorld()->GetDistance( city->m_longitude, city->m_latitude,
														 Fixed::FromDouble(_mouseX), Fixed::FromDouble(_mouseY) );
        if( distance < nearest )
        {
            underMouseId = city->m_objectId;
            nearest = distance;
        }
    }

    return underMouseId;
}


void MapRenderer::HandleSelectObject( int _underMouseId )
{
    bool landed = false;
    bool selected = false;

    if( _underMouseId != m_currentSelectionId )
    {
        WorldObject *selection = g_app->GetWorld()->GetWorldObject(m_currentSelectionId);
        WorldObject *undermouse = g_app->GetWorld()->GetWorldObject(_underMouseId);

        if( undermouse->m_teamId != g_app->GetWorld()->m_myTeamId )
        {
            return;
        }

        //
        // If clicking on airbase/carrier with aircraft selected, request landing

        if( selection && 
            undermouse &&
            selection->m_teamId == g_app->GetWorld()->m_myTeamId &&
            undermouse->m_teamId == g_app->GetWorld()->m_myTeamId )
        {
            bool selectionLandable = ( selection->m_type == WorldObject::TypeFighter ||
                                       selection->m_type == WorldObject::TypeBomber );

            bool underMouseLandable = ( undermouse->m_type == WorldObject::TypeAirBase ||
                                        undermouse->m_type == WorldObject::TypeCarrier );

            if( selectionLandable && underMouseLandable )
            {
                g_app->GetClientToServer()->RequestSpecialAction( m_currentSelectionId, _underMouseId, World::SpecialActionLandingAircraft );
                CreateAnimation( AnimationTypeActionMarker, selection->m_objectId,
								 undermouse->m_longitude.DoubleValue(), undermouse->m_latitude.DoubleValue() );
                SetCurrentSelectionId(-1);
                landed = true;
            }
        }


        //
        // If not then try to select the object

        bool selectable = undermouse->IsActionable() ||
                          undermouse->IsMovingObject();

        if( !landed &&
            selectable &&
            undermouse->m_type != WorldObject::TypeNuke )
        {
            SetCurrentSelectionId(_underMouseId);
            selected = true;
        }
    }


    if( !landed && !selected )
    {
        SetCurrentSelectionId(-1);
    }
}


void MapRenderer::HandleClickStateMenu()
{
    WorldObject *highlight = g_app->GetWorld()->GetWorldObject(m_currentHighlightId);
    if( highlight )
    {
        if( g_keys[KEY_SHIFT] &&
            highlight->m_fleetId != -1 )
        {
            //
            // SHIFT pressed - set for all units in fleet

            Fleet *fleet = g_app->GetWorld()->GetTeam(highlight->m_teamId)->GetFleet(highlight->m_fleetId);
            if( fleet )
            {
                for( int i = 0; i < fleet->m_fleetMembers.Size(); ++i )
                {
                    int thisObjId = fleet->m_fleetMembers[i];
                    WorldObject *thisObj = g_app->GetWorld()->GetWorldObject(thisObjId);
                    if( thisObj && 
                        thisObj->m_type == highlight->m_type )
                    {
                        if( m_currentStateId == CLEARQUEUE_STATEID )
                        {
                            g_app->GetClientToServer()->RequestClearActionQueue( thisObj->m_objectId );
                            g_soundSystem->TriggerEvent( "Interface", "SelectObjectState" );           
                        }
                        else
                        {
                            if( thisObj->CanSetState(m_currentStateId) )
                            {
                                g_app->GetClientToServer()->RequestStateChange( thisObj->m_objectId, m_currentStateId );
                                g_soundSystem->TriggerEvent( "Interface", "SelectObjectState" );           
                            }
                            else
                            {
                                g_soundSystem->TriggerEvent( "Interface", "Error" );
                            }
                        }                        
                    }
                }
            }
        }
        else
        {
            if( m_currentStateId == CLEARQUEUE_STATEID )
            {
                //
                // Clear action queue clicked

                g_app->GetClientToServer()->RequestClearActionQueue( m_currentHighlightId );
                g_soundSystem->TriggerEvent( "Interface", "SelectObjectState" );           
            }
            else
            {
                g_app->GetClientToServer()->RequestStateChange( m_currentHighlightId, m_currentStateId );

                //
                // Select the object if it can spawn/do stuff
                
                if( highlight )
                {
                    if( highlight->m_currentState == m_currentStateId ||
                        highlight->CanSetState(m_currentStateId) )
                    {                         
                        g_soundSystem->TriggerEvent( "Interface", "SelectObjectState" );           

                        if( highlight->m_states[m_currentStateId]->m_isActionable )
                        {
                            SetCurrentSelectionId( m_currentHighlightId );                             
                        }
                    }
                    else
                    {
                        g_soundSystem->TriggerEvent( "Interface", "Error" );
                    }
                }
            }
        }
    }

    m_stateRenderTime = 0.0f;
    m_stateObjectId = -1;
}


void MapRenderer::HandleObjectAction( float _mouseX, float _mouseY, int underMouseId )
{
    WorldObject *underMouse = g_app->GetWorld()->GetWorldObject(underMouseId);

    if( m_currentSelectionId != -1 )
    {
        WorldObject *obj = g_app->GetWorld()->GetWorldObject( m_currentSelectionId );
        if( obj )
        {
            int numTimesRemaining = obj->m_states[obj->m_currentState]->m_numTimesPermitted;
            if( numTimesRemaining > -1 ) numTimesRemaining -= obj->m_actionQueue.Size();

            if( numTimesRemaining == 0 )
            {
                SetCurrentSelectionId(-1);
            }
            else if( numTimesRemaining == -1 || numTimesRemaining > 0 )
            {
                Fixed targetLong;
                Fixed targetLat;

                if( underMouse )
                {
                    targetLong = underMouse->m_longitude;
                    targetLat = underMouse->m_latitude;
                }
                else
                {
                    targetLong = Fixed::FromDouble( _mouseX );
                    targetLat = Fixed::FromDouble( _mouseY );
                }

                //
                // This section is a hack
                // designed to give good "negative" feedback when a user tries to do something they can't

                bool canAction = true;
                if( !underMouse )
                {
                    if( obj->m_type == WorldObject::TypeBattleShip )
                    {
                        // With battleships and subs, Left clicking in open space does nothing
                        canAction = false;
                    }

                    if( obj->m_type == WorldObject::TypeSub )
                    {
                        canAction = false;
                    }

                    if( obj->m_type == WorldObject::TypeCarrier ||
                        obj->m_type == WorldObject::TypeAirBase )
                    {
                        if( obj->IsValidMovementTarget( targetLong, targetLat )
                            < WorldObject::TargetTypeValid )
                        {
                            // Carrier/Airbase, but cant launch anything for whatever reason
                            canAction = false;
                        }
                    }
                }
                else
                {
                    if( obj->m_type == WorldObject::TypeCarrier ||
                        obj->m_type == WorldObject::TypeAirBase )
                    {
                        if( obj->IsValidMovementTarget( targetLong, targetLat )
                            < WorldObject::TargetTypeValid )
                        {
                            // Carrier/Airbase, but cant launch anything for whatever reason
                            canAction = false;
                        }
                    }
                }

                if( !canAction )
                {
                    g_soundSystem->TriggerEvent( "Interface", "Error" );
                    m_tooltipTimer = 0.0f;
                }
                else
                {
                    if( obj->SetWaypointOnAction() )
                    {
                        g_app->GetClientToServer()->RequestSetWaypoint( m_currentSelectionId, targetLong,
                                                                        targetLat );
                    }
                    g_app->GetClientToServer()->RequestAction( m_currentSelectionId, underMouseId,
                                                               targetLong, targetLat ); 

                    int animid = CreateAnimation( AnimationTypeActionMarker, m_currentSelectionId, targetLong.DoubleValue(), targetLat.DoubleValue() );
                    ActionMarker *action = (ActionMarker *) m_animations[animid];

                    if( !underMouse )
                    {
                        action->m_targetType = obj->IsValidMovementTarget( targetLong, targetLat );
                        action->m_combatTarget = false;
                    }
                    else
                    {
                        action->m_targetType = obj->IsValidCombatTarget( underMouseId );
                        action->m_combatTarget = ( action->m_targetType > WorldObject::TargetTypeInvalid );

                        if( action->m_targetType == WorldObject::TargetTypeInvalid )
                        {
                            if( obj->m_type == WorldObject::TypeCarrier || obj->m_type == WorldObject::TypeAirBase )
                            {
                                if( obj->m_currentState == 0 )
                                {
                                    // HACK: Airbase or Carrier, trying to launch a fighter, clicked on a city within range
                                    // Show valid movement target to launch fighter
                                    action->m_targetType = WorldObject::TargetTypeLaunchFighter;
                                    action->m_combatTarget = false;
                                }
                            }
                        }
                    }

                    if( action->m_targetType >= WorldObject::TargetTypeValid )
                    {
                        g_soundSystem->TriggerEvent( "Interface", "SetCombatTarget" );
                    }

                    if( !obj->IsActionQueueable() ||
                        numTimesRemaining - 1 <= 0 )
                    {
                        SetCurrentSelectionId(-1);
                    }

                    if( underMouse ) underMouse->m_nukeCountTimer = GetHighResTime() + 0.1f;
                }
            }            
        }
        else
        {
            SetCurrentSelectionId(-1);;
        }
    }
}


void MapRenderer::HandleSetWaypoint( float _mouseX, float _mouseY )
{
    MovingObject *obj = (MovingObject*)g_app->GetWorld()->GetWorldObject( m_currentSelectionId );

    if( obj && obj->IsMovingObject() )
    {
        if( obj->m_fleetId != -1 )
        {
            //
            // Moving a fleet

            Fixed mouseX = Fixed::FromDouble(_mouseX);
            Fixed mouseY = Fixed::FromDouble(_mouseY);

            Fleet *fleet = g_app->GetWorld()->GetTeam( obj->m_teamId )->GetFleet( obj->m_fleetId );

            if( fleet &&
                fleet->IsValidFleetPosition( mouseX, mouseY ) &&
                g_app->GetWorld()->GetClosestNode( mouseX, mouseY ) != -1 )
            {
                g_app->GetClientToServer()->RequestFleetMovement( obj->m_teamId, obj->m_fleetId,
																  Fixed::FromDouble(_mouseX),
																  Fixed::FromDouble(_mouseY) );

                for( int i = 0; i < fleet->m_fleetMembers.Size(); ++i )
                {
                    WorldObject *thisShip = g_app->GetWorld()->GetWorldObject( fleet->m_fleetMembers[i] );
                    if( thisShip )
                    {
                        Fixed offsetX = 0;
                        Fixed offsetY = 0;
                        fleet->GetFormationPosition( fleet->m_fleetMembers.Size(), i, &offsetX, &offsetY );
                        int index = CreateAnimation( AnimationTypeActionMarker, thisShip->m_objectId,
													 _mouseX+offsetX.DoubleValue(),
													 _mouseY+offsetY.DoubleValue() );  
                        ActionMarker *marker = (ActionMarker *)m_animations[index];
                        marker->m_targetType = WorldObject::TargetTypeValid;
                        g_soundSystem->TriggerEvent( "Interface", "SetMovementTarget" );
                    }
                }
                SetCurrentSelectionId(-1);
            }
            else
            {
                g_soundSystem->TriggerEvent( "Interface", "Error" );
            }
        }
        else
        {
            //
            // Moving a single object

            g_app->GetClientToServer()->RequestSetWaypoint( m_currentSelectionId,
															Fixed::FromDouble(_mouseX), Fixed::FromDouble(_mouseY) );
            int index = CreateAnimation( AnimationTypeActionMarker, m_currentSelectionId, _mouseX, _mouseY );  
            ActionMarker *marker = (ActionMarker *)m_animations[index];
            marker->m_targetType = WorldObject::TargetTypeValid;
            g_soundSystem->TriggerEvent( "Interface", "SetMovementTarget" );
            SetCurrentSelectionId(-1);
        }
    }

}


void MapRenderer::Update()
{
    if( g_keys[KEY_F1] && g_keyDeltas[KEY_F1] ) m_showFps = !m_showFps;

    UpdateMouseIdleTime();


    // Work out our mouse co-ordinates in long/lat space

    m_pixelX = 0;
    m_pixelY = 0;
    m_pixelW = g_windowManager->WindowW();
    m_pixelH = g_windowManager->WindowH();

    float longitude;
    float latitude;
    ConvertPixelsToAngle( g_inputManager->m_mouseX, g_inputManager->m_mouseY, &longitude, &latitude, true );

    //
    // Camera control

    UpdateCameraControl( longitude, latitude );

    
	//
	// WhiteBoard planning drawing

	if( UpdatePlanning( longitude, latitude ) ) return;


    //
    // Is the mouse over something?

    if( !IsMouseInMapRenderer() ) return;
    
    if( m_highlightTime > 0.0f )    m_highlightTime -= g_advanceTime*2.0f;        
    if( m_highlightTime < 0.0f )    m_highlightTime = 0.0f;
    
    if( m_selectTime > 0.0f )       m_selectTime  -= g_advanceTime;
    if( m_selectTime < 0.0f )       m_selectTime  = 0.0f;

    if( m_deselectTime < 1.0f )     m_deselectTime  += g_advanceTime;    
    if( m_deselectTime > 1.0f )     m_deselectTime = 1.0f;
        

    WorldObject *underMouse = NULL;
    int underMouseId = GetNearestObjectToMouse( longitude, latitude );
    if( underMouseId != -1 ) underMouse = g_app->GetWorld()->GetWorldObject( underMouseId );


#ifndef NON_PLAYABLE
    if( g_app->GetWorld()->GetTimeScaleFactor() > 0 && !m_lockCommands)
    {
        if( g_inputManager->m_lmbUnClicked )                                        // LEFT BUTTON UNCLICKED
        {
            if( m_currentStateId != -1 )                            
            {             
                HandleClickStateMenu();
            }
            else if( underMouse &&
                     underMouseId == m_currentHighlightId )                             
            {                
                HandleSelectObject(underMouseId);
            }
            else 
            {
                HandleObjectAction(longitude, latitude, underMouseId);
            }
        }

        if( g_inputManager->m_rmbUnClicked )                                        // RIGHT MOUSE UNCLICKED
        {
            if( m_currentSelectionId != -1 )
            {
                WorldObject *obj = g_app->GetWorld()->GetWorldObject(m_currentSelectionId);
                if( obj &&
                    obj->m_fleetId != -1 &&
                    underMouse &&
                    underMouse->m_fleetId == obj->m_fleetId )
                {
                    // Dont set waypoint if we just clicked on a ship in the same fleet
                    m_currentSelectionId = -1;
                }
                else
                {
                    HandleSetWaypoint( longitude, latitude );
                }
            }

            if( underMouseId != -1 )
            {
                WorldObject *obj = g_app->GetWorld()->GetWorldObject(underMouseId);
                if( obj &&
                    obj->m_teamId == g_app->GetWorld()->m_myTeamId &&
                    m_currentSelectionId == -1 )
                {
                    m_stateRenderTime = 10.0f;
                    m_stateObjectId = underMouseId;
                    g_soundSystem->TriggerEvent( "Interface", "HighlightObject" );
                }
            }
            else
            {
                m_stateRenderTime = 0.0f;
                m_stateObjectId = -1;
                m_currentHighlightId = -1;
            }           
        }
    }
#endif


    int oldHighlightId = m_currentHighlightId;
    m_currentHighlightId = -1;
    if( g_inputManager->m_lmbClicked )
    {
        if( underMouse &&
            underMouse->m_teamId == g_app->GetWorld()->m_myTeamId )                 // Mouse down on one of our own
        {
            m_currentHighlightId = underMouseId;
            m_highlightTime = 0.0f;
        }
    }
    else if( g_inputManager->m_lmbUnClicked )                                // Mouse up
    {
        m_currentHighlightId = -1;
        m_stateRenderTime = 0.0f;
        m_stateObjectId = -1;
    }
    else if( !g_inputManager->m_lmb )                                 // Mouse highlights something of ours
    {
        if( !underMouse ||
            underMouse->m_type == WorldObject::TypeCity ||
            underMouseId == -1 ||
            g_app->GetWorld()->m_myTeamId == -1 ||
            g_app->GetGame()->m_winner ||
            underMouse->m_teamId == g_app->GetWorld()->m_myTeamId ||
            underMouse->m_visible[g_app->GetWorld()->m_myTeamId] ||
            underMouse->m_lastSeenTime[g_app->GetWorld()->m_myTeamId] > 0 )
        {
            m_currentHighlightId = underMouseId;
            if( oldHighlightId != underMouseId )
            {
                m_highlightTime = 1.0f;
            }
        }
    }
    else if( g_inputManager->m_lmb && underMouseId != -1 )
    {
        m_currentHighlightId = oldHighlightId;
    }
             
    int oldStateId = m_currentStateId;
    m_currentStateId = -1;
    if( m_stateRenderTime > 0.0f )  
    {
        m_stateRenderTime -= g_advanceTime;
        m_highlightTime = 0.0f;
        m_currentHighlightId = m_stateObjectId;     
    }
    if( m_stateRenderTime <= 0.0f && m_stateObjectId != -1)
    {
        m_stateRenderTime = 0.0f;
        m_stateObjectId = -1;
    }

    if( m_stateRenderTime > 0.0f &&                                              // Player is choosing state changes
        m_currentHighlightId != -1 )
    {
    if( longitude < -180.0f ) longitude += 360.0f;
    if( longitude >180.0f ) longitude -= 360.0f;

        WorldObject *wobj = g_app->GetWorld()->GetWorldObject( m_currentHighlightId );
        if( wobj )
        {
            for( int i = 0; i < wobj->m_states.Size(); ++i )
            {
                float screenX, screenY, screenW, screenH;
                GetWorldObjectStatePosition( wobj, i, &screenX, &screenY, &screenW, &screenH );
                //if( screenX < -180 ) screenX += 360.0f;
                //if( screenX > 180 ) screenX -= 360.0f;
                if( longitude >= screenX && longitude <= screenX + screenW &&
                    latitude >= screenY && latitude <= screenY + screenH )
                {
                    m_currentStateId = i;
                    break;
                }
            }
            if( m_currentStateId == -1 )
            {
                if( wobj->IsActionQueueable() &&
                    wobj->m_actionQueue.Size() > 0 )
                {
                    float screenX, screenY, screenW, screenH;
                    GetWorldObjectStatePosition( wobj, -1, &screenX, &screenY, &screenW, &screenH );
                    if( longitude >= screenX && longitude <= screenX + screenW &&
                        latitude >= screenY && latitude <= screenY + screenH )
                    {
                        m_currentStateId = CLEARQUEUE_STATEID;
                    }
                }
            }
        }
    }

    if( g_keys[KEY_SPACE] )
    {
        SetCurrentSelectionId(-1);
        m_stateRenderTime = 0.0f;
    }

    if( m_currentStateId != -1 &&
        m_currentStateId != oldStateId )
    {
        g_soundSystem->TriggerEvent( "Interface", "HighlightObjectState" );
    }

    m_lockCommands = false;
}

void MapRenderer::ConvertPixelsToAngle( float pixelX, float pixelY, float *longitude, float *latitude, bool absoluteLongitude)
{
    float width = 360.0f * m_zoomFactor;
    float aspect = (float) g_windowManager->WindowH() / (float) g_windowManager->WindowW();
    float height = (360.0f * aspect) * m_zoomFactor;

    float left, right, top, bottom;
    GetWindowBounds( &left, &right, &top, &bottom );

    *longitude = width * (pixelX - m_pixelX) / m_pixelW - width/2 + m_middleX;
    *latitude = height * ((m_pixelY+m_pixelH)-pixelY) / m_pixelH - height/2 + m_middleY;

    // correct for zoom factor
    *longitude -= (1.0f - m_zoomFactor ) / 3.0f;
    *latitude  -= (1.0f - m_zoomFactor ) / 3.0f;

    if( absoluteLongitude == false )
    {
        if( *longitude < -180.0f ) *longitude += 360.0f;
        if( *longitude >180.0f ) *longitude -= 360.0f;
    }
}


void MapRenderer::ConvertAngleToPixels( float longitude, float latitude, float *pixelX, float *pixelY )
{
    float left, right, top, bottom;
    GetWindowBounds( &left, &right, &top, &bottom );
    
    if( right > 180 )
    {
        right -= 360;
        left -= 360;
    }
    

    if( left < -180 && longitude > 0 )
    {
        left += 360;
        right += 360;
    }

    float fractionalLongitude = (longitude-left) / (right-left);
    float fractionalLatitude = (latitude-top) / (bottom-top);
    
    *pixelX = fractionalLongitude * (float)g_windowManager->WindowW();
    *pixelY = fractionalLatitude * (float)g_windowManager->WindowH();
}


void MapRenderer::GetWindowBounds( float *left, float *right, float *top, float *bottom )
{
    float width = 360.0f * m_zoomFactor;    
    float aspect = (float) g_windowManager->WindowH() / (float) g_windowManager->WindowW();
    float height = (360.0f * aspect) * m_zoomFactor;

    *left = m_middleX - width/2.0f;
    *top = m_middleY + height/2.0f;
    *right = *left+width;
    *bottom = *top-height;
}

bool MapRenderer::IsValidTerritory( int teamId, Fixed longitude, Fixed latitude, bool seaUnit )
{    
    if( latitude > 100 || latitude < -100 )
    {
        return false;
    }

    if( longitude < -180 )
    {
        longitude = -180;
    }
    else if( longitude > 180 )
    {
        longitude = 179;
    }    
    if( teamId == -1 )
    {
        int pixelX = ( bmpSailableWater->Width() * (longitude+180)/360 ).IntValue();
        int pixelY = ( bmpSailableWater->Height() * (latitude+100)/200 ).IntValue();
        Colour theCol = bmpSailableWater->GetColour( pixelX, pixelY );

        return ( theCol.m_r > 20 && 
                 theCol.m_g > 20 &&
                 theCol.m_b > 20 );
    }
    else
    {
        bool isValid = false;
        Team *team = g_app->GetWorld()->GetTeam(teamId);
        for( int i = 0; i < team->m_territories.Size(); ++i )
        {
            Image *img = m_territories[team->m_territories[i]];            
            AppDebugAssert( img );

            int pixelX = ( img->Width() * (longitude+180)/360 ).IntValue();
            int pixelY = ( img->Height() * (latitude+100)/200 ).IntValue();

            Colour theCol = img->GetColour( pixelX, pixelY );
            Colour sailableCol = bmpSailableWater->GetColour( pixelX, pixelY );
            int landthreshold = 130;
            int waterthreshold = 60;
            if( !seaUnit )
            {
                if ( theCol.m_r > landthreshold && 
                     theCol.m_g > landthreshold &&
                     theCol.m_b > landthreshold &&
                     sailableCol.m_r <= waterthreshold && 
                     sailableCol.m_g <= waterthreshold &&
                     sailableCol.m_b <= waterthreshold )
                {
                    isValid = true;
                }
            }
            else
            {
                if ( theCol.m_r > waterthreshold && 
                     theCol.m_g > waterthreshold &&
                     theCol.m_b > waterthreshold &&
                     sailableCol.m_r > waterthreshold && 
                     sailableCol.m_g > waterthreshold &&
                     sailableCol.m_b > waterthreshold )
                {
                    isValid = true;
                }

            }
        }
        return isValid;
    }
    return false;
}

bool MapRenderer::IsCoastline(Fixed longitude, Fixed latitude )
{
    Image *img = g_resource->GetImage( "earth/coastlines.bmp" );
    int pixelX = ( img->Width() * (longitude+180)/360 ).IntValue();
    int pixelY = ( img->Height() * (latitude+100)/200 ).IntValue();

    Colour theCol = img->GetColour( pixelX, pixelY );

    return ( theCol.m_r > 20 &&
             theCol.m_g > 20 &&
             theCol.m_b > 20 );
}

int MapRenderer::GetTerritory( Fixed longitude, Fixed latitide, bool seaArea )
{
    for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
    {
        Team *team = g_app->GetWorld()->m_teams[i];
        if( IsValidTerritory( team->m_teamId, longitude, latitide, seaArea ) )
        {
            return team->m_teamId;
        }
    }
    return -1;
}

float MapRenderer::GetZoomFactor()
{
    return m_zoomFactor;
}

float MapRenderer::GetDrawScale()
{
    return m_drawScale;
}


void MapRenderer::GetPosition( float &_middleX, float &_middleY )
{
    _middleX = m_middleX;
    _middleY = m_middleY;
}


int MapRenderer::GetLongitudeMod()
{
    if( m_middleX < 0 )
    {
        return 360;
    }
    else
    {
        return -360;
    }
}

Image *MapRenderer::GetTerritoryImage( int territoryId )
{
    return m_territories[territoryId];
}

int MapRenderer::GetCurrentSelectionId()
{
    return m_currentSelectionId; 
}

int MapRenderer::GetCurrentHighlightId()
{
    return m_currentHighlightId;
}

void MapRenderer::SetCurrentSelectionId( int id )
{
    if( m_currentSelectionId != -1 && id == -1 )
    {
        g_soundSystem->TriggerEvent( "Interface", "DeselectObject" );
    }
    else if( id != -1 )
    {
        g_soundSystem->TriggerEvent( "Interface", "SelectObject" );
    }

    if( m_currentSelectionId != -1 )
    {
        m_previousSelectionId = m_currentSelectionId;
    }
    m_currentSelectionId = id;
    m_selectTime = 1.0f;
    if( id == -1 )
    {
        m_deselectTime = 0.0f; 
    }
}

int MapRenderer::GetTerritoryId( Fixed longitude, Fixed latitude )
{
    for( int i = 0; i < World::NumTerritories; ++i )
    {
        Image *img = m_territories[i];
        
        AppDebugAssert( img );

        int pixelX = ( img->Width() * (longitude+180)/360 ).IntValue();
        int pixelY = ( img->Height() * (latitude+100)/200 ).IntValue();

        Colour theCol = img->GetColour( pixelX, pixelY );
        if ( theCol.m_r > 20 && 
             theCol.m_g > 20 &&
             theCol.m_b > 20 )
        {
            return i;
        }
    }
    return -1;
}

int MapRenderer::GetTerritoryIdUnique( Fixed longitude, Fixed latitude )
{
    int prevTerritory = -1;
    for( int i = 0; i < World::NumTerritories; ++i )
    {
        Image *img = m_territories[i];
        
        AppDebugAssert( img );

        int pixelX = ( img->Width() * (longitude+180)/360 ).IntValue();
        int pixelY = ( img->Height() * (latitude+100)/200 ).IntValue();

        Colour theCol = img->GetColour( pixelX, pixelY );
        if ( theCol.m_r > 20 && 
             theCol.m_g > 20 &&
             theCol.m_b > 20 )
        {
            if( prevTerritory != -1 )
            {
                return -1;
            }
            prevTerritory = i;
        }
    }
    return prevTerritory;
}

void MapRenderer::CenterViewport( float longitude, float latitude, int zoom, int camSpeed )
{
    m_cameraLongitude = longitude;
    m_cameraLatitude = latitude;

    float longDist = g_app->GetWorld()->GetDistance( Fixed::FromDouble(m_middleX), Fixed(0), Fixed::FromDouble(m_cameraLongitude), Fixed(0) ).DoubleValue();
    float latDist = g_app->GetWorld()->GetDistance( Fixed(0), Fixed::FromDouble(m_middleY), Fixed(0), Fixed::FromDouble(m_cameraLatitude) ).DoubleValue();

    m_speedRatioX = (longDist/latDist);
    if( m_speedRatioX < 0 )
    {
        m_speedRatioX = 1.0f;
    }
    m_speedRatioY = 1.0f;
  
    m_cameraZoom = zoom;
    m_camSpeed = camSpeed;

    m_camAtTarget = false;
}

void MapRenderer::CenterViewport( int objectId, int zoom, int camSpeed )
{
    WorldObject *obj = g_app->GetWorld()->GetWorldObject( objectId );
    if( obj )
    {
        CenterViewport( obj->m_longitude.DoubleValue(), obj->m_latitude.DoubleValue(), zoom, camSpeed );
    }
}

void MapRenderer::CameraCut( float longitude, float latitude, int zoom )
{
    m_middleX = longitude;
    m_middleY = latitude;
    m_totalZoom = zoom;
    m_cameraZoom = zoom;
}

int MapRenderer::CreateAnimation( int animationType, int _fromObjectId, float longitude, float latitude )
{
    AnimatedIcon *anim = NULL;
    switch( animationType )
    {
        case AnimationTypeActionMarker  : anim = new ActionMarker();    break;
        case AnimationTypeSonarPing     : anim = new SonarPing();       break; 
        case AnimationTypeNukePointer   : anim = new NukePointer();     break;

        default: return -1;
    }

    anim->m_longitude = longitude;
    anim->m_latitude = latitude;
    anim->m_fromObjectId = _fromObjectId;
    anim->m_animationType = animationType;
    
    int index = m_animations.PutData( anim );
    return index;
}
    

void MapRenderer::FindScreenEdge( Vector3<float> const &_line, float *_posX, float *_posY )
{
//    y = mx + c
//    c = y - mx
//    x = (y - c) / m
 

    float left, right, top, bottom;
    g_app->GetMapRenderer()->GetWindowBounds( &left, &right, &top, &bottom );


    // shrink screen slightly so arrow renders fully onscreen

    top -= 2.5f * m_drawScale;
    right -= 2.5f * m_drawScale;
    bottom += 2.5f * m_drawScale;
    left += 2.5f * m_drawScale;

    float m = (_line.y - m_middleY) / (_line.x - m_middleX);
    float c = m_middleY - m * m_middleX;
    
    if( _line.y > top )
    {
        // Intersect with top view plane
        float x = ( top - c ) / m;
        if( x >= left && x <= right )
        {
            *_posX = x;
            *_posY = top;
            return;
        }
    }
    else
    {
        // Intersect with the bottom view plane
        float x = ( bottom - c ) / m;
        if( x >= left && x <= right )
        {
            *_posX = x;
            *_posY = bottom;
            return;
        }        
    }
 
    if( _line.x < left )
    {
        // Intersect with left view plane 
        float y = m * left + c;
        if( y >= bottom && y <= top ) 
        {
            *_posX = left;
            *_posY = y;
            return;
        }
    }
    else
    {
        // Intersect with right view plane
        float y = m * right + c;
        if( y >= bottom && y <= top ) 
        {
            *_posX = right;
            *_posY = y;
            return;
        }        
    }
 
    // We should never ever get here
    AppAbort( "We should never ever get here" );
}

void MapRenderer::UpdateMouseIdleTime()
{
    if( m_draggingCamera ) return;

    if( m_oldMouseX == g_inputManager->m_mouseX &&
        m_oldMouseY == g_inputManager->m_mouseY )
    {
        m_mouseIdleTime += SERVER_ADVANCE_PERIOD.DoubleValue();
    }
    else
    {
        m_oldMouseX = g_inputManager->m_mouseX;
        m_oldMouseY = g_inputManager->m_mouseY;
        m_mouseIdleTime = 0.0f;
    }
}

void MapRenderer::LockRadarRenderer()
{
    m_radarLocked = true;
    m_showRadar = false;
}

void MapRenderer::UnlockRadarRenderer()
{
    m_radarLocked = false;
}

void MapRenderer::AutoCam()
{
    // automatically moves the camera to areas of interest every 10 seconds, or zooms out if nothing is happening
    m_autoCamTimer -= SERVER_ADVANCE_PERIOD.DoubleValue();
    if( m_autoCam && 
        (m_camAtTarget || m_autoCamTimer <= 0.0f) )
    {
        if( m_autoCamTimer <= 0.0f )
        {
            m_autoCamTimer = 20.0f;
            LList<Vector3<Fixed > *> areasOfInterest; // store longitude/latitude in x/y, and scene 'score' in z
        
            if( g_app->GetWorld()->m_gunfire.Size() > 0 )
            {
                for( int i = 0; i < g_app->GetWorld()->m_gunfire.Size(); ++i )
                {
                    if( g_app->GetWorld()->m_gunfire.ValidIndex(i) )
                    {
                        GunFire *gunfire = g_app->GetWorld()->m_gunfire[i];

                        if( gunfire->m_range > 5 ) // gf isnt about to expire
                        {
                            areasOfInterest.PutData( new Vector3<Fixed>(gunfire->m_longitude, gunfire->m_latitude, 1 ) );
                        }                    
                    }
                }
            }

            if( g_app->GetWorld()->m_explosions.Size() > 0 )
            {
                for( int i = 0; i < g_app->GetWorld()->m_explosions.Size(); ++i )
                {
                    if( g_app->GetWorld()->m_explosions.ValidIndex(i) )
                    {
                        Explosion *explosion = g_app->GetWorld()->m_explosions[i];

                        if( explosion->m_intensity >= 25 &&
                            explosion->m_intensity < 30 )
                        {
                            areasOfInterest.PutData( new Vector3<Fixed>( explosion->m_longitude, explosion->m_latitude, 1 ) );
                        }

                        if( explosion->m_intensity > 90 )
                        {
                            areasOfInterest.PutData( new Vector3<Fixed>( explosion->m_longitude, explosion->m_latitude, 10 ) );
                        }
                    }
                }
            }

            for( int i = 0; i < g_app->GetWorld()->m_messages.Size(); ++i )
            {
                if( g_app->GetWorld()->m_messages[i]->m_messageType == WorldMessage::TypeLaunch ||
                    g_app->GetWorld()->m_messages[i]->m_messageType == WorldMessage::TypeDirectHit )
                {
                    areasOfInterest.PutData( new Vector3<Fixed>( g_app->GetWorld()->m_messages[i]->m_longitude, g_app->GetWorld()->m_messages[i]->m_latitude, 20 ) );
                }

            }

            for( int i = 0; i < m_animations.Size(); ++i )
            {
                if( m_animations.ValidIndex(i) )
                {
                    if( m_animations[i]->m_animationType == AnimationTypeNukePointer )
                    {
                        areasOfInterest.PutData( new Vector3<Fixed>( Fixed::FromDouble(m_animations[i]->m_longitude),
																	 Fixed::FromDouble(m_animations[i]->m_latitude),
																	 Fixed::FromDouble(15 + 5 * sfrand()) ) );
                    }
                }
            }

            for( int i = 0; i < areasOfInterest.Size(); ++i )
            {
                for( int j = 0; j < areasOfInterest.Size(); ++j )
                {
                    if( i != j )
                    {
                        if( g_app->GetWorld()->GetDistance( areasOfInterest[i]->x, areasOfInterest[i]->y, areasOfInterest[j]->x, areasOfInterest[j]->y ) <= 30 )
                        {
                            Vector3<Fixed> *vec = areasOfInterest[j];
                            delete vec;
                            areasOfInterest[i]->z += 1;
                            areasOfInterest.RemoveData(j);
                            j--;
                        }
                    }
                }
            }

            for( int i = 0; i < areasOfInterest.Size(); ++i )
            {
                if( areasOfInterest[i]->z < 2 )
                {
                    Vector3<Fixed> *vec = areasOfInterest[i];
                    delete vec;
                    areasOfInterest.RemoveData(i);
                    i--;
                }
            }


            if( areasOfInterest.Size() == 0 )
            {
                m_camAtTarget = true;
                CameraCut(0.0f, 0.0f, 0 );
                return;
            }

            m_autoCamCounter = 0;

            BoundedArray<int> topThree;
            BoundedArray<int> score;
            topThree.Initialise(3);
            score.Initialise(3);

            for( int i = 0; i < 3; ++i )
            {
                topThree[i] = -1;
                score[i] = 0;
            }
            int totalScore = 0;
            for( int i = 0; i < areasOfInterest.Size(); ++i )
            {
                totalScore += areasOfInterest[i]->z.IntValue();
                if( areasOfInterest[i]->z > score[0] )
                {
                    topThree[2] = topThree[1];
                    score[2] = score[1];

                    topThree[1] = topThree[0];
                    score[1] = score[0];

                    topThree[0] = i;
                    score[0] = areasOfInterest[i]->z.IntValue();
                }
                else if( areasOfInterest[i]->z > score[1] )
                {
                    topThree[2] = topThree[1];
                    score[2] = score[1];

                    topThree[1] = i;
                    score[1] = areasOfInterest[i]->z.IntValue();
                }
                else if( areasOfInterest[i]->z > score[2] )
                {
                    topThree[2] = i;
                    score[2] = areasOfInterest[i]->z.IntValue();
                }
            }

            int r = 0;
            if( areasOfInterest.Size() >= 9 )
            {
                int r = rand() % 3;
            }

            if( g_app->GetWorld()->GetDistance( Fixed::FromDouble(m_middleX), Fixed::FromDouble(m_middleY), areasOfInterest[ topThree[r] ]->x, areasOfInterest[ topThree[r] ]->y ) > 15 )
            {
                int zoom = ( (areasOfInterest[ topThree[r] ]->z / totalScore) * 20 ).IntValue();
                Clamp( zoom, 0, 20 );

                int camSpeed = g_app->GetWorld()->GetDistance( Fixed::FromDouble(m_middleX), Fixed::FromDouble(m_middleY), areasOfInterest[ topThree[r] ]->x,
															   areasOfInterest[ topThree[r] ]->y ).DoubleValue() / 180 * 70;
                camSpeed = max(20, camSpeed );

                if( camSpeed > 50 )
                {
                    CameraCut( areasOfInterest[ topThree[r] ]->x.DoubleValue(),
							   areasOfInterest[ topThree[r] ]->y.DoubleValue(), zoom );
                }
                else
                {
                    CenterViewport( areasOfInterest[ topThree[r] ]->x.DoubleValue(),
									areasOfInterest[ topThree[r] ]->y.DoubleValue(), zoom, camSpeed );
                }
            }
        }
    }
}

void MapRenderer::ToggleAutoCam()
{
    m_autoCam = !m_autoCam;
    if( m_autoCam )
    {
        m_lockCamControl = true;
    }
    else
    {
        m_lockCamControl = false;
    }
    m_cameraLongitude = 0.0f;
    m_cameraLatitude = 0.0f;
    m_cameraZoom = -1;
}


bool MapRenderer::GetAutoCam()
{
    return m_autoCam;
}

void MapRenderer::MoveCam()
{
    if(( m_cameraLongitude || m_cameraLatitude ) &&
        m_cameraIdleTime >= 1.0f )
    {
        m_lockCamControl = true;

        //m_totalZoom = m_cameraZoom;
        float scrollSpeed = m_camSpeed * m_zoomFactor * g_advanceTime;
        if( m_cameraLongitude )
        {
            if( m_middleX > m_cameraLongitude )
            {
                if( m_middleX - (scrollSpeed  * m_speedRatioX) < m_cameraLongitude )
                {
                    m_middleX = m_cameraLongitude;
                    m_cameraLongitude = 0.0f;
                    m_speedRatioX = 0.0f;
                }
                else
                {
                    m_middleX -= scrollSpeed * m_speedRatioX;
                }
            }
            else
            {
                if( m_middleX + (scrollSpeed  * m_speedRatioX)> m_cameraLongitude )
                {
                    m_middleX = m_cameraLongitude;
                    m_cameraLongitude = 0.0f;
                    m_speedRatioX = 0.0f;
                }
                else
                {
                    m_middleX += scrollSpeed * m_speedRatioX;
                }
            }
        }

        if( m_cameraLatitude )
        {
            if( m_middleY > m_cameraLatitude )
            {
                if( m_middleY - (scrollSpeed * m_speedRatioY )< m_cameraLatitude )
                {
                    m_middleY = m_cameraLatitude;
                    m_cameraLatitude = 0.0f;
                    m_speedRatioY = 0.0f;
                }
                else
                {
                    m_middleY -= scrollSpeed * m_speedRatioY;
                }
            }
            else
            {
                if( m_middleY + (scrollSpeed * m_speedRatioY) > m_cameraLatitude )
                {
                    m_middleY = m_cameraLatitude;
                    m_cameraLatitude = 0.0f;
                    m_speedRatioY = 0.0f;
                }
                else
                {
                    m_middleY += scrollSpeed * m_speedRatioY;
                }
            }
        }

        if( !m_cameraLongitude && !m_cameraLongitude )
        {
            m_camAtTarget = true;
            m_lockCamControl = false;
        }
        
    }

    if( m_camAtTarget && m_cameraZoom != -1 )
    {
        if( m_totalZoom != m_cameraZoom )
        {
            m_totalZoom = m_cameraZoom;
        }
        else
        {
            m_cameraZoom = -1;
        }
    }
}


bool MapRenderer::IsMouseInMapRenderer()
{    
    EclWindow *window = EclGetWindow();

    if( !window ) return true;

    if( stricmp(window->m_name, "comms window") == 0 &&
        stricmp(EclGetCurrentClickedButton(), "none") == 0 )
    {
        return true;
    }

    return false;
}


void MapRenderer::DragCamera()
{
    if( g_inputManager->m_mmb )
    {
        float left, right, top, bottom;
        GetWindowBounds( &left, &right, &top, &bottom );

        g_app->SetMousePointerVisible ( false );

        m_middleX -= g_inputManager->m_mouseVelX * m_drawScale * 0.2f;
        m_middleY += g_inputManager->m_mouseVelY * m_drawScale * 0.2f;
        m_draggingCamera = true;
        m_lockCommands = true;
    }
    else if( g_inputManager->m_mmbUnClicked && m_draggingCamera )
    {
        m_lockCommands = true;
    }
    else
    {
        if( !g_app->MousePointerIsVisible() )
        {
            g_app->SetMousePointerVisible ( true );
        }
        m_draggingCamera = false;
    }
}

bool MapRenderer::IsOnScreen( float _longitude, float _latitude, float _expandScreen )
{
    float left, right, top, bottom;
    GetWindowBounds( &left, &right, &top, &bottom );

    if( _expandScreen > 0.0f )
    {
        left -= 2.0f;
        right += 2.0f;
        top += 2.0f;
        bottom -= 2.0f;
    }

    if( _longitude > left && _longitude < right &&
        _latitude > bottom && _latitude < top )
    {
        return true;
    }

    if( left < -180.0f )
    {
        if(((_longitude > left+360.0f && _longitude < 180.0f) ||
            (_longitude < right )) &&
            _latitude > bottom && _latitude < top )
        {
            return true;
        }
    }

    if( right > 180.0f )
    {
        if(((_longitude < right-360.0f && _longitude > -180.0f) ||
            (_longitude > left )) &&
            _latitude > bottom && _latitude < top )
        {
            return true;
        }
    }
    return false;
}

bool MapRenderer::GetShowWhiteBoard() const
{
	return m_showWhiteBoard;
}

void MapRenderer::SetShowWhiteBoard( bool showWhiteBoard )
{
	m_showWhiteBoard = showWhiteBoard;
}

bool MapRenderer::GetEditWhiteBoard() const
{
	return m_editWhiteBoard;
}

void MapRenderer::SetEditWhiteBoard( bool editWhiteBoard )
{
	if ( m_editWhiteBoard != editWhiteBoard )
	{
		m_editWhiteBoard = editWhiteBoard;
		if ( m_editWhiteBoard )
		{
			if ( m_showPlanning )
			{
				g_app->GetInterface()->SetMouseCursor( "gui/pen.bmp" );
			}
		}
		else
		{
			g_app->GetInterface()->SetMouseCursor();
		}
		m_drawingPlanning = false;
		m_erasingPlanning = false;
	}
}

bool MapRenderer::GetShowPlanning() const
{
	return m_showPlanning;
}

void MapRenderer::SetShowPlanning( bool showPlanning )
{
	if ( m_showPlanning != showPlanning )
	{
		m_showPlanning = showPlanning;
		if ( m_showPlanning )
		{
			g_app->GetInterface()->SetMouseCursor( "gui/pen.bmp" );
		}
		else
		{
			g_app->GetInterface()->SetMouseCursor();
		}
		m_drawingPlanning = false;
		m_erasingPlanning = false;
	}
}

bool MapRenderer::GetShowAllWhiteBoards() const
{
	return m_showAllWhiteBoards;
}

void MapRenderer::SetShowAllWhiteBoards( bool showAllWhiteBoards )
{
	m_showAllWhiteBoards = showAllWhiteBoards;
}

static float GetDistancePoint( float longitude1, float latitude1, float longitude2, float latitude2 )
{
	float distdx = 0.0f;
	float distdy = 0.0f;

	if ( -90.0f >= longitude1 && longitude2 >= 90.0f )
	{
		float dx1 = -180.0f - longitude1;
		float dx2 = longitude2 - 180.0f;
		float dx = dx1 + dx2;
		float dy = latitude2 - latitude1;
		float midLatitude = latitude1;
		if ( dx != 0.0f )
			midLatitude += dy * ( dx1 / dx );
		distdx = -180.0f - longitude1;
		distdy = midLatitude - latitude1;
		distdx += longitude2 - 180.0f;
		distdy += latitude2 - midLatitude;
	}
	else if ( 90.0f <= longitude1 && longitude2 <= -90.0f )
	{
		float dx1 = 180.0f - longitude1;
		float dx2 = longitude2 - -180.0f;
		float dx = dx1 + dx2;
		float dy = latitude2 - latitude1;
		float midLatitude = latitude1;
		if ( dx != 0.0f )
			midLatitude += dy * ( dx1 / dx );
		distdx = 180.0f - longitude1;
		distdy = midLatitude - latitude1;
		distdx += longitude2 - -180.0f;
		distdy += latitude2 - midLatitude;
	}
	else
	{
		distdx = longitude2 - longitude1;
		distdy = latitude2 - latitude1;
	}

	return sqrt( distdx * distdx + distdy * distdy );
}

bool MapRenderer::UpdatePlanning( float longitude, float latitude )
{
	if( !m_editWhiteBoard || !m_showPlanning )
	{
		m_drawingPlanning = false;
		m_erasingPlanning = false;
		return false;
	}

    if( GetShowPlanning() && g_keys[KEY_SPACE] )
    {
		m_drawingPlanning = false;
		m_erasingPlanning = false;
		SetShowPlanning( false );
		return false;
    }

	Team *myTeam = g_app->GetWorld()->GetMyTeam();
	if ( !myTeam )
	{
		m_drawingPlanning = false;
		m_erasingPlanning = false;
		return false;
	}

	if ( !IsMouseInMapRenderer() )
	{
		m_drawingPlanning = false;
		m_erasingPlanning = false;
		return true;
	}

	WhiteBoard *whiteBoard = &g_app->GetWorld()->m_whiteBoards[ myTeam->m_teamId ];

	if( longitude < -180.0f ) longitude += 360.0f;
    if( longitude > 180.0f ) longitude -= 360.0f;

	// Drawing part
	if( g_inputManager->m_lmbClicked || ( g_inputManager->m_lmb && !m_drawingPlanning ) )
	{
		m_erasingPlanning = false;
		m_drawingPlanning = true;
		m_drawingPlanningTime = 0.0f;
		whiteBoard->RequestStartLine( longitude, latitude );
		m_longitudePlanningOld = longitude;
		m_latitudePlanningOld = latitude;
	}
	else if( g_inputManager->m_lmb || ( g_inputManager->m_lmbUnClicked && m_drawingPlanning ) )
	{
		m_erasingPlanning = false;
		m_drawingPlanningTime += g_advanceTime;
		if ( m_drawingPlanningTime >= 0.03333f || g_inputManager->m_lmbUnClicked || 
		     ( m_drawingPlanningTime >= 0.01667f && GetDistancePoint ( m_longitudePlanningOld, m_latitudePlanningOld, longitude, latitude ) >= WhiteBoard::LINE_LENGTH_MAX ) )
		{
			m_drawingPlanningTime = 0.0f;
			if ( m_longitudePlanningOld != longitude || m_latitudePlanningOld != latitude )
			{
				if ( -90.0f >= m_longitudePlanningOld && longitude >= 90.0f )
				{
					float dx1 = -180.0f - m_longitudePlanningOld;
					float dx2 = longitude - 180.0f;
					float dx = dx1 + dx2;
					float dy = latitude - m_latitudePlanningOld;
					float midLatitude = m_latitudePlanningOld;
					if ( dx != 0.0f )
						midLatitude += dy * ( dx1 / dx );
					whiteBoard->RequestAddLinePoint( m_longitudePlanningOld, m_latitudePlanningOld, -180.0f, midLatitude );
					whiteBoard->RequestStartLine( 180.0f, midLatitude );
					whiteBoard->RequestAddLinePoint ( 180.0f, midLatitude, longitude, latitude );
				}
				else if ( 90.0f <= m_longitudePlanningOld && longitude <= -90.0f )
				{
					float dx1 = 180.0f - m_longitudePlanningOld;
					float dx2 = longitude - -180.0f;
					float dx = dx1 + dx2;
					float dy = latitude - m_latitudePlanningOld;
					float midLatitude = m_latitudePlanningOld;
					if ( dx != 0.0f )
						midLatitude += dy * ( dx1 / dx );
					whiteBoard->RequestAddLinePoint( m_longitudePlanningOld, m_latitudePlanningOld, 180.0f, midLatitude );
					whiteBoard->RequestStartLine( -180.0f, midLatitude );
					whiteBoard->RequestAddLinePoint ( -180.0f, midLatitude, longitude, latitude );
				}
				else
				{
					whiteBoard->RequestAddLinePoint ( m_longitudePlanningOld, m_latitudePlanningOld, longitude, latitude );
				}

				m_longitudePlanningOld = longitude;
				m_latitudePlanningOld = latitude;
			}
		}

		if ( g_inputManager->m_lmbUnClicked )
		{
			m_drawingPlanning = false;
		}
	}
	// Erasing part
	else if( g_inputManager->m_rmbClicked || ( g_inputManager->m_rmb && !m_erasingPlanning ) )
	{
		m_drawingPlanning = false;
		m_erasingPlanning = true;
		m_drawingPlanningTime = 0.0f;
		m_longitudePlanningOld = longitude;
		m_latitudePlanningOld = latitude;
	}
	else if( g_inputManager->m_rmb || ( g_inputManager->m_rmbUnClicked && m_erasingPlanning ) )
	{
		m_drawingPlanning = false;
		m_drawingPlanningTime += g_advanceTime;
		if ( m_drawingPlanningTime >= 0.03333f || g_inputManager->m_rmbUnClicked )
		{
			m_drawingPlanningTime = 0.0f;
			if ( -90.0f >= m_longitudePlanningOld && longitude >= 90.0f )
			{
				float dx1 = -180.0f - m_longitudePlanningOld;
				float dx2 = longitude - 180.0f;
				float dx = dx1 + dx2;
				float dy = latitude - m_latitudePlanningOld;
				float midLatitude = m_latitudePlanningOld;
				if ( dx != 0.0f )
					midLatitude += dy * ( dx1 / dx );
				whiteBoard->RequestErase( m_longitudePlanningOld, m_latitudePlanningOld, -180.0f, midLatitude );
				whiteBoard->RequestErase( 180.0f, midLatitude, longitude, latitude );
			}
			else if ( 90.0f <= m_longitudePlanningOld && longitude <= -90.0f )
			{
				float dx1 = 180.0f - m_longitudePlanningOld;
				float dx2 = longitude - -180.0f;
				float dx = dx1 + dx2;
				float dy = latitude - m_latitudePlanningOld;
				float midLatitude = m_latitudePlanningOld;
				if ( dx != 0.0f )
					midLatitude += dy * ( dx1 / dx );
				whiteBoard->RequestErase( m_longitudePlanningOld, m_latitudePlanningOld, 180.0f, midLatitude );
				whiteBoard->RequestErase( -180.0f, midLatitude, longitude, latitude );
			}
			else
			{
				whiteBoard->RequestErase ( m_longitudePlanningOld, m_latitudePlanningOld, longitude, latitude );
			}

			m_longitudePlanningOld = longitude;
			m_latitudePlanningOld = latitude;
		}

		if ( g_inputManager->m_rmbUnClicked )
		{
			m_erasingPlanning = false;
		}
	}
	else
	{
		m_drawingPlanning = false;
		m_erasingPlanning = false;
	}

	return true;
}

void MapRenderer::RenderWhiteBoard()
{
	if ( !m_showWhiteBoard && !m_editWhiteBoard ) 
	{
		return;
	}

	Team *myTeam = g_app->GetWorld()->GetMyTeam();
	if( !myTeam )
	{
		return;
	}

	START_PROFILE( "WhiteBoard" );

	int sizeteams = g_app->GetWorld()->m_teams.Size();
    for( int i = 0; i < sizeteams; ++i )
    {
        Team *team = g_app->GetWorld()->m_teams[ i ];

		if ( ( m_showAllWhiteBoards && g_app->GetWorld()->IsFriend ( myTeam->m_teamId, team->m_teamId ) ) || 
		     myTeam->m_teamId == team->m_teamId )
		{
			WhiteBoard *whiteBoard = &g_app->GetWorld()->m_whiteBoards[ team->m_teamId ];
			char whiteboardname[32];
			snprintf( whiteboardname, sizeof(whiteboardname), "WhiteBoard%d", team->m_teamId );

			Colour colourBoard = team->GetTeamColour();
			glColor4ub( colourBoard.m_r, colourBoard.m_g, colourBoard.m_b, colourBoard.m_a );

			glLineWidth( 3.0f );

			unsigned int displayListId = 0;
			bool generated = g_resource->GetDisplayList( whiteboardname, displayListId );

			bool whiteBoardChanges = whiteBoard->HasChanges();
			if ( whiteBoardChanges )
			{
				if ( !generated )
				{
					glDeleteLists ( displayListId, 1 );
					g_resource->DeleteDisplayList( whiteboardname );
				}
				whiteBoard->ClearChanges();
			}
		    
			if( whiteBoardChanges || generated )
			{
				glNewList( displayListId, GL_COMPILE );

				const LList<WhiteBoardPoint *> *points = whiteBoard->GetListPoints();
				int sizePoints = points->Size();

				if ( sizePoints > 0 )
				{
					for ( int i = 0; i < sizePoints; i++ )
					{
						WhiteBoardPoint *pt = points->GetData( i );

						if ( i == 0 )
						{
							glBegin( GL_LINE_STRIP );
						}
						else if ( pt->m_startPoint )
						{
							glEnd();
							glBegin( GL_LINE_STRIP );
						}

						glVertex2f( pt->m_longitude, pt->m_latitude );
					}
					glEnd();
				}

				glEndList();
			}

			glCallList(displayListId);
		}
	}

	END_PROFILE( "WhiteBoard" );
}
