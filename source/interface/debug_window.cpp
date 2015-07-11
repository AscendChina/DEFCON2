#include "lib/universal_include.h"

#include <float.h>

#include "lib/render/renderer.h"
#include "lib/netlib/net_lib.h"

#include "lib/metaserver/metaserver.h"
#include "lib/metaserver/metaserver_defines.h"

#include "app/app.h"
#include "app/globals.h"

#include "renderer/map_renderer.h"

#include "network/Server.h"

#include "interface/components/core.h"
#include "interface/network_window.h"
#include "interface/profile_window.h"
#include "interface/soundsystem/soundeditor_window.h"
#include "interface/soundsystem/soundstats_window.h"
#include "interface/debug_window.h"
#include "interface/tutorial_window.h"
#include "interface/styleeditor_window.h"
#include "interface/connecting_window.h"
#include "interface/resynchronise_window.h"


class NetworkButton : public InterfaceButton
{
#ifdef TOOLS_ENABLED
    void MouseUp()
    {
        if( !EclGetWindow( "Network" ) )
        {
            NetworkWindow *window = new NetworkWindow( "Network", "dialog_network_title", true );
            EclRegisterWindow( window, m_parent );
        }
        else
        {
            EclBringWindowToFront( "Network" );
        }
    }
#endif
};

class ProfileButton : public InterfaceButton
{
#ifdef TOOLS_ENABLED
    void MouseUp()
    {
        if( !EclGetWindow( "Profile" ) )
        {
            ProfileWindow *profile = new ProfileWindow( "Profile", "dialog_profile_title", true );
            EclRegisterWindow( profile, m_parent );
        }
        else
        {
            EclBringWindowToFront( "Profile" );
        }
    }
#endif
};

class SoundStatsButton : public InterfaceButton
{
#ifdef TOOLS_ENABLED
    void MouseUp()
    {
        EclRegisterWindow( new SoundStatsWindow() );
    }
#endif
};


class SoundEditorButton : public InterfaceButton
{
#ifdef TOOLS_ENABLED
    void MouseUp()
    {
        EclRegisterWindow( new SoundEditorWindow() );
    }
#endif
};



class TutorialButton : public InterfaceButton
{
    void MouseUp()
    {
        EclRegisterWindow( new TutorialWindow() );
    }
};


class StyleEditorButton : public InterfaceButton
{
    void MouseUp()
    {
        EclRegisterWindow( new StyleEditorWindow(), m_parent );
    }
};


class TestBedButton : public InterfaceButton
{
    void MouseUp()
    {
        char serverIp[256];
        int serverPort=0;
        int numTeams=0;

        //
        // Parse the settings file

        FILE *file = fopen( "testbed.txt", "rt" );
        if( !file ) return;
        
        fscanf( file, "IP      %s\n", serverIp );
        fscanf( file, "PORT    %d\n", &serverPort );
        fscanf( file, "TEAMS   %d\n", &numTeams );
        fclose(file);


        //
        // Shut down any old games

        g_app->ShutdownCurrentGame();
        EclRemoveWindow( "LOBBY" );


        //
        // Connect to the new server
        // Open up game windows

        g_app->GetClientToServer()->ClientJoin( serverIp, serverPort );
        g_app->InitWorld();

        ConnectingWindow *connectWindow = new ConnectingWindow();
        connectWindow->m_popupLobbyAtEnd = true;
        EclRegisterWindow( connectWindow );
    }
};

class DebugResynchronisedButton : public InterfaceButton
{
    void MouseUp()
    {
        ResynchroniseWindow *window = new ResynchroniseWindow();
        window->m_debugVersion = true;
        EclRegisterWindow( window );
    }
};


DebugWindow::DebugWindow()
:   InterfaceWindow( "Tools", "dialog_tools", true )
{
    SetSize( 200, 250 );
    Centralise();
}

void DebugWindow::Create()
{
    int y = 5;
    int h = 19;
    int gap = 6;
    
    NetworkButton *network = new NetworkButton();
    network->SetProperties( "Network Data", 10, y+=h+gap, m_w-20, h, "dialog_network_title", " ", true, false );
    RegisterButton( network );

#ifdef PROFILER_ENABLED
    ProfileButton *profiler = new ProfileButton();
    profiler->SetProperties( "Profiler", 10, y+=h+gap, m_w-20, h, "dialog_profile_title", " ", true, false );
    RegisterButton( profiler );
#endif

#ifdef SOUND_EDITOR_ENABLED
    SoundStatsButton *soundStats = new SoundStatsButton();
    soundStats->SetProperties( "Sound Stats", 10, y+=h+gap, m_w-20, h, "Sound Stats", " ", false, false );
    RegisterButton( soundStats );
    
    SoundEditorButton *soundEditor = new SoundEditorButton();
    soundEditor->SetProperties( "Sound Editor", 10, y+=h+gap, m_w-20, h, "Sound Editor", " ", false, false );
    RegisterButton( soundEditor );
#endif

#ifdef _DEBUG
    //TutorialButton *tutorial = new TutorialButton();
    //tutorial->SetProperties( "Tutorial", 10, y+=h+gap, m_w-20, h, "Tutorial", " ", false, false );
    //RegisterButton( tutorial );
#endif

    StyleEditorButton *style = new StyleEditorButton();
    style->SetProperties( "Style Editor", 10, y+=h+gap, m_w-20, h, "dialog_style_editor", " ", true, false );
    RegisterButton( style );

#ifdef _DEBUG
    DebugResynchronisedButton *resync = new DebugResynchronisedButton();
    resync->SetProperties( "Resynchronise", 10, y+=h+gap, m_w-20, h, "dialog_resync_title", " ", true, false );
    RegisterButton( resync );
#endif


    //
    // Try and read testbed settings

#ifdef TESTBED
    FILE *file = fopen( "testbed.txt", "rt" );
    if( file )
    {
        fclose( file );

        TestBedButton *testBed = new TestBedButton();
        testBed->SetProperties( "Run TestBed", 10, y+=h+gap, m_w-20, h );
        RegisterButton( testBed );
    }
#endif

    CloseButton *close = new CloseButton();
    close->SetProperties( "Close", 10, m_h - 30, m_w-20, h, "dialog_close", " ", true, false );
    RegisterButton( close );

    InterfaceWindow::Create();
}


