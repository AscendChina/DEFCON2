
#ifndef _included_maprenderer_h
#define _included_maprenderer_h

#include "world/worldobject.h"
#include "world/world.h"

class Image;
class WorldObject;
class AnimatedIcon;

#define    CLEARQUEUE_STATEID  255

#define    PREFS_GRAPHICS_BORDERS          "RenderBorders"
#define    PREFS_GRAPHICS_CITYNAMES        "RenderCityNames"
#define    PREFS_GRAPHICS_COUNTRYNAMES     "RenderCountryNames"
#define    PREFS_GRAPHICS_WATER            "RenderWater"
#define    PREFS_GRAPHICS_RADIATION        "RenderRadiation"
#define    PREFS_GRAPHICS_LOWRESWORLD      "RenderLowDetailWorld"
#define    PREFS_GRAPHICS_TRAILS           "RenderObjectTrails"

#define    STYLE_WORLD_COASTLINES          "WorldCoastlines"
#define    STYLE_WORLD_BORDERS             "WorldBorders"
#define    STYLE_WORLD_WATER               "WorldWater"
#define    STYLE_WORLD_LAND                "WorldLand"

#define    PREFS_INTERFACE_TOOLTIPS        "InterfaceTooltips"
#define    PREFS_INTERFACE_POPUPSCALE      "InterfacePopupScale"
#define    PREFS_INTERFACE_SIDESCROLL      "InterfaceSideScrolling"
#define    PREFS_INTERFACE_CAMDRAGGING     "InterfaceCameraDragging"
#define    PREFS_INTERFACE_PANICKEY        "InterfacePanicKey"
#define    PREFS_INTERFACE_STYLE           "InterfaceStyle"
#define    PREFS_INTERFACE_KEYBOARDLAYOUT  "InterfaceKeyboard"
#define    PREFS_INTERFACE_ZOOM_SPEED      "InterfaceZoomSpeed"

#define    STYLE_POPUP_BACKGROUND          "PopupBackground"
#define    STYLE_POPUP_TITLEBAR            "PopupTitleBar"
#define    STYLE_POPUP_BORDER              "PopupBorder"
#define    STYLE_POPUP_HIGHLIGHT           "PopupHighlight"
#define    STYLE_POPUP_SELECTION           "PopupSelection"
#define    FONTSTYLE_POPUP_TITLE           "FontPopupTitle"
#define    FONTSTYLE_POPUP                 "FontPopup"




class MapRenderer
{
protected:
    int     m_pixelX;               // Exact pixel positions
    int     m_pixelY;               // of the 'drawable'
    int     m_pixelW;               // part of the window
    int     m_pixelH;

    float   m_totalZoom;

    int     m_currentHighlightId;
    int     m_currentSelectionId;
    int     m_currentStateId;
    float   m_highlightTime;

    float   m_selectTime;
    float   m_deselectTime;
    int     m_previousSelectionId;

    float   m_stateRenderTime;
    int     m_stateObjectId;

	float	m_drawScale;
    
    Image   *m_territories[World::NumTerritories];
    
    Image   *bmpRadar;    
    Image   *bmpBlur;    
    Image   *bmpWater;
    Image   *bmpExplosion;

    Image   *bmpTravelNodes;
    Image   *bmpSailableWater;

    float   m_oldMouseX;  // Used for mouse idle time
    float   m_oldMouseY;

    bool    m_radarLocked;

    float   m_cameraLongitude;
    float   m_cameraLatitude;
    float   m_speedRatioX;
    float   m_speedRatioY;
    float   m_cameraIdleTime;
    int     m_cameraZoom;
    int     m_camSpeed;

    bool    m_autoCam;
    bool    m_camAtTarget;
    int     m_autoCamCounter;
    float   m_autoCamTimer;

    bool    m_lockCamControl;
    bool    m_lockCommands;
    bool    m_draggingCamera;

    int     m_currentFrames;
    int     m_framesPerSecond;
    float   m_frameCountTimer;
    bool    m_showFps;    
    float   m_tooltipTimer;

	bool    m_showWhiteBoard;			// Used to show the white board(s) without editing the player white board
	bool	m_editWhiteBoard;			// Used to show the white board panel
	bool    m_showPlanning;				// Used to edit the player white board
	bool	m_showAllWhiteBoards;		// Used to toggle between showing only the player whiteboard or all whiteboard in the alliance
	bool    m_drawingPlanning;			// Used to indicate the player is currently drawing
	bool    m_erasingPlanning;			// Used to indicate the player is currently erasing
	double  m_drawingPlanningTime;
	float   m_longitudePlanningOld;
	float   m_latitudePlanningOld;

public:
    enum
    {
        AnimationTypeActionMarker,
        AnimationTypeSonarPing,
        AnimationTypeAttackMarker,
        AnimationTypeNukePointer,
        AnimationTypeNukeMarker,
        NumAnimations
    };

    bool    m_showRadar;
    bool    m_showPopulation;
    bool    m_showOrders;
    
    bool    m_showTeam[MAX_TEAMS];
    bool    m_showAllTeams;
    bool    m_showNodes;
    bool    m_hideMouse;
    bool    m_renderEverything;
    bool    m_showNukeUnits;

    int     m_highlightUnit;
    
    float   m_middleX;
    float   m_middleY;
    float   m_zoomFactor;

	char	m_imageFiles[WorldObject::NumObjectTypes][256];

    DArray  <AnimatedIcon *>    m_animations;
    LList   <char *>            *m_tooltip;

    float   m_mouseIdleTime;

public:
    MapRenderer();
	~MapRenderer();

    void    Init();                   // Call me after the window is open

    void    Update();
    void    UpdateCameraControl     ( float _mouseX, float _mouseY );
    bool    UpdatePlanning          ( float _mouseX, float _mouseY );
    int     GetNearestObjectToMouse ( float _mouseX, float _mouseY );
    void    HandleObjectAction      ( float _mouseX, float _mouseY, int _underMouseId );
    void    HandleSetWaypoint       ( float _mouseX, float _mouseY );
    void    HandleSelectObject      ( int _underMouseId );
    void    HandleClickStateMenu    ();

    void    Render();
    void    RenderMouse();
    void    RenderCoastlines();
    void    RenderLowDetailCoastlines();
    void    RenderBorders();
    void    RenderObjects();
    void    RenderRadar();
    void    RenderRadar( bool _allies, bool _outlined );
    void    RenderCities();
    void    RenderCountryControl();
    void    RenderPopulationDensity();
    void    RenderWorldMessages();
    void    RenderNodes();
    void    RenderGunfire();
    void    RenderExplosions();
    void    RenderAnimations();
    void    RenderBlips();
    void    RenderUnitHighlight( int _objectId );
    void    RenderNukeUnits();

    void    RenderWorldObjectTargets   ( WorldObject *wobj, bool maxRanges=true );
    void    RenderWorldObjectDetails   ( WorldObject *wobj );
    void    GetWorldObjectStatePosition( WorldObject *wobj, int state,
                                         float *screenX, float *screenY, float *screenW, float *screenH );
    void    GetStatePositionSizes     ( float *titleSize, float *textSize, float *gapSize );
	
    void    RenderFriendlyObjectDetails( WorldObject *wobj, float *boxX, float *boxY, float *boxW, float *boxH, float *boxSep );
    void    RenderFriendlyObjectMenu   ( WorldObject *wobj, float *boxX, float *boxY, float *boxW, float *boxH, float *boxSep );
	void    RenderEnemyObjectDetails   ( WorldObject *wobj, float *boxX, float *boxY, float *boxW, float *boxH, float *boxSep );
    void    RenderCityObjectDetails    ( WorldObject *wobj, float *boxX, float *boxY, float *boxW, float *boxH, float *boxSep );
    void    RenderEmptySpaceDetails    ( float mouseX, float mouseY );
    void    RenderPlacementDetails     ();

    void    RenderTooltip              ( char *_tooltip );          // old, remove me eventually
    void    RenderTooltip              ( float *boxX, float *boxY, float *boxW, float *boxH, float *boxSep, float alpha );
    void    RenderActionLine           ( float fromLong, float fromLat, float toLong, float toLat,
                                         Colour col, float width, bool animate=true );

    void    GetWindowBounds             ( float *left, float *right, float *top, float *bottom );               // Returns angles
    void    ConvertPixelsToAngle        ( float pixelX, float pixelY, float *longitude, float *latitude,  bool absoluteLongitude = false );
    void    ConvertAngleToPixels        ( float longitude, float latitude, float *pixelX, float *pixelY );

    bool    IsValidTerritory            ( int teamId, Fixed longitude, Fixed latitude, bool seaUnit );
    int     GetTerritory                ( Fixed longitude, Fixed latitide, bool seaArea = false );
    bool    IsCoastline                 ( Fixed longitude, Fixed latitude );

    float   GetZoomFactor();
    float   GetDrawScale();
    void    GetPosition( float &_middleX, float &_middleY );

    int     GetLongitudeMod();

    Image   *GetTerritoryImage( int territoryId );
    int     GetTerritoryId( Fixed longitude, Fixed latitude );
    int     GetTerritoryIdUnique( Fixed longitude, Fixed latitude );

    int     GetCurrentSelectionId();
    void    SetCurrentSelectionId( int id );

    int     GetCurrentHighlightId();

    void    CenterViewport( float longitude, float latitude, int zoom = 20, int camSpeed = 200 );
    void    CenterViewport( int objectId, int zoom = 20, int camSpeed = 200 );
    void    CameraCut     ( float longitude, float latitude, int zoom = 10 );

    int     CreateAnimation( int animationType, int _fromObjectId, float longitude, float latitude );
    void    FindScreenEdge( Vector3<float> const &_line, float *_posX, float *_posY );

    void    UpdateMouseIdleTime();
    bool    IsMouseInMapRenderer();

    void    LockRadarRenderer();
    void    UnlockRadarRenderer();

    void    AutoCam();
    void    ToggleAutoCam();
    bool    GetAutoCam();

    void    DragCamera();
    void    MoveCam();

    bool    IsOnScreen( float _longitude, float _latitude, float _expandScreen = 2.0f );

	bool	GetShowWhiteBoard() const;
	void	SetShowWhiteBoard( bool showWhiteBoard );
	bool	GetEditWhiteBoard() const;
	void	SetEditWhiteBoard( bool showPlanning );
	bool    GetShowPlanning() const;
	void    SetShowPlanning( bool showPlanning );
	bool	GetShowAllWhiteBoards() const;
	void	SetShowAllWhiteBoards( bool showAllWhiteBoards );
	void    RenderWhiteBoard();
};


#endif
