#include "lib/universal_include.h"

#include <string.h>

#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/language_table.h"
#include "lib/hi_res_time.h"

#include "app/app.h"
#include "app/globals.h"

#include "world/city.h"
#include "world/world.h"
#include "world/team.h"
#include "world/nuke.h"

#include "interface/interface.h"

#include "renderer/map_renderer.h"


City::City()
:   WorldObject(),
    m_name(NULL),
    m_country(NULL),
    m_population(-1),
    m_capital(false),
    m_numStrikes(0),
    m_dead(0)
{
    SetType( TypeCity );
    
    m_name = strdup("City");

    strcpy( bmpImageFilename, "graphics/city.bmp" );

    m_radarRange = 0;
    AddState( "City", 0, 0, 5, 0, false );
}



bool City::Update()
{
    double realTimeNow = GetHighResTime();
    if( realTimeNow > m_nukeCountTimer )
    {
        g_app->GetWorld()->GetNumNukers( m_objectId, &m_numNukesInFlight, &m_numNukesInQueue );
        m_nukeCountTimer = realTimeNow + 2;
    }


    // deliberately don't call WorldObject::Update

    return false;
}


bool City::NuclearStrike( int causedBy, Fixed intensity, Fixed range, bool directHitPossible )
{
    if( range <= intensity/50 )
    {
        Fixed intensityEffect = (intensity/100) / 2;
        Fixed rangeEffect = 1 - range / (intensity/50);
        int deaths = ( m_population * intensityEffect * rangeEffect ).IntValue();
        m_dead += deaths;
        m_population -= deaths;
        m_numStrikes ++;

        Team *owner = g_app->GetWorld()->GetTeam(m_teamId);
        Team *guilty = g_app->GetWorld()->GetTeam(causedBy);

		bool trackedStat = false;

        if( causedBy == g_app->GetWorld()->m_myTeamId ||
			m_teamId == g_app->GetWorld()->m_myTeamId )
		{
			// We shot this nuke, or it was shot at us, so record the deaths
            trackedStat = true;
		}
        
        if( owner ) owner->m_friendlyDeaths += deaths;        
        if( !owner && guilty ) guilty->m_collatoralDamage += deaths;
        
        if( guilty )
        {
            if( g_app->GetWorld()->IsFriend(m_teamId,causedBy) )
            {
                guilty->m_collatoralDamage += deaths;
            }
            else
            {
                guilty->m_enemyKills += deaths;
            }
        }

        bool directHit = false;
        char caption[256];
        if( rangeEffect >= Fixed::Hundredths(75) && directHitPossible )
        {
            if( deaths > 1000000 )
            {
                bool messageFound = false;
                for( int i = 0; i < g_app->GetWorld()->m_messages.Size(); ++i )
                {
                    WorldMessage *wm = g_app->GetWorld()->m_messages[i];
                    if( wm->m_messageType == WorldMessage::TypeDirectHit &&
                        wm->m_longitude == m_longitude &&
                        wm->m_latitude == m_latitude )
                    {
                        char dead[16];
                        sprintf( dead, "%.1f", (deaths + wm->m_deaths) / 1000000.0f );
                        sprintf( caption, "%s", LANGUAGEPHRASE("message_directhit") );
                        LPREPLACESTRINGFLAG( 'D', dead, caption );
                        LPREPLACESTRINGFLAG( 'C', LANGUAGEPHRASEADDITIONAL(m_name), caption );
                        wm->SetMessage( caption );

                        wm->m_deaths += deaths;
                        wm->m_renderFull = true;
                        wm->m_timer += 5;
                        if( wm->m_timer > 15 )
                        {
                            wm->m_timer = 15;
                        }
                        messageFound = true;
                        break;
                    }
                }
                if( !messageFound )
                {
                    char dead[16];
                    sprintf( dead, "%.1f", deaths / 1000000.0f );
                    sprintf( caption, "%s", LANGUAGEPHRASE("message_directhit") );
                    LPREPLACESTRINGFLAG( 'D', dead, caption );
                    LPREPLACESTRINGFLAG( 'C', LANGUAGEPHRASEADDITIONAL(m_name), caption );

                    g_app->GetWorld()->AddWorldMessage( m_longitude, m_latitude, m_teamId, caption, WorldMessage::TypeDirectHit );
                    g_app->GetWorld()->m_messages[ g_app->GetWorld()->m_messages.Size() -1 ]->m_deaths = deaths;
                }
            }
            directHit = true;
        }
        
        if( !directHit && deaths > 1000000 )
        {
            char dead[16];
            sprintf( dead, "%.1f", deaths / 1000000.0f );
            sprintf( caption, "%s", LANGUAGEPHRASE("message_fallout") );
            LPREPLACESTRINGFLAG( 'D', dead, caption );
            LPREPLACESTRINGFLAG( 'C', LANGUAGEPHRASEADDITIONAL(m_name), caption );
            g_app->GetInterface()->ShowMessage( m_longitude, m_latitude, m_teamId, caption );
        }

        return directHit;
    }

    return false;
}

int City::GetEstimatedPopulation( int teamId, int cityId, int numNukes )
{
    if( g_app->GetWorld()->m_cities.ValidIndex( cityId ) )
    {
        City *city = g_app->GetWorld()->m_cities[cityId];
        int pop = city->m_population;
        for( int i = 0; i < numNukes; ++i )
        {
            pop *= 0.5f;
        }
        return pop;
    }
    else
    {
        return -1;
    }
}
