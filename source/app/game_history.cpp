#include "lib/universal_include.h"
#include "lib/tosser/directory.h"
#include "lib/metaserver/metaserver_defines.h"
#include "lib/math/math_utils.h"
#include "lib/filesys/text_stream_readers.h"
#include "lib/string_utils.h"

#include "game_history.h"


GameHistory g_gameHistory;


GameHistory::GameHistory()
{
    Load();
}


void GameHistory::JoinedGame( char *_ip, int _port, const char *_name )
{
    bool found = false;

    char identity[512];
    sprintf( identity, "%s:%d", _ip, _port );


    //
    // if the server already exists, update it

    for( int i = 0; i < m_servers.Size(); ++i )
    {
        GameHistoryServer *server = m_servers[i];
        if( strcmp( server->m_identity, identity ) == 0 )
        {
            strcpy( server->m_name, _name );
            
            if( i != 0 )
            {
                m_servers.RemoveData(i);
                m_servers.PutDataAtStart( server );
            }
            found = true;
            break;
        }
    }


    //
    // Didnt find the server, so add it to the list

    if( !found )
    {
        GameHistoryServer *server = new GameHistoryServer();
        strcpy( server->m_identity, identity );
        strcpy( server->m_name, _name );
        m_servers.PutDataAtStart( server );
    }


    //
    // Purge list

    while( m_servers.ValidIndex(10) )
    {
        GameHistoryServer *server = m_servers[10];
        delete server;
        m_servers.RemoveData(10);
    }


    //
    // Save list

    Save();
}


int GameHistory::HasJoinedGame( char *_ip, int _port )
{
    char identity[512];
    sprintf( identity, "%s:%d", _ip, _port );

    

    for( int i = 0; i < m_servers.Size(); ++i )
    {
        GameHistoryServer *server = m_servers[i];
        if( strcmp( server->m_identity, identity ) == 0 )
        {
            int returnValue = i+1;
            Clamp(returnValue, 1, 10 );
            return returnValue;
        }
    }



    return 0;
}


void GameHistory::Save()
{
    FILE *file = fopen( "game-history.txt", "wt" );
    if( file )
    {
        for( int i = 0; i < m_servers.Size(); ++i )
        {
            GameHistoryServer *server = m_servers[i];

            fprintf( file, "%s,%s\n", server->m_identity, server->m_name );
        }

        fclose( file );
    }
}


void GameHistory::Load()
{
    TextFileReader reader( "game-history.txt" );

    if( reader.IsOpen() )
    {
        reader.SetSeperatorChars( "\n\r," );

        while( reader.ReadLine() )
        {
			char *identity = NULL;
			char *name = NULL;
			if( reader.TokenAvailable() )
			{
				identity = reader.GetNextToken();
				if( reader.TokenAvailable() )
				{
					name = reader.GetNextToken();
				}
			}

			if( identity && name )
			{
				GameHistoryServer *server = new GameHistoryServer();
				
				strncpy( server->m_identity, identity, sizeof(server->m_identity) );
				server->m_identity[ sizeof(server->m_identity) - 1 ] = '\x0';
				SafetyString( server->m_identity );

				strncpy( server->m_name, name, sizeof(server->m_name) );
				server->m_name[ sizeof(server->m_name) - 1 ] = '\x0';
				SafetyString( server->m_name );

				m_servers.PutData( server );
			}
        }
    }
}


