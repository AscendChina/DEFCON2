#include "lib/universal_include.h"
#include "lib/filesys/text_stream_readers.h"
#include "lib/filesys/file_system.h"
#include "lib/hi_res_time.h"
#include "lib/language_table.h"

#include "globals.h"
#include "app.h"
#include "tutorial.h"


Tutorial::Tutorial( int _startChapter )
:   m_currentChapter(-1),
    m_nextChapter(-1),
    m_nextChapterTimer(0.0f),
    m_objectHighlight(-1),    
    m_miscTimer(-1.0f),
    m_worldInitialised(false),
    m_startLevel(_startChapter)
{
    m_levelName[0] = '\x0';
    SetCurrentLevel(1);
    
    //
    // Parse the tutorial data file

    TextReader *reader = g_fileSystem->GetTextReader( "data/tutorial.txt" );
    AppAssert( reader && reader->IsOpen() );
    
    while( reader->ReadLine() )
    {
        if( !reader->TokenAvailable() ) continue;
        char *chapterHeading = reader->GetNextToken();
        AppAssert( stricmp( chapterHeading, "CHAPTER" ) == 0 );
        
        TutorialChapter *chapter = new TutorialChapter();
        m_chapters.PutData( chapter );

        chapter->m_name = strdup( reader->GetNextToken() );
        char temp[256];
        sprintf( temp, "tutorial_%s", chapter->m_name );
        chapter->m_message = strdup( temp );
        sprintf( temp, "tutorial_%s_obj", chapter->m_name );
        chapter->m_objective = strdup( temp );


        while( reader->ReadLine() )
        {
            if( !reader->TokenAvailable() ) continue;
            char *field = reader->GetNextToken();
            if( stricmp( field, "END" ) == 0 )      break;

            char *value = reader->GetRestOfLine();     
            if( value ) value[ strlen(value) - 1 ] = '\x0';
        
            if( stricmp( field, "MESSAGE" ) == 0 )              chapter->m_message = strdup(value);
            if( stricmp( field, "OBJECTIVE" ) == 0 )            chapter->m_objective = strdup(value);
            if( stricmp( field, "WINDOWHIGHLIGHT" ) == 0 )      chapter->m_windowHighlight = strdup(value);
            if( stricmp( field, "BUTTONHIGHLIGHT" ) == 0 )      chapter->m_buttonHighlight = strdup(value);
            
            if( stricmp( field, "NEXTCLICKABLE" ) == 0 )        
            {
                chapter->m_nextClickable = true;
                chapter->m_objective = strdup( "tutorial_clicknext" );
            }

            if( stricmp( field, "RESTARTCLICKABLE" ) == 0 )
            {
                chapter->m_restartClickable = true;
                chapter->m_objective = strdup("tutorial_restart_mission");
            }
        }
    }
}


int Tutorial::GetChapterId( char *_name )
{
    for( int i = 0; i < m_chapters.Size(); ++i )
    {
        if( m_chapters.ValidIndex(i) )
        {
            TutorialChapter *chapter = m_chapters[i];
            if( chapter->m_name &&
                stricmp( chapter->m_name, _name ) == 0 )
            {
                return i;
            }
        }
    }

    return -1;
}


int Tutorial::GetCurrentChapter()
{
    return m_currentChapter;
}


bool Tutorial::InChapter( char *_name )
{
    if( !m_chapters.ValidIndex(m_currentChapter) )
    {
        return false;
    }

    TutorialChapter *chapter = m_chapters[m_currentChapter];

    if( !chapter->m_name )
    {
        return false;
    }

    return( stricmp( chapter->m_name, _name ) == 0 );
}


void Tutorial::StartTutorial()
{
    SetCurrentLevel( m_startLevel );
    ClickRestart();
}


void Tutorial::RunNextChapter( float _pause, int _chapter )
{
    if( m_nextChapter != -1 ) 
    {
        // We are already waiting to start the next chapter
        return;
    }

    if( _chapter == -1 )
    {
        _chapter = m_currentChapter + 1;
    }

    m_nextChapter = _chapter;
    m_nextChapterTimer = GetHighResTime() + _pause;
}


void Tutorial::Update()
{
    //
    // If the world is not yet initialised, do that now

    if( !m_worldInitialised )
    {
        InitialiseWorld();
    }

    
    //
    // Is it time to run the next chapter?

    if( m_nextChapter != -1 && GetHighResTime() >= m_nextChapterTimer )
    {
        //
        // Reset everything for the start of a new chapter

        m_currentChapter = m_nextChapter;
        m_nextChapter = -1;
        m_messageTimer = GetHighResTime();
                
        SetObjectHighlight();

        SetupCurrentChapter();
    }

    
    //
    // Advance the current chapter

    if( m_currentChapter > -1 )
    {
        UpdateCurrentChapter();
    }
}


char *Tutorial::GetCurrentMessage()
{
    if( m_chapters.ValidIndex(m_currentChapter) )
    {
        TutorialChapter *chapter = m_chapters[m_currentChapter];
        char *phrase = LANGUAGEPHRASE( chapter->m_message );
        return phrase;
    }

    return NULL;
}


char *Tutorial::GetCurrentObjective()
{
    if( m_chapters.ValidIndex(m_currentChapter) )
    {
        TutorialChapter *chapter = m_chapters[m_currentChapter];
        char *phrase = LANGUAGEPHRASE( chapter->m_objective );
        return phrase;
    }

    return NULL;
}


float Tutorial::GetMessageTimer()
{
    float timeNow = GetHighResTime();
    return( timeNow - m_messageTimer );
}


bool Tutorial::IsNextClickable()
{
    if( m_chapters.ValidIndex(m_currentChapter) )
    {
        TutorialChapter *chapter = m_chapters[m_currentChapter];
        return( chapter->m_nextClickable );
    }

    return false;
}

bool Tutorial::IsRestartClickable()
{
    if( m_chapters.ValidIndex(m_currentChapter) )
    {
        TutorialChapter *chapter = m_chapters[m_currentChapter];
        return( chapter->m_restartClickable );
    }

    return false;
}


void Tutorial::ClickNext()
{
    RunNextChapter(0.0f);
}

void Tutorial::ClickRestart()
{
    int chapterId = GetMissionStart();
    if( chapterId != -1 )
    {
       RunNextChapter(0.0f, chapterId );
    }
}


void Tutorial::SetObjectHighlight( int _objectId )
{
    m_objectHighlight = _objectId;
}


int Tutorial::GetObjectHighlight()
{
    return m_objectHighlight;
}


int Tutorial::GetButtonHighlight( char *_windowName, char *_buttonName )
{
    int result = 0;

    if( m_chapters.ValidIndex(m_currentChapter) )
    {
        TutorialChapter *chapter = m_chapters[m_currentChapter];

        if( chapter->m_windowHighlight )
        {
            strcpy( _windowName, chapter->m_windowHighlight );
            ++result;
        }

        if( chapter->m_buttonHighlight )
        {
            strcpy( _buttonName, chapter->m_buttonHighlight );
            ++result;
        }
    }

    return result;
}


void Tutorial::SetCurrentLevelName( char *_stringId )
{
    strcpy( m_levelName, _stringId );
}


char *Tutorial::GetCurrentLevelName()
{
    return LANGUAGEPHRASE(m_levelName);
}


