#include "lib/universal_include.h"
#include "lib/filesys/filesys_utils.h"
#include "lib/filesys/file_system.h"
#include "lib/filesys/text_stream_readers.h"
#include "lib/gucci/window_manager.h"
#include "lib/resource/resource.h"
#include "lib/metaserver/authentication.h"
#include "lib/render/styletable.h"
#include "lib/sound/soundsystem.h"
#include "lib/sound/sound_sample_bank.h"
#include "lib/debug_utils.h"
#include "lib/string_utils.h"
#include "lib/filesys/filesys_utils.h"
#include "lib/preferences.h"
#include "lib/language_table.h"

#include "renderer/map_renderer.h"

#include "interface/interface.h"
#include "interface/components/message_dialog.h"

#include "globals.h"
#include "app.h"
#include "modsystem.h"

#include "world/earthdata.h"


ModSystem *g_modSystem = NULL;

ModSystem::ModSystem()
{
	m_modsDir = newStr("mods");
#ifdef TARGET_OS_MACOSX
    if ( getenv("HOME") )
	{
		delete[] m_modsDir;
		m_modsDir = ConcatPaths( getenv("HOME"), "Library/Application Support/DEFCON/mods", NULL );
	}
#endif
}


ModSystem::~ModSystem()
{
	delete[] m_modsDir;
}


void ModSystem::Initialise()
{
    //
    // Load the critical file list

    TextFileReader *reader = (TextFileReader *)g_fileSystem->GetTextReader( "data/critical_files.txt" );
    if( reader )
    {
        while( reader->ReadLine() )
        {
            char *thisFile = reader->GetNextToken();
            m_criticalFiles.PutData( newStr( thisFile ) );
        }

        delete reader;
    }

	// Ensure mods directory exists, just as a convenience to the
	// user. Wouldn't hurt to do this on Win32, but I didn't want
	// to change existing behavior.
#ifdef TARGET_OS_MACOSX
	CreateDirectoryRecursively(m_modsDir);
#endif

    //
    // Load installed mods and set up

    LoadInstalledMods();


    //
    // Set mod path from preferences, unless we are DEMO user

    char authKey[256];
    Authentication_GetKey( authKey );
    bool demoUser = Authentication_IsDemoKey(authKey);

    if( !demoUser )
    {
        SetModPath( g_preferences->GetString( "ModPath" ) );
    }
}


void ModSystem::LoadInstalledMods()
{
    //
    // Clear out all known mods

    m_mods.EmptyAndDelete();


    //
    // Explore the mods directory, looking for installed mods

    LList<char *> *subDirs = ListSubDirectoryNames( m_modsDir );

    for( int i = 0; i < subDirs->Size(); ++i )
    {
        char *thisSubDir = subDirs->GetData(i);

        InstalledMod *mod = new InstalledMod();
        sprintf( mod->m_name, thisSubDir );
        sprintf( mod->m_path, "%s/%s/", m_modsDir, thisSubDir );
        sprintf( mod->m_version, "v1.0" );

        LoadModData( mod, mod->m_path );

        if( GetMod( mod->m_name, mod->m_version ) )
        {
            AppDebugOut( "Found installed mod '%s %s' in '%s' %s (DISCARDED:DUPLICATE)\n", 
                        mod->m_name, 
                        mod->m_version, 
                        mod->m_path,
                        mod->m_critical ? "(CRITICAL)" : " " );

            delete mod;
        }
        else
        {
            m_mods.PutData( mod );

            AppDebugOut( "Found installed mod '%s %s' in '%s' %s\n", 
                            mod->m_name, 
                            mod->m_version, 
                            mod->m_path,
                            mod->m_critical ? "(CRITICAL)" : " " );
        }
    }

    subDirs->EmptyAndDelete();
    delete subDirs;
}


LList<char *> *ModSystem::ParseModPath( char *_modPath )
{
    //
    // Tokenize the modPath into strings
    // seperated by semi-colon

    LList<char *> *tokens = new LList<char *>();
    char *currentToken = _modPath;

    while( true )
    {
        char *endPoint = strchr( currentToken, ';' );
        if( !endPoint ) break;

        *endPoint = '\x0';            
        tokens->PutData( currentToken );
        currentToken = endPoint+1;
    }

    return tokens;
}


void ModSystem::SetModPath( char *_modPath )
{
    if( _modPath )
    {
        char modPathCopy[4096];
        strcpy( modPathCopy, _modPath );

        LList<char *> *tokens = ParseModPath( modPathCopy );

        //
        // Try to install each mod from the token list,
        // Assuming its organised as name / version / name / version

        for( int i = tokens->Size()-2; i >= 0; i-=2 )
        {
            char *modName = tokens->GetData(i);
            char *version = tokens->GetData(i+1);

            ActivateMod( modName, version );
        }

        delete tokens;

        //
        // Commit the finished settings

        if( CommitRequired() )
        {
            Commit();
        }
    }
}


void ModSystem::GetCriticalModPath( char *_modPath )
{
    _modPath[0] = '\x0';
    
    for( int i = 0; i < m_mods.Size(); ++i )
    {
        InstalledMod *mod = m_mods[i];

        if( mod->m_active && mod->m_critical )
        {
            strcat( _modPath, mod->m_name );
            strcat( _modPath, ";" );
            strcat( _modPath, mod->m_version );
            strcat( _modPath, ";" );
        }
    }   
}


bool ModSystem::IsCriticalModRunning()
{
    for( int i = 0; i < m_mods.Size(); ++i )
    {
        InstalledMod *mod = m_mods[i];

        if( mod->m_active && mod->m_critical )
        {
            return true;
        }
    }

    return false;
}


bool ModSystem::CanSetModPath( char *_modPath )
{
    char modPathCopy[4096];
    strcpy( modPathCopy, _modPath );

    LList<char *> *tokens = ParseModPath( modPathCopy );

    bool result = true;

    for( int i = 0; i < tokens->Size(); i+=2 )
    {
        char *modName = tokens->GetData(i);
        char *version = tokens->GetData(i+1);

        if( !IsModInstalled( modName, version ) )
        {
            result = false;
            break;
        }
    }

    delete tokens;
    return result;
}


void ModSystem::LoadModData( InstalledMod *_mod, char *_path )
{
    //
    // Try to parse the mod.txt file

    char fullFilename[512];
    sprintf( fullFilename, "%smod.txt", _path );

    if( DoesFileExist( fullFilename ) )
    {
        TextFileReader reader( fullFilename );
        bool lineAvailable = true;

        while( lineAvailable )
        {
            lineAvailable = reader.ReadLine();

            char *fieldName = reader.GetNextToken();
            char *restOfLine = reader.GetRestOfLine();

            if( fieldName && restOfLine )
            {
                if      ( stricmp( fieldName, "Name" ) == 0 )               strcpy( _mod->m_name, reader.GetRestOfLine() );
                else if ( stricmp( fieldName, "Version" ) == 0 )            strcpy( _mod->m_version, reader.GetRestOfLine() );
                else if ( stricmp( fieldName, "Author" ) == 0 )             strcpy( _mod->m_author, reader.GetRestOfLine() );
                else if ( stricmp( fieldName, "Website" ) == 0 )            strcpy( _mod->m_website, reader.GetRestOfLine() );            
                else if ( stricmp( fieldName, "Comment" ) == 0 )            strcpy( _mod->m_comment, reader.GetRestOfLine() );            
                else
                {
                    AppDebugOut( "Error parsing '%s' : unrecognised field name '%s'\n", fullFilename, fieldName );
                }
            }
        }        

        StripTrailingWhitespace( _mod->m_name );
        StripTrailingWhitespace( _mod->m_version );
        StripTrailingWhitespace( _mod->m_author );
        StripTrailingWhitespace( _mod->m_website );
        StripTrailingWhitespace( _mod->m_comment );
    }

    //
    // Determine if this is a critical mod or not

    for( int i = 0; i < m_criticalFiles.Size(); ++i )
    {
        char fullPath[512];
        sprintf( fullPath, "%sdata/%s", _path, m_criticalFiles[i] );

        if( DoesFileExist( fullPath ) )
        {
            _mod->m_critical = true;
            break;
        }
    }
}


void ModSystem::ActivateMod( char *_mod, char *_version )
{
    //
    // De-activate any existing versions of this mod

    DeActivateMod( _mod );


    //
    // Activate that version

    bool found = false;

    for( int i = 0; i < m_mods.Size(); ++i )
    {
        InstalledMod *mod = m_mods[i];

        if( strcmp( mod->m_name, _mod ) == 0 && 
            strcmp( mod->m_version, _version ) == 0 )
        {
            mod->m_active = true;
            if( i != 0 )
            {
                m_mods.RemoveData(i);
                m_mods.PutDataAtStart( mod );
            }
            found = true;
            break;
        }
    }


    if( !found )
    {
        AppDebugOut( "Failed to activate mod '%s %s'\n", _mod, _version );
    }
}


void ModSystem::MoveMod( char *_mod, char *_version, int _move )
{
    for( int i = 0; i < m_mods.Size(); ++i )
    {
        InstalledMod *mod = m_mods[i];

        if( strcmp( mod->m_name, _mod ) == 0 && 
            strcmp( mod->m_version, _version ) == 0 )
        {
            if( m_mods.ValidIndex(i+_move) &&
                m_mods[i+_move]->m_active != mod->m_active )
            {
                // Cant move an active mod into the non-active list
                // and vice versa
                break;
            }

            if( m_mods.ValidIndex(i + _move) )
            {
                m_mods.RemoveData(i);
                m_mods.PutDataAtIndex( mod, i + _move );
            }
            else
            {
                m_mods.RemoveData(i);
                m_mods.PutDataAtIndex( mod, i );
            }

            break;
        }
    }
}


void ModSystem::DeActivateMod( char *_mod )
{
    for( int i = 0; i < m_mods.Size(); ++i )
    {
        InstalledMod *mod = m_mods[i];

        if( strcmp( mod->m_name, _mod ) == 0 && mod->m_active )
        {
            mod->m_active = false;

            //
            // Move the mod so its below all activated modes

            m_mods.RemoveData(i);
            bool added = false;

            for( int j = 0; j < m_mods.Size(); ++j )
            {
                if( !m_mods[j]->m_active )
                {
                    m_mods.PutDataAtIndex( mod, j );
                    added = true;
                    break;
                }
            }

            if( !added )
            {
                m_mods.PutDataAtEnd( mod );
            }

            break;
        }
    }
}


void ModSystem::DeActivateAllMods()
{
    for( int i = 0; i < m_mods.Size(); ++i )
    {
        InstalledMod *mod = m_mods[i];

        if( mod->m_active )
        {
            mod->m_active = false;
        }
    }
}


void ModSystem::DeActivateAllCriticalMods()
{
    for( int i = 0; i < m_mods.Size(); ++i )
    {
        InstalledMod *mod = m_mods[i];

        if( mod->m_active && mod->m_critical )
        {
            mod->m_active = false;
        }
    }
}


InstalledMod *ModSystem::GetMod( char *_mod, char *_version )
{
    for( int i = 0; i < m_mods.Size(); ++i )
    {
        InstalledMod *mod = m_mods[i];

        if( strcmp( mod->m_name, _mod ) == 0 && 
            strcmp( mod->m_version, _version ) == 0 )
        {
            return mod;
        }
    }

    return NULL;
}


bool ModSystem::IsModInstalled ( char *_mod, char *_version )
{
    return( GetMod(_mod, _version) != NULL );
}


bool ModSystem::IsCriticalModPathSet( char *_modPath )
{
    //
    // Build a list of active critical mods

    LList<int> activeMods;

    for( int i = 0; i < m_mods.Size(); ++i )
    {
        InstalledMod *mod = m_mods[i];

        if( mod->m_active && mod->m_critical )
        {
            activeMods.PutData(i);            
        }
    }   


    //
    // Tokenize the incoming mod path

    char modPathCopy[4096];
    strcpy( modPathCopy, _modPath );

    LList<char *> *tokens = ParseModPath( modPathCopy );


    //
    // Basic check - same number of mods?

    if( tokens->Size()/2 != activeMods.Size() )
    {
        delete tokens;
        return false;
    }


    //
    // Run through the Tokens, looking if they are set

    for( int i = 0; i < tokens->Size(); i+=2 )
    {
        char *desiredModName = tokens->GetData(i);
        char *desiredModVer = tokens->GetData(i+1);

        int actualModIndex = activeMods[ int(i/2) ];
        InstalledMod *actualMod = m_mods[actualModIndex];

        if( strcmp( actualMod->m_name, desiredModName ) != 0 ||
            strcmp( actualMod->m_version, desiredModVer ) != 0 )
        {
            delete tokens;
            return false;
        }
    }


    //
    // Looking good

    delete tokens;
    return true;
}


bool ModSystem::CommitRequired()
{
    //
    // Build a list of Mods that should be running

    LList<char *> modPaths;

    for( int i = 0; i < m_mods.Size(); ++i )
    {
        InstalledMod *mod = m_mods[i];

        if( mod->m_active )
        {
            modPaths.PutData( mod->m_path );
        }
    }   
    

    //
    // Right number of mods?

    if( modPaths.Size() != g_fileSystem->m_searchPath.Size() )
    {
        return true;
    }


    //
    // Do the mods match?

    for( int i = 0; i < modPaths.Size(); ++i )
    {
        char *modPath = modPaths[i];
        char *installedPath = g_fileSystem->m_searchPath[i];

        if( strcmp( modPath, installedPath ) != 0 )
        {
            return true;
        }
    }

    //
    // Looks like all is well

    return false;
}


void ModSystem::Commit()
{
    AppDebugOut( "Committing MOD settings:\n" );

    char authKey[256];
    Authentication_GetKey( authKey );
    bool demoUser = Authentication_IsDemoKey(authKey);

    g_fileSystem->ClearSearchPath();

    char fullModPath[4096];
    char modDescription[8192];
    char temp[1024];

    fullModPath[0] = '\x0';
    modDescription[0] = '\x0';
    int numActivated = 0;

    if( g_languageTable && g_windowManager )
    {
        strcat( modDescription, LANGUAGEPHRASE("dialog_mod_commit_mod_path") );
    }

    if( !demoUser )
    {
        for( int i = 0; i < m_mods.Size(); ++i )
        {
            InstalledMod *mod = m_mods[i];

            if( mod->m_active )
            {
                g_fileSystem->AddSearchPath( mod->m_path );

                AppDebugOut( "Activated mod '%s %s'\n", mod->m_name, mod->m_version );
                                
                sprintf( temp, "%s;%s;", mod->m_name, mod->m_version );
                strcat( fullModPath, temp );

                if( g_languageTable && g_windowManager )
                {
                    sprintf( temp, LANGUAGEPHRASE("dialog_mod_commit_activating_mod") );
                    LPREPLACESTRINGFLAG( 'N', mod->m_name, temp );
                    LPREPLACESTRINGFLAG( 'V', mod->m_version, temp );
                    strcat( modDescription, temp );
                }

                numActivated++;
            }
        }   
    }

    if( g_languageTable && g_windowManager )
    {
        if( numActivated == 0 )
        {
            sprintf( temp, LANGUAGEPHRASE("dialog_mod_commit_all_mods_off") );
            strcat( modDescription, temp );
        }
    }

    //
    // Open up a window to show MODs are being loaded

    MessageDialog *msgDialog = NULL;

    if( g_languageTable && g_windowManager )
    {
        msgDialog = new MessageDialog( "LOADING MODS...", modDescription, false, "dialog_mod_commit_title", true );    
        EclRegisterWindow( msgDialog );
        g_app->Render();
    }


    //
    // Restart everything

    if( g_resource )
    {
		g_languageTable->LoadLanguages();
		g_languageTable->LoadCurrentLanguage();

		g_resource->Restart();
        g_app->InitFonts();
        g_app->GetMapRenderer()->Init();

        g_app->GetEarthData()->LoadCoastlines();
        g_app->GetEarthData()->LoadBorders();

        if( !g_app->m_gameRunning )
        {
            // It's only safe to do this when a game is NOT running
            // Because the Cities stored here are used in World
            g_app->GetEarthData()->LoadCities();
        }

		// We need to make sure the sound callback isn't running while
		// we do this, so we don't swap out sounds that it's playing
		g_soundSystem->EnableCallback(false);
        g_soundSystem->m_blueprints.ClearAll();
        g_soundSystem->m_blueprints.LoadEffects();
        g_soundSystem->m_blueprints.LoadBlueprints();
        g_soundSampleBank->EmptyCache();
        g_soundSystem->PropagateBlueprints(true);
		g_soundSystem->EnableCallback(true);

        g_styleTable->Load( "default.txt" );
        g_styleTable->Load( g_preferences->GetString(PREFS_INTERFACE_STYLE) );        
    }    
        

    if( msgDialog )
    {
        EclRemoveWindow( msgDialog->m_name );
    }

    AppDebugOut( "%d MODs activated\n", numActivated );

    g_preferences->SetString( "ModPath", fullModPath );
    g_preferences->Save();
}



// ============================================================================



InstalledMod::InstalledMod()
:   m_active(false),
    m_critical(false)
{
    sprintf( m_name,    "unknown" );
    sprintf( m_version, "unknown" );
    sprintf( m_path,    "unknown" );
    sprintf( m_author,  "unknown" );
    sprintf( m_website, "unknown" );
    sprintf( m_comment, "unknown" );
}
