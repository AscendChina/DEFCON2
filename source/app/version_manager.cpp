#include "lib/universal_include.h"
#include "lib/tosser/directory.h"
#include "lib/metaserver/metaserver_defines.h"

#include "network/network_defines.h"

#include "version_manager.h"



float VersionManager::VersionStringToNumber( char *_version )
{
    return atof(_version);
}


void VersionManager::EnsureCompatability( char *_version, Directory *_letter )
{
    if( !_letter ||    
        !_letter->HasData( NET_METASERVER_COMMAND, DIRECTORY_TYPE_STRING ) ) 
    {
        return;
    }

    char *cmd = _letter->GetDataString( NET_METASERVER_COMMAND );
    bool supported = true;


    //
    // Special case : MOD system messages only supported by 1.2 or greater

    if( strcmp( cmd, NET_DEFCON_SETMODPATH ) == 0 &&
        !DoesSupportModSystem(_version) )
    {
        supported = false;
    }


    //
    // Special case : White Board messages only supported by 1.3 or greater

	if( supported )
	{
		int sizeSubDir = _letter->m_subDirectories.Size();
		for( int i = sizeSubDir - 1; i >= 0; i--)
		{
			if( _letter->m_subDirectories.ValidIndex( i ) )
			{
				Directory *subDir = _letter->m_subDirectories.GetData( i );
				if( subDir->HasData( NET_DEFCON_COMMAND, DIRECTORY_TYPE_STRING ) &&
				    ( ( strcmp( subDir->GetDataString( NET_DEFCON_COMMAND ), NET_DEFCON_WHITEBOARD ) == 0 && 
				        !DoesSupportWhiteBoard(_version) ) ||
				      ( strcmp( subDir->GetDataString( NET_DEFCON_COMMAND ), NET_DEFCON_TEAM_SCORE ) == 0 && 
				        !DoesSupportSendTeamScore(_version) ) ) )
				{
					_letter->m_subDirectories.RemoveData( i );
					delete subDir;
				}
			}
		}
	}


    //
    // If the message is not supported, replace it with something harmless
    // ie an empty Update message (with no subdirectories)

    if( !supported )
    {
        _letter->CreateData( NET_METASERVER_COMMAND, NET_DEFCON_UPDATE );
        _letter->m_subDirectories.EmptyAndDelete();
    }
}


bool VersionManager::DoesSupportModSystem( char *_version )
{
    float versionNumber = VersionStringToNumber(_version);

    return( versionNumber >= 1.2f );
}


bool VersionManager::DoesSupportWhiteBoard( char *_version )
{
    float versionNumber = VersionStringToNumber(_version);

    return( versionNumber >= 1.3f );
}


bool VersionManager::DoesSupportSendTeamScore( char *_version )
{
    float versionNumber = VersionStringToNumber(_version);

    return( versionNumber >= 1.51f );
}
