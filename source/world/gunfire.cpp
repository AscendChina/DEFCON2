#include "lib/universal_include.h"

#include <math.h>

#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/render/renderer.h"
#include "lib/math/vector3.h"
#include "lib/math/random_number.h"
#include "lib/math/math_utils.h"
 
#include "app/app.h"
#include "app/globals.h"

#include "renderer/map_renderer.h"

#include "world/world.h"
#include "world/gunfire.h"
#include "world/team.h"
#include "world/city.h"


GunFire::GunFire( Fixed range )
:   MovingObject(),
    m_origin(-1),
    m_distanceToTarget(0)
{
    m_range = range;
    m_speed = Fixed::Hundredths(48);
    m_turnRate = Fixed::Hundredths(80);
    m_maxHistorySize = -1;
    m_movementType = MovementTypeAir;
    
    strcpy( bmpImageFilename, "graphics/laser.bmp" );

    m_speed /= g_app->GetWorld()->GetGameScale();
    m_turnRate /= g_app->GetWorld()->GetGameScale();
}

bool GunFire::Update()
{
    WorldObject *obj = g_app->GetWorld()->GetWorldObject( m_targetObjectId );
    if( obj )
    {
        SetWaypoint( obj->m_longitude + obj->m_vel.x * 5, obj->m_latitude + obj->m_vel.y * 5 );
    }
    else
    {
        //m_range = 0.0f;
        // Fly in a straight line for remainder
        m_targetLongitude = m_longitude + m_vel.x * 10;
        m_targetLatitude = m_latitude + m_vel.y * 10;
    }

    //
    // Update history  
    
    m_historyTimer -= SERVER_ADVANCE_PERIOD * g_app->GetWorld()->GetTimeScaleFactor() / 10;
    if( m_historyTimer <= 0 )
    {
        if( m_longitude > -180 ||
            m_longitude < 180 )
        {
            m_history.PutDataAtStart( new Vector3<Fixed>(m_longitude, m_latitude, 0) );
            m_historyTimer = Fixed::Hundredths(10);
        }
    }

    m_maxHistorySize = 10;
    
    while( m_maxHistorySize != -1 && 
           m_history.ValidIndex(m_maxHistorySize) )
    {
        m_history.RemoveData(m_maxHistorySize);
    }

    return MoveToWaypoint();
}

void GunFire::Render()
{
    Fixed predictionTime = Fixed::FromDouble(g_predictionTime) * g_app->GetWorld()->GetTimeScaleFactor();
    float predictedLongitude = (m_longitude + m_vel.x * Fixed(predictionTime)).DoubleValue();
    float predictedLatitude = (m_latitude + m_vel.y * Fixed(predictionTime)).DoubleValue(); 
    float size = 2;
    if( g_app->GetMapRenderer()->GetZoomFactor() <= 0.25f )
    {
        size *= g_app->GetMapRenderer()->GetZoomFactor() * 4;
    }
    float angle = atan( -m_vel.x.DoubleValue() / m_vel.y.DoubleValue() );
    if( m_vel.y < 0 ) angle += M_PI;

    //g_app->4()->Blit( bmpImage, predictedLongitude, predictedLatitude, size/2, size/2, g_app->GetWorld()->GetTeam( m_teamId )->GetTeamColour(), angle );
    //g_app->GetRenderer()->CircleFill( predictedLongitude, predictedLatitude, 0.3f * size, 8, White );
    //MovingObject::Render();

    Team *team = g_app->GetWorld()->GetTeam(m_teamId);
    Colour colour = team->GetTeamColour();            
    float maxSize = 10;
    
    //g_app->GetRenderer()->CircleFill( predictedLongitude, predictedLatitude, 0.2f * size, 8, colour );

    for( int i = 1; i < m_history.Size(); ++i )
    {
        Vector3<float> lastPos, thisPos;
		
		lastPos = *m_history[i-1];
        thisPos = *m_history[i];

        if( lastPos.x < -170 && thisPos.x > 170 )
        {
            thisPos.x = -180 - ( 180 - thisPos.x );
        }

        if( lastPos.x > 170 && thisPos.x < -170 )
        {
            thisPos.x = 180 + ( 180 - fabs(thisPos.x) );
        }

        Vector3<float> diff = thisPos - lastPos;        
        colour.m_a = 255 - 255 * (float) i / (float) maxSize;
        g_renderer->Line( lastPos.x, lastPos.y, thisPos.x, thisPos.y, colour, 0.2f );
    }

    if( m_history.Size() > 0 )
    {
        Vector3<float> lastPos, thisPos;
		
		lastPos = *m_history[ 0 ];
        thisPos = Vector3<float>( predictedLongitude, predictedLatitude, 0 );
        
        if( lastPos.x < -170 && thisPos.x > 170 )
        {
            thisPos.x = -180 - ( 180 - thisPos.x );
        }

        if( lastPos.x > 170 && thisPos.x < -170 )
        {
            thisPos.x = 180 + ( 180 - fabs(thisPos.x) );
        }

        colour.m_a = 255;
        g_renderer->Line( lastPos.x, lastPos.y, thisPos.x, thisPos.y, colour, 0.2f );
    }

    g_renderer->Line( predictedLongitude, predictedLatitude, 
                                predictedLongitude-m_vel.x.DoubleValue(), 
                                predictedLatitude-m_vel.y.DoubleValue(), colour, 2.0f );
}

bool GunFire::MoveToWaypoint()
{
    Fixed distToTarget = g_app->GetWorld()->GetDistance( m_longitude, m_latitude, m_targetLongitude, m_targetLatitude);
    Fixed timePerUpdate = SERVER_ADVANCE_PERIOD * g_app->GetWorld()->GetTimeScaleFactor();

    Fixed newLongitude;
    Fixed newLatitude;
    Fixed newDistance;

    CalculateNewPosition( &newLongitude, &newLatitude, &newDistance );

    // if the unit has reached the edge of the map, move it to the other side and update all nessecery information
    if( m_longitude <= -180 ||
        m_longitude >= 180 )
    {
        CrossSeam();
        CalculateNewPosition( &newLongitude, &newLatitude, &newDistance );
    }

    if( newDistance <= distToTarget && 
        newDistance < Fixed::Hundredths(48) &&
        m_targetLongitudeAcrossSeam == 0 )
    {
        ClearWaypoints();
        m_vel.Zero();

        return true;
    }
    else
    {
        m_range -= Vector3<Fixed>( m_vel.x * Fixed(timePerUpdate), m_vel.y * Fixed(timePerUpdate), 0 ).Mag();
        m_longitude = newLongitude;
        m_latitude = newLatitude;
        m_distanceToTarget -= Fixed(timePerUpdate) * m_vel.Mag();
    }

    return false;    
}

void GunFire::CalculateNewPosition( Fixed *newLongitude, Fixed *newLatitude, Fixed *newDistance )
{
    Vector3<Fixed> targetDir = (Vector3<Fixed>( m_targetLongitude, m_targetLatitude, 0 ) -
								Vector3<Fixed>( m_longitude, m_latitude, 0 )).Normalise();    
    
    Fixed distance = (Vector3<Fixed>( m_targetLongitude, m_targetLatitude, 0 ) -
                      Vector3<Fixed>( m_longitude, m_latitude, 0 )).Mag();
    m_speed = distance / 48;

    Fixed minSpeed = Fixed::Hundredths(40) / g_app->GetWorld()->GetGameScale();
    Fixed maxSpeed = 2 / g_app->GetWorld()->GetGameScale();

    Clamp( m_speed, minSpeed, maxSpeed );

    targetDir * m_speed;

    Fixed timePerUpdate = SERVER_ADVANCE_PERIOD * g_app->GetWorld()->GetTimeScaleFactor();
    
    Fixed factor1 = m_turnRate * timePerUpdate / 10;
    Fixed factor2 = 1 - factor1;
    m_vel = ( targetDir * factor1 ) + ( m_vel * factor2 );
    m_vel.Normalise();
    m_vel *= m_speed;


    *newLongitude = m_longitude + m_vel.x * timePerUpdate;
    *newLatitude = m_latitude + m_vel.y * timePerUpdate;
    *newDistance =g_app->GetWorld()->GetDistance( *newLongitude, *newLatitude, m_targetLongitude, m_targetLatitude );

}

void GunFire::Impact()
{
    WorldObject *targetObject = g_app->GetWorld()->GetWorldObject(m_targetObjectId);
    if( targetObject )
    {
        if( targetObject->m_type == WorldObject::TypeSub &&
            targetObject->m_currentState == 2 )
        {
            m_attackOdds *= 2;
        }
        int randomChance = syncfrand(100).IntValue();
        if( Fixed(randomChance) < m_attackOdds )
        {
            targetObject->m_life--;               
            targetObject->m_life = max( targetObject->m_life, 0 );
            if( targetObject->m_life == 0 )
            {
                g_app->GetWorld()->CreateExplosion( m_teamId, targetObject->m_longitude, targetObject->m_latitude, 30, targetObject->m_teamId );

                WorldObject *origin = g_app->GetWorld()->GetWorldObject(m_origin);
                if( origin )
                {
                    origin->SetTargetObjectId(-1);
                    //g_app->GetWorld()->AddDestroyedMessage( m_origin, m_targetObjectId );
                    origin->m_isRetaliating = false;
					targetObject->m_lastHitByTeamId = origin->m_teamId;
                }
            }
        }
        g_app->GetWorld()->CreateExplosion( m_teamId, m_longitude, m_latitude, 10, targetObject->m_teamId );
        targetObject->Retaliate( m_origin );
    }
}
