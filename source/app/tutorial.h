#ifndef _included_tutorial_h
#define _included_tutorial_h

#include "lib/eclipse/eclwindow.h"
#include "lib/tosser/darray.h"
#include "lib/math/fixed.h"

class TutorialChapter;

#define PREFS_TUTORIAL_COMPLETED    "TutorialCompleted"


class Tutorial
{
protected:
    DArray  <TutorialChapter *> m_chapters;

    bool    m_worldInitialised;         // Has the world been set up ready yet?
    int     m_currentChapter;    
    float   m_messageTimer;
    int     m_nextChapter;
    float   m_nextChapterTimer;
    int     m_currentLevel;
    int     m_startLevel;
    char    m_levelName[256];
    float   m_miscTimer;                // timer variable can be used by various chapters
    
    int     m_objectHighlight;

protected:
    void    InitialiseWorld();

    void    RunNextChapter          ( float _pause=1.0f, int _chapter=-1 );
    
    void    SetupCurrentChapter     ();
    void    UpdateCurrentChapter    ();
    void    SetObjectHighlight      ( int _objectId=-1 );

    void    UpdateLevel1();
    void    UpdateLevel2();
    void    UpdateLevel3();
    void    UpdateLevel4();
    void    UpdateLevel5();
    void    UpdateLevel6();
    void    UpdateLevel7();

    int     ObjectPlacement         ( int teamId, int unitType, float longitude, float latitude, int fleetId); // cut down version of world::objectplacement, removes a lot of validity checks
    void    MissionComplete();
    void    MissionFailed();

public:
    int     m_objectIds[16];              // Object IDs for each chapter
    
public:
    Tutorial( int _startChapter );

    void    StartTutorial();

    void    Update();

    char    *GetCurrentMessage      ();    
    char    *GetCurrentObjective    ();
    float   GetMessageTimer         ();
    int     GetButtonHighlight      ( char *_windowName, char *_buttonName );
    int     GetObjectHighlight      ();

    bool    IsNextClickable         ();
    bool    IsRestartClickable      ();
    void    ClickNext               ();
    void    ClickRestart            ();
    int     GetMissionStart         ( int _mission = -1 );

    void    SetCurrentLevel         ( int _level );
    int     GetCurrentLevel         ();   
    int     GetCurrentChapter       ();
    int     GetChapterId            ( char *_name );
    bool    InChapter               ( char *_name );

    void    SetCurrentLevelName     ( char *_stringId );
    char    *GetCurrentLevelName    ();

};




// ============================================================================

class TutorialChapter
{
public:
    char    *m_name;
    char    *m_message;
    char    *m_objective;
    char    *m_windowHighlight;
    char    *m_buttonHighlight;
    bool    m_nextClickable;
    bool    m_restartClickable;

public:
    TutorialChapter()
        :   m_name(NULL),
            m_message(NULL),
            m_objective(NULL),
            m_windowHighlight(NULL),
            m_buttonHighlight(NULL),
            m_nextClickable(false),
            m_restartClickable(false)
    {}
};



// ============================================================================

class TutorialPopup : public EclWindow
{
public:
    char    m_stringId[256];
    
    int     m_targetType;           //0=none, 1=object, 2=worldpos, 3=ui
    int     m_objectId;
    char    m_windowTarget[256];
    char    m_buttonTarget[256];
    float   m_targetLongitude;
    float   m_targetLatitude;
    
    float   m_positionX;
    float   m_positionY;
    float   m_dragOffsetX;
    float   m_dragOffsetY;
    float   m_angle;
    float   m_distance;
    float   m_size;

    int     m_currentLevel;
    bool    m_rendered;
    float   m_renderTimer;

public:
    TutorialPopup();

    void    GenerateUniqueName  ( char *_name );

    void    SetMessage          ( char *_name, char *_stringId );
    void    SetDimensions       ( float _angle, float _distance, float _size );

    void    SetTargetObject     ( int _objectId );
    void    SetTargetWorldPos   ( float _longitude, float _latitude );
    void    SetUITarget         ( char *_window, char *_button );
    
    void    Update          ();
    void    Render          ( bool _hasFocus );
};



#endif
