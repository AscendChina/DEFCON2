#include "lib/universal_include.h"
#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/render/renderer.h"
#include "lib/math/vector3.h"
#include "lib/math/random_number.h"
#include "lib/language_table.h"

#include "app/app.h"
#include "app/globals.h"

#include "interface/interface.h"

#include "world/world.h"
#include "world/saucer.h"
#include "world/team.h"
#include "world/city.h"

Saucer::Saucer()
:   MovingObject(),
    m_leavingWorld(false)
{
    SetType( TypeSaucer );

    strcpy( bmpImageFilename, "graphics/saucer.bmp" );

    m_radarRange = 10;
    m_speed = Fixed::Hundredths(20);
    m_turnRate = Fixed::Hundredths(10);
    m_selectable = true;
    m_maxHistorySize = 10;
    m_range = 2000;

    m_life = 10;

    m_explosionSize = Fixed::Hundredths(5);
    m_damageTimer = 20;
    m_angle = 0.0f;

    m_movementType = MovementTypeAir;

    AddState( LANGUAGEPHRASE("state_flight"), 0, 0, 10, 180, true );
    AddState( LANGUAGEPHRASE("state_attack"), 0, 0, 10, 180, true );

    InitialiseTimers();
}


void Saucer::Action( int targetObjectId, Fixed longitude, Fixed latitude )
{
    if( m_currentState == 0 )
    {
        SetWaypoint( longitude, latitude );
    }
    else if( m_currentState == 1 )
    {
       
    }    
}

bool Saucer::Update()
{
    //
    // Do we move ?

    if( m_leavingWorld )
    {
        return true;
    }

    if( m_currentState == 0 )
    {    
        Vector3<Fixed> oldVel = m_vel;

        if( m_targetLongitude != 0 || m_targetLatitude != 0 )
        {
            bool arrived = MoveToWaypoint();
            if( arrived )
            {
                m_vel = oldVel;
                m_targetLongitude = 0;
                m_targetLatitude = 0;
                int targetId = m_targetObjectId;
                SetState( 1 );    
                m_targetObjectId = targetId;
            }
        }
        else
        {
            GetNewTarget();
        }
    }
    else 
    {
        if( g_app->GetWorld()->m_cities.ValidIndex( m_targetObjectId ) )
        {
            m_explosionSize += Fixed::Hundredths(5);
            if( m_damageTimer <= 0 )
            {
                City *city = g_app->GetWorld()->m_cities[m_targetObjectId];
                if( city->m_population > 0 )
                {
                    int deaths = (city->m_population * (m_explosionSize / 10 )).IntValue(); 
                    if( deaths > 0 )
                    {
                        Team *owner = g_app->GetWorld()->GetTeam( city->m_teamId );
                        if( owner )
                        {
                            owner->m_friendlyDeaths += deaths;
                        }
                        city->m_population -= deaths;
                        if ( city->m_population < 1000 )
                        {
                            city->m_population = 0;
                        }
                        /*char caption[256];
                        sprintf( caption, "ALIEN ATTACK on %s, %u dead", city->m_name, deaths );
                        g_app->GetInterface()->ShowMessage( m_longitude, m_latitude, TEAMID_SPECIALOBJECTS, caption, false);*/
                        m_damageTimer = 20;
                    }
                }
                else
                {
                    char msg[256];
                    sprintf( msg, LANGUAGEPHRASE("alien_attack") );
                    LPREPLACESTRINGFLAG( 'C', LANGUAGEPHRASEADDITIONAL(city->m_name), msg );

                    g_app->GetWorld()->AddWorldMessage( city->m_longitude, city->m_latitude, TEAMID_SPECIALOBJECTS, msg, WorldMessage::TypeDirectHit);
                    GetNewTarget();
                }
            }
            else
            {
                m_damageTimer -= SERVER_ADVANCE_PERIOD * g_app->GetWorld()->GetTimeScaleFactor();
                if( m_damageTimer < 0 )
                    m_damageTimer = 0;
            }
        }
        else
        {
            GetNewTarget();
        }
    }

    return MovingObject::Update();   
}

void Saucer::Render()
{
    Fixed predictionTime = Fixed::FromDouble(g_predictionTime) * g_app->GetWorld()->GetTimeScaleFactor();
    float predictedLongitude = ( m_longitude + m_vel.x * predictionTime ).DoubleValue();
    float predictedLatitude = ( m_latitude + m_vel.y * predictionTime ).DoubleValue(); 
    float size = 8.0f;


    Colour colour       = COLOUR_SPECIALOBJECTS;
    if( m_currentState == 0 )
    {
        RenderHistory(); 
    }
    
    m_angle += 0.01f;

    Image *bmpImage = g_resource->GetImage( bmpImageFilename );
    if( m_currentState == 0 )
    {  
        g_renderer->Blit( bmpImage, predictedLongitude + m_vel.x.DoubleValue() * 2,
						  predictedLatitude + m_vel.y.DoubleValue() * 2, size/2, size/2, colour, m_angle );
    }
    else
    {
        g_renderer->Blit( bmpImage, m_longitude.DoubleValue(), m_latitude.DoubleValue(), size/2, size/2, colour, m_angle );
    }
    
    if( m_currentState == 1 )
    {
        Image *explosion = g_resource->GetImage( "graphics/explosion.bmp" );
		float explosionSize = m_explosionSize.DoubleValue();
		Colour fire = Colour (200, 100, 100, 255 );
        g_renderer->Blit( explosion, m_longitude.DoubleValue() - explosionSize/4,
						  m_latitude.DoubleValue() - explosionSize/4, explosionSize/2, explosionSize/2, fire);
    }


}

Fixed Saucer::GetActionRange()
{
    return 0;
}

void Saucer::GetNewTarget()
{
    SetState(0);
    int count = 0;
    int randomCity = syncrand() % g_app->GetWorld()->m_cities.Size();
    City *city = g_app->GetWorld()->m_cities[randomCity];
    while( city->m_population == 0 &&
           count < 20 )
    {
        randomCity = syncrand() % g_app->GetWorld()->m_cities.Size();
        city = g_app->GetWorld()->m_cities[randomCity];
        count++;
    }
    if( city->m_population > 0 )
    {
        SetWaypoint( city->m_longitude, city->m_latitude );
        m_targetObjectId = randomCity;
        m_explosionSize = 0;
    }
    else
    {
        m_leavingWorld = true;
    }
}
