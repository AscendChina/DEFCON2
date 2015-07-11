#include "lib/universal_include.h"

#include "app/defcon_soundinterface.h"
#include "app/app.h"
#include "app/globals.h"
#include "app/game.h"

#include "world/world.h"
#include "world/worldobject.h"

#include "renderer/map_renderer.h"



bool DefconSoundInterface::ListObjectTypes( LList<char *> *_list )
{
    for( int i = 1; i < WorldObject::NumObjectTypes; ++i )
    {
        char fullName[256];
        sprintf( fullName, "Object_%s", WorldObject::GetName(i) );
        _list->PutData( strdup(fullName) );
    }

    _list->PutData( "Music" );
    _list->PutData( "Bunker" );
    _list->PutData( "Interface" );

    return true;
}


bool DefconSoundInterface::ListObjectEvents( LList<char *> *_list, char *_objType )
{
    if( stricmp( _objType, "Music" ) == 0 )
    {
        _list->PutData( "StartMusic" );
    }

    if( stricmp( _objType, "Bunker" ) == 0 )
    {
        _list->PutData( "StartAmbience" );
    }

    if( stricmp( _objType, "Interface" ) == 0 )
    {
        _list->PutData( "HighlightObject" );
        _list->PutData( "HighlightObjectState" );
        _list->PutData( "SelectObjectState" );
        _list->PutData( "SelectObject" );
        _list->PutData( "DeselectObject" );
        _list->PutData( "SetMovementTarget" );
        _list->PutData( "SetCombatTarget" );
        _list->PutData( "Error" );
        _list->PutData( "Text" );
        _list->PutData( "DefconChange" );
        _list->PutData( "GameOver" );
        _list->PutData( "FirstLaunch" );
        _list->PutData( "ReceiveChatMessage" );
        _list->PutData( "SendChatMessage" );
    }

    if( stricmp( _objType, "Object_Sub" ) == 0 )
    {
        _list->PutData( "SonarPing" );
    }

    if( stricmp( _objType, "Object_Carrier" ) == 0 )
    {
        _list->PutData( "SonarPing" );
        _list->PutData( "DepthCharge" );
    }

    if( stricmp( _objType, "Object_Nuke" ) == 0 )
    {
        _list->PutData( "Detonate" );
    }

    return true;
}


bool DefconSoundInterface::DoesObjectExist( SoundObjectId _id )
{
    if( !g_app->m_gameRunning ) return false;
    
    WorldObject *wobj = g_app->GetWorld()->GetWorldObject( _id.m_data );
    return( wobj != NULL );
}


char *DefconSoundInterface::GetObjectType( SoundObjectId _id )
{
    if( !g_app->m_gameRunning ) return NULL;

    WorldObject *wobj = g_app->GetWorld()->GetWorldObject( _id.m_data );
    if( wobj )
    {
        static char typeName[256];
        sprintf( typeName, "Object_%s", WorldObject::GetName(wobj->m_type) );
        return typeName;
    }

    return NULL;
}


bool DefconSoundInterface::GetObjectPosition( SoundObjectId _id, Vector3<float> &_pos, Vector3<float> &_vel )
{
    if( !g_app->m_gameRunning ) return false;

    WorldObject *wobj = g_app->GetWorld()->GetWorldObject( _id.m_data );
    if( wobj )
    {
        _pos.x = wobj->m_longitude.DoubleValue();
        _pos.y = wobj->m_latitude.DoubleValue();
        _pos.z = 0;

        _vel = wobj->m_vel;

        return true;
    }

    return false;
}


bool DefconSoundInterface::GetCameraPosition( Vector3<float> &_pos, Vector3<float> &_front, Vector3<float> &_up, Vector3<float> &_vel )
{
    _pos.x = g_app->GetMapRenderer()->m_middleX;
    _pos.y = g_app->GetMapRenderer()->m_middleY;
    _pos.z = g_app->GetMapRenderer()->m_zoomFactor * 100;

    _front.Set( 0, 0, 1 );
    _up.Set( 0, -1, 0 );
    _vel.Zero();

    return true;
}


bool DefconSoundInterface::ListProperties( LList<char *> *_list )
{
    _list->PutData( "Object_Longitude" );
    _list->PutData( "Object_Latitude" );
    _list->PutData( "Object_Velocity" );

    _list->PutData( "Camera_Distance" );

    _list->PutData( "Team_Survivors" );
    _list->PutData( "Team_Kills" );

    _list->PutData( "World_NumExplosions" );

    return true;
}


bool DefconSoundInterface::GetPropertyRange( char *_property, float *_min, float *_max )
{
    *_min = 0;
    *_max = 0;

    if( stricmp(_property, "Object_Longitude") == 0 )
    {
        *_min = 0.0f;
        *_max = 360.0f;
    }

    if( stricmp(_property, "Object_Latitude") == 0 )
    {
        *_min = 0.0f;
        *_max = 180.0f;
    }

    if( stricmp(_property, "Object_Velocity") == 0 )
    {
        *_min = 0.0f;
        *_max = 100.0f;
    }

    if( stricmp(_property, "Camera_Distance") == 0 )
    {
        *_min = 0.0f;
        *_max = 500.0f;
    }

    if( stricmp(_property, "Team_Survivors") == 0 ||
        stricmp(_property, "Team_Kills") == 0 )
    {
        *_min = 0.0f;
        *_max = 100.0f;
    }

    if( stricmp(_property, "World_NumExplosions") == 0 )
    {
        *_min = 0.0f;
        *_max = 30.0f;
    }

    return true;
}


float DefconSoundInterface::GetPropertyValue( char *_property, SoundObjectId _id )
{    
    WorldObject *wobj = g_app->m_gameRunning ? 
                        g_app->GetWorld()->GetWorldObject( _id.m_data ) :
                        NULL;

    if( stricmp(_property, "Object_Longitude") == 0 )
    {
        if( wobj ) return wobj->m_longitude.DoubleValue() + 180.0f;
        return 0.0f;
    }


    if( stricmp(_property, "Object_Latitude") == 0 )
    {
        if( wobj ) return wobj->m_latitude.DoubleValue() + 90.0f;
        return 0.0f;
    }


    if( stricmp(_property, "Object_Velocity") == 0 )
    {
        if( wobj ) return wobj->m_vel.Mag().DoubleValue();
    }


    if( stricmp(_property, "Camera_Distance") == 0 )
    {
        if( wobj )
        {
            Vector3<float> camPos, camFront, camUp, camVel;
            GetCameraPosition( camPos, camFront, camUp, camVel );

            Vector3<float> objPos( wobj->m_longitude.DoubleValue(), wobj->m_latitude.DoubleValue(), 0 );
            return (camPos - objPos).Mag();            
        }

        return 0.0f;
    }


    if( stricmp(_property, "Team_Survivors") == 0 )
    {
        static float s_previousValue = 100.0f;
        if( !g_app->m_gameRunning ) return s_previousValue;

        bool allAIs = true;
        float averageFader = 0.0f;
        for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
        {
            Team *team = g_app->GetWorld()->m_teams[i];
            averageFader += team->m_teamColourFader;
            if( team->m_type != Team::TypeAI )
            {
                allAIs = false;
            }
        }
        averageFader /= (float) g_app->GetWorld()->m_teams.Size();

        float fraction = 100.0f;
        Team *team = g_app->GetWorld()->GetMyTeam();

        if( team && !allAIs )
        {
            fraction = team->m_teamColourFader * 100;                    
        }
       
        Clamp( fraction, 0.0f, 100.0f );
        s_previousValue = fraction;
        return fraction;
    }


    if( stricmp(_property, "Team_Kills") == 0 )
    {
        if( !g_app->m_gameRunning ) return 0.0f;

        Team *team = g_app->GetWorld()->GetMyTeam();
        if( !team ) return 0.0f;
        
        float startintPop = g_app->GetGame()->GetOptionValue( "PopulationPerTerritory" );
        startintPop *= g_app->GetGame()->GetOptionValue( "TerritoriesPerTeam" );

        float fraction = 100 * team->m_enemyKills / startintPop;
        Clamp( fraction, 0.0f, 100.0f );

        return fraction;
    }


    if( stricmp(_property, "World_NumExplosions") == 0 )
    {
        if( !g_app->m_gameRunning ) return 0.0f;

        int result = 0;
        
        for( int i = 0; i < g_app->GetWorld()->m_explosions.Size(); ++i )
        {
            if( g_app->GetWorld()->m_explosions.ValidIndex(i) )
            {
                Explosion *explosion = g_app->GetWorld()->m_explosions[i];
                if( explosion->m_initialIntensity > 50 )
                {
                    ++result;
                }
            }

            if( result >= 30 ) break;
        }

        return result;
    }

    return SoundSystemInterface::GetPropertyValue( _property, _id );
}
