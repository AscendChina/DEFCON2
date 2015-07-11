 
/*
 * ===
 * APP
 * ===
 *
 * Overall Glue object
 *
 */

#ifndef _included_app_h
#define _included_app_h

class MapRenderer;
class LobbyRenderer;
class Interface;
class Server;
class ClientToServer;
class World;
class SoundSystem;
class Game;
class StatusIcon;
class Tutorial;
class EarthData;
class NetMutex;
class AchievementTracker;


class App
{
public:                 // STARTUP OPTIONS   
    int         m_desiredScreenW;
    int         m_desiredScreenH;
    int         m_desiredColourDepth;
    bool        m_desiredFullscreen;
    
    bool        m_gameRunning;
    float       m_gameStartTimer;
    bool        m_hidden;
    bool        m_inited;

    World       *m_world;

	AchievementTracker	*m_achievementTracker;
    
protected:
    MapRenderer         *m_mapRenderer;
    LobbyRenderer       *m_lobbyRenderer;
    Interface           *m_interface;
    Game                *m_game;
    EarthData           *m_earthData;
    Server              *m_server;                  // Server process, can be NULL if client
    ClientToServer      *m_clientToServer;          // Clients connection to Server
    StatusIcon			*m_statusIcon;
    Tutorial            *m_tutorial;
    
    bool        m_mousePointerVisible;
        
public:
    App();
    ~App();

    void    MinimalInit();
	void    FinishInit();
    void    InitMetaServer();
    void    NotifyStartupErrors();
    void    Update();
    void    Render();
    void    RenderMouse();
    void    Shutdown();
   
    bool    InitServer();
    void    InitWorld(); 
    void    StartGame();
    void    StartTutorial( int _chapter );

    void    ShutdownCurrentGame();                  // Closes everything, ie client, server, world etc

    void    InitialiseWindow();                     // First time
    void    ReinitialiseWindow();                   // Window already exists, destroy first
    void    InitStatusIcon();
    void    InitFonts();
    void    InitialiseTestBed();
	void    RestartAmbienceMusic();                 // Restart the Ambience sounds and Music if necessary (call after applying mods)
    
    void    RenderOwner();
    void    RenderTitleScreen();

    MapRenderer     *GetMapRenderer();
    LobbyRenderer   *GetLobbyRenderer();
    Interface       *GetInterface();
    Server          *GetServer();
    ClientToServer  *GetClientToServer();
    World           *GetWorld();
    EarthData       *GetEarthData();
    Game            *GetGame();
    StatusIcon      *GetStatusIcon();
    Tutorial        *GetTutorial();
	
	static const char *GetAuthKeyPath();
	static const char *GetPrefsPath();

    void    HideWindow();   // panic button pressed in office mode
    bool    MousePointerIsVisible();
    void    SetMousePointerVisible(bool visible);

	void    SaveGameName();
};

void	ConfirmExit( const char *_parentWindowName );
void	AttemptQuitImmediately();
void	ToggleFullscreenAsync();

#endif
