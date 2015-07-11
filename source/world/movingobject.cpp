#include "lib/universal_include.h"

#include <math.h>

#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/math/math_utils.h"
#include "lib/math/random_number.h"
#include "lib/sound/soundsystem.h"
#include "lib/preferences.h"
#include "lib/gucci/input.h"

#include "app/globals.h"
#include "app/app.h"
#include "app/game.h"

#include "renderer/map_renderer.h"

#include "network/ClientToServer.h"

#include "interface/interface.h"

#include "world/world.h"
#include "world/movingobject.h"
#include "world/blip.h"
#include "world/fleet.h"


MovingObject::MovingObject()
:   WorldObject(),
    m_speed(0),
    m_historyTimer(0),
    m_maxHistorySize(10),
    m_targetLongitude(0),
    m_targetLatitude(0),
    m_movementType(MovementTypeLand),
    m_range(0),
    m_targetNodeId(-1),
    m_finalTargetLongitude(0),
    m_finalTargetLatitude(0),
    m_pathCalcTimer(1),
    m_targetLongitudeAcrossSeam(0),
    m_targetLatitudeAcrossSeam(0),
    m_blockHistory(false),
    m_isLanding(-1),
    m_turning(false),
    m_angleTurned(0)
{
}

MovingObject::~MovingObject()
{
    m_history.EmptyAndDelete();
    //m_movementBlips.EmptyAndDelete();
}


void MovingObject::InitialiseTimers()
{
    WorldObject::InitialiseTimers();

    Fixed gameScale = World::GetGameScale();
    m_speed /= gameScale;
    //m_range /= gameScale;
}


bool MovingObject::Update()
{
    //
    // Update history    
    m_historyTimer -= SERVER_ADVANCE_PERIOD * g_app->GetWorld()->GetTimeScaleFactor() / 10;
    if( m_historyTimer <= 0 )
    {
        m_history.PutDataAtStart( new Vector3<Fixed>(m_longitude, m_latitude, 0) );
        m_historyTimer = 2;
    }

    while( m_maxHistorySize != -1 && 
           m_history.ValidIndex(m_maxHistorySize) )
    {
        m_history.RemoveData(m_maxHistorySize);
    }


    for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
    {
        Team *team = g_app->GetWorld()->m_teams[i];
        m_lastSeenTime[team->m_teamId] -= g_app->GetWorld()->GetTimeScaleFactor() * SERVER_ADVANCE_PERIOD ;
        if( m_lastSeenTime[team->m_teamId] < 0 ) m_lastSeenTime[team->m_teamId] = 0;
    }

    if( m_isLanding != -1 )
    {
        WorldObject *home = g_app->GetWorld()->GetWorldObject( m_isLanding );
        if( home )
        {
            if((m_type == WorldObject::TypeFighter &&
                home->m_states[0]->m_numTimesPermitted >= home->m_maxFighters ) ||
                (m_type == WorldObject::TypeBomber &&
                home->m_states[1]->m_numTimesPermitted >= home->m_maxBombers ))
            {
                Land( GetClosestLandingPad() );
            }            
            else if( g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, home->m_longitude, home->m_latitude ) < 2 * 2 )
            {
                if( home->m_teamId == m_teamId )
                {
                    bool landed = false;
                    if( m_type == WorldObject::TypeFighter )
                    {
                        if( home->m_states[0]->m_numTimesPermitted < home->m_maxFighters )
                        {
                            home->m_states[0]->m_numTimesPermitted++;
                            landed = true;
                        }
                    }
                    else
                    {
                        if( home->m_states[1]->m_numTimesPermitted < home->m_maxBombers )
                        {
                            home->m_states[1]->m_numTimesPermitted++;
                            home->m_nukeSupply += m_states[1]->m_numTimesPermitted;
                            landed = true;
                        }
                    }
                    if( landed )
                    {
                        m_life = 0;
						m_lastHitByTeamId = -1;
                    }
                    else
                    {
                        Land( GetClosestLandingPad() );
                    }
                }
            }
            else if( home->IsMovingObject() &&
                home->m_vel.MagSquared() > 0 )
            {
                Land( m_isLanding );
            }
        }
        else
        {
            m_isLanding = -1;
        }
    }

    for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
    {
        Team *team = g_app->GetWorld()->m_teams[i];
        if( m_lastSeenTime[team->m_teamId] <= 0 )
        {
            m_seen[team->m_teamId] = false;
        }
    }
 
    return WorldObject::Update();
}

bool MovingObject::IsValidPosition ( Fixed longitude, Fixed latitude )
{
    bool validWaypoint = false;

    if( m_movementType == MovementTypeLand && !g_app->GetMapRenderer()->IsValidTerritory( -1, longitude, latitude, false ) ) validWaypoint = true;
    if( m_movementType == MovementTypeSea && g_app->GetMapRenderer()->IsValidTerritory( -1, longitude, latitude, true ) ) validWaypoint = true;
    if( m_movementType == MovementTypeAir ) validWaypoint = true;

    return validWaypoint;
}

void MovingObject::SetWaypoint( Fixed longitude, Fixed latitude )
{
    if( latitude > 100 || latitude < -100 )
    {
        return;
    }

    // stop the unit before setting a new course
    ClearWaypoints();
       
    if( m_movementType == MovementTypeAir )
    {
        Fixed directDistanceSqd = g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, longitude, latitude, true);
        Fixed distanceAcrossSeamSqd = g_app->GetWorld()->GetDistanceAcrossSeamSqd( m_longitude, m_latitude, longitude, latitude);

        if( distanceAcrossSeamSqd < directDistanceSqd )
        {
            Fixed targetSeamLatitude;
            Fixed targetSeamLongitude;
            g_app->GetWorld()->GetSeamCrossLatitude( Vector3<Fixed>( longitude, latitude, 0 ), 
                                                     Vector3<Fixed>(m_longitude, m_latitude, 0), 
                                                     &targetSeamLongitude, &targetSeamLatitude);
            if(targetSeamLongitude < 0)
            {
                longitude -= 360;
            }
            else 
            {
                longitude += 360;
            }
        }

    }
    m_targetLongitude = longitude;
    m_targetLatitude = latitude;
}


char *HashDouble( double value, char *buffer );


void FFClamp( Fixed &f, unsigned long long clamp )
{
    union {
        double d;
        unsigned long long l;
    } value;

    value.d = f.DoubleValue();
    value.l &= ~(unsigned long long)clamp;

    f = Fixed::FromDouble( value.d );
}



//void MovingObject::CalculateNewPosition( Fixed *newLongitude, Fixed *newLatitude, Fixed *newDistance )
//{
//    if( m_longitude > -180 && m_longitude < 0 && m_targetLongitude > 180 )
//    {
//        m_targetLongitude -= 360;
//    }
//    else if( m_longitude < 180 && m_longitude > 0 && m_targetLongitude < -180 )
//    {
//        m_targetLongitude += 360;
//    }
//    
//    Vector3<Fixed> targetDir = (Vector3<Fixed>( m_targetLongitude, m_targetLatitude, 0 ) -
//								Vector3<Fixed>( m_longitude, m_latitude, 0 )).Normalise();
//    Vector3<Fixed> originalTargetDir = targetDir;
//
//    Fixed timePerUpdate = SERVER_ADVANCE_PERIOD * g_app->GetWorld()->GetTimeScaleFactor();
//    
//    Fixed factor1 = m_turnRate * timePerUpdate / 10;
//    Fixed factor2 = 1 - factor1;
//
//    {
//        char buffer1[32];
//        char buffer2[32];
//        SyncRandLog( "a: %s %s\n", HashDouble( m_vel.x.DoubleValue(), buffer1 ), HashDouble( m_vel.y.DoubleValue(), buffer2 ) );
//    }
//
//    m_vel = ( targetDir * factor1 ) + ( m_vel * factor2 );
//
//    FFClamp(m_vel.x, 0xF);
//    FFClamp(m_vel.y, 0xF);
//    FFClamp(m_vel.z, 0xF);
//
//
//    {
//        char buffer1[32];
//        char buffer2[32];
//        char buffer3[32];
//        char buffer4[32];
//        char buffer5[32];
//        char buffer6[32];
//        SyncRandLog( "a1: %s %s %s %s %s %s\n", 
//                            HashDouble( factor1.DoubleValue(), buffer3 ),
//                            HashDouble( factor2.DoubleValue(), buffer4 ),
//                            HashDouble( targetDir.x.DoubleValue(), buffer5 ),
//                            HashDouble( targetDir.y.DoubleValue(), buffer6 ),
//                            HashDouble( m_vel.x.DoubleValue(), buffer1 ), 
//                            HashDouble( m_vel.y.DoubleValue(), buffer2 ) );
//    }
//
//    //m_vel.Normalise();
//
//    Fixed lenSqrd = m_vel.x*m_vel.x + m_vel.y*m_vel.y + m_vel.z*m_vel.z;
//    if (lenSqrd > 0)
//    {
//        Fixed len = sqrt(lenSqrd);
//        Fixed invLen = 1 / len;
//
//        {
//            char buffer1[32];
//            char buffer2[32];
//            char buffer3[32];
//            SyncRandLog( "a2: %s %s %s\n", 
//                            HashDouble( lenSqrd.DoubleValue(), buffer1 ), 
//                            HashDouble( len.DoubleValue(), buffer2 ),
//                            HashDouble( invLen.DoubleValue(), buffer3 ) );
//        }
//
//        m_vel.x *= invLen;
//        m_vel.y *= invLen;
//        m_vel.z *= invLen;
//    }
//    else
//    {
//        m_vel.x = m_vel.y = 0;
//        m_vel.z = 1;
//    }
//
//    FFClamp(m_vel.x, 0xFF);
//    FFClamp(m_vel.y, 0xFF);
//    FFClamp(m_vel.z, 0xFF);
//
//    {
//        char buffer1[32];
//        char buffer2[32];
//        SyncRandLog( "b: %s %s\n", HashDouble( m_vel.x.DoubleValue(), buffer1 ), HashDouble( m_vel.y.DoubleValue(), buffer2 ) );
//    }
//
//	Fixed dotProduct = originalTargetDir * m_vel;
//    Fixed angle = acos( dotProduct );
//
//    if( dotProduct < Fixed::FromDouble(-0.98) )
//    {
//        m_turning = true;
//    }
//
//    if( m_turning )
//    {
//        Fixed turn = (angle / 50) * timePerUpdate;
//        m_vel.RotateAroundZ( turn );
//        m_vel.Normalise();
//        m_angleTurned += turn;
//        if( turn > Fixed::Hundredths(12) )
//        {
//            m_turning = false;
//            m_angleTurned = 0;
//        }
//    }
//
//    m_vel *= m_speed;
//
//    FFClamp(m_vel.x, 0xFFF);
//    FFClamp(m_vel.y, 0xFFF);
//    FFClamp(m_vel.z, 0xFFF);
//
//    {
//        char buffer1[32];
//        char buffer2[32];
//        SyncRandLog( "c: %s %s\n", HashDouble( m_vel.x.DoubleValue(), buffer1 ), HashDouble( m_vel.y.DoubleValue(), buffer2 ) );
//    }
//
//    *newLongitude = m_longitude + m_vel.x * Fixed(timePerUpdate);
//    *newLatitude = m_latitude + m_vel.y * Fixed(timePerUpdate);
//    *newDistance = g_app->GetWorld()->GetDistance( *newLongitude, *newLatitude, m_targetLongitude, m_targetLatitude );
//}


void MovingObject::CalculateNewPosition( Fixed *newLongitude, Fixed *newLatitude, Fixed *newDistance )
{
    if( m_longitude > -180 && m_longitude < 0 && m_targetLongitude > 180 )
    {
        m_targetLongitude -= 360;
    }
    else if( m_longitude < 180 && m_longitude > 0 && m_targetLongitude < -180 )
    {
        m_targetLongitude += 360;
    }

    Vector3<Fixed> targetDir = (Vector3<Fixed>( m_targetLongitude, m_targetLatitude, 0 ) -
                                Vector3<Fixed>( m_longitude, m_latitude, 0 )).Normalise();
    Vector3<Fixed> originalTargetDir = targetDir;

    Fixed timePerUpdate = SERVER_ADVANCE_PERIOD * g_app->GetWorld()->GetTimeScaleFactor();

    Fixed factor1 = m_turnRate * timePerUpdate / 10;
    Fixed factor2 = 1 - factor1;

    m_vel = ( targetDir * factor1 ) + ( m_vel * factor2 );
    m_vel.Normalise();

    Fixed dotProduct = originalTargetDir * m_vel;

    if( dotProduct < Fixed::FromDouble(-0.98) )
    {
        m_turning = true;
    }

    if( m_turning )
    {
        Fixed angle = acos( dotProduct );
        Fixed turn = (angle / 50) * timePerUpdate;
        m_vel.RotateAroundZ( turn );
        m_vel.Normalise();
        m_angleTurned += turn;
        if( turn > Fixed::Hundredths(12) )
        {
            m_turning = false;
            m_angleTurned = 0;
        }
    }

    m_vel *= m_speed;

    *newLongitude = m_longitude + m_vel.x * Fixed(timePerUpdate);
    *newLatitude = m_latitude + m_vel.y * Fixed(timePerUpdate);
    *newDistance = g_app->GetWorld()->GetDistance( *newLongitude, *newLatitude, m_targetLongitude, m_targetLatitude );
}


bool MovingObject::MoveToWaypoint()
{
    Fixed timePerUpdate = SERVER_ADVANCE_PERIOD * g_app->GetWorld()->GetTimeScaleFactor();
    m_range -= Vector3<Fixed>( m_vel.x * Fixed(timePerUpdate), m_vel.y * Fixed(timePerUpdate), 0 ).Mag();
    if( m_targetLongitude != 0 &&
        m_targetLatitude != 0 )
    {
        Fixed distToTarget = g_app->GetWorld()->GetDistance( m_longitude, m_latitude, m_targetLongitude, m_targetLatitude);

        Fixed newLongitude;
        Fixed newLatitude;
        Fixed newDistance;

        CalculateNewPosition( &newLongitude, &newLatitude, &newDistance );

        // if the unit has reached the edge of the map, move it to the other side and update all nessecery information
        if( newLongitude <= -180 ||
            newLongitude >= 180 )
        {
            m_longitude = newLongitude;
            CrossSeam();
            newLongitude = m_longitude;
            newDistance = g_app->GetWorld()->GetDistance( newLongitude, newLatitude, m_targetLongitude, m_targetLatitude );
        }

        if( (newDistance < timePerUpdate * m_speed * Fixed::Hundredths(50)) ||
            (m_movementType == MovementTypeAir &&
             newDistance < 3 &&
             newDistance > distToTarget) ||
            (m_movementType == MovementTypeSea &&
             newDistance < 1 &&
             newDistance > distToTarget))
        {
            ClearWaypoints();
            if( m_movementType == MovementTypeSea )
            {
                m_vel.Zero();
            }
            return true;
        }
        else
        {
            if( m_range <= 0 )
            {
                m_life = 0;
				m_lastHitByTeamId = m_teamId;
                g_app->GetWorld()->AddOutOfFueldMessage( m_objectId );
            }
            m_longitude = newLongitude;
            m_latitude = newLatitude;
        }
        return false;
    }
    return true;
    
}

void MovingObject::CrossSeam()
{
    if( m_longitude >= 180 )
    {
        Fixed amountCrossed = m_longitude - 180;
        m_longitude = -180 + amountCrossed;
    }
    else if( m_longitude <= -180 )
    {
        Fixed amountCrossed = m_longitude + 180;
        m_longitude = 180 + amountCrossed;
    }

    if( m_targetLongitude > 180 )
    {
        m_targetLongitude -= 360;
    }
    else if( m_targetLongitude < -180 )
    {
        m_targetLongitude += 360;
    }

    if( m_type != WorldObject::TypeNuke )
    {
        // nukes have their own seperate calculation for this kind of thing
        Fixed seamDistanceSqd =  g_app->GetWorld()->GetDistanceAcrossSeamSqd( m_longitude, m_latitude, m_targetLongitude, m_targetLatitude );
        Fixed directDistanceSqd = g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, m_targetLongitude, m_targetLatitude, true );

        if( seamDistanceSqd < directDistanceSqd )
        {
            // target has crossed seam when it shouldnt have (possible while turning )
            if( m_targetLongitude < 0 )
            {
                m_targetLongitude += 360;
            }
            else
            {
                m_targetLongitude -= 360;
            }
        }
    }
}

void MovingObject::Render()
{    
    //
    // Render history
    if( g_preferences->GetInt( PREFS_GRAPHICS_TRAILS ) == 1 )
    {
        RenderHistory();
    }
    
    if( m_movementType == MovementTypeAir )
    {
        float angle = atan( -m_vel.x.DoubleValue() / m_vel.y.DoubleValue() );
        if( m_vel.y < 0 ) angle += M_PI;
        
        Fixed predictionTime = Fixed::FromDouble(g_predictionTime) * g_app->GetWorld()->GetTimeScaleFactor();
        float predictedLongitude = (m_longitude + m_vel.x * Fixed(predictionTime)).DoubleValue();
        float predictedLatitude = (m_latitude + m_vel.y * Fixed(predictionTime)).DoubleValue();
        float size = GetSize().DoubleValue();

        Team *team = g_app->GetWorld()->GetTeam(m_teamId);
        Colour colour = team->GetTeamColour();
        colour.m_a = 255;
        
        Image *bmpImage = g_resource->GetImage( bmpImageFilename );
        g_renderer->Blit( bmpImage, 
                          predictedLongitude + m_vel.x.DoubleValue() * 2, 
                          predictedLatitude + m_vel.y.DoubleValue() * 2, 
                          size/2, 
                          size/2, 
                          colour, 
                          angle );


        //
        // Current selection?

        colour.Set(255,255,255,255);
        int selectionId = g_app->GetMapRenderer()->GetCurrentSelectionId();
        for( int i = 0; i < 2; ++i )
        {
            if( i == 1 ) 
            {            
                int highlightId = g_app->GetMapRenderer()->GetCurrentHighlightId();
                if( highlightId == selectionId ) break;
                selectionId = highlightId;
            }

            if( selectionId == m_objectId )
            {
                bmpImage = g_resource->GetImage( GetBmpBlurFilename() );
                g_renderer->Blit( bmpImage, 
                                predictedLongitude + m_vel.x.DoubleValue() * 2, 
                                predictedLatitude + m_vel.y.DoubleValue() * 2, 
                                size/2, 
                                size/2, 
                                colour, 
                                angle );

            }
            colour.m_a *= 0.5f;
        }
    }
    else
    {
        WorldObject::Render();
    }
}

void MovingObject::RenderHistory()
{
    Fixed predictionTime = Fixed::FromDouble(g_predictionTime) * g_app->GetWorld()->GetTimeScaleFactor();
    float predictedLongitude = (m_longitude + m_vel.x * Fixed(predictionTime)).DoubleValue();
    float predictedLatitude = (m_latitude + m_vel.y * Fixed(predictionTime)).DoubleValue(); 

    int maxSize = m_history.Size();
    
    int sizeCap = (int)(80 * g_app->GetMapRenderer()->GetZoomFactor() );
    sizeCap /= World::GetGameScale().DoubleValue();

    if( g_app->GetGame()->GetOptionValue("GameMode") == GAMEMODE_BIGWORLD )
    {
        switch( m_type )
        {
            case TypeNuke:
                sizeCap = 12 * g_app->GetMapRenderer()->GetZoomFactor();
                if( g_app->GetMapRenderer()->GetZoomFactor() < 0.25f )
                {
                    return;
                }
                break;

            case TypeBattleShip:
            case TypeCarrier:
            case TypeSub:
                break;

            default:
                return;
        }

        if( sizeCap < 2 ) return;
    }

    maxSize = ( maxSize > sizeCap ? sizeCap : maxSize );
        
    Team *team = g_app->GetWorld()->GetTeam(m_teamId);
    Colour colour;
    if( team )
    {
        colour = team->GetTeamColour();
    }
    else
    {
        colour = COLOUR_SPECIALOBJECTS;
    }

    int myTeamId = g_app->GetWorld()->m_myTeamId;
    if( m_teamId != myTeamId &&
        myTeamId != -1 &&
        myTeamId < g_app->GetWorld()->m_teams.Size() &&
        g_app->GetWorld()->GetTeam(myTeamId)->m_type != Team::TypeUnassigned )
    {
        maxSize = min( maxSize, 4 );
    }

    if( m_history.Size() > 0 )
    {
        Vector3<float> lastPos( predictedLongitude, predictedLatitude, 0 );

        for( int i = 0; i < maxSize; ++i )
        {
            Vector3<float> historyPos, thisPos;
			thisPos = historyPos = *m_history[i];

            if( lastPos.x < -170 && thisPos.x > 170 )       thisPos.x = -180 - ( 180 - thisPos.x );        
            if( lastPos.x > 170 && thisPos.x < -170 )       thisPos.x = 180 + ( 180 - fabs(thisPos.x) );        

            Vector3<float> diff = thisPos - lastPos;
            lastPos += diff * 0.1f;
            colour.m_a = 255 - 255 * (float) i / (float) maxSize;
            
            g_renderer->Line( lastPos.x, lastPos.y, thisPos.x, thisPos.y, colour, 2.0f );        
            lastPos = historyPos;
        }
    }
}


int MovingObject::GetAttackState()
{
    return -1;
}

bool MovingObject::IsIdle()
{
    if( m_targetLongitude == 0 &&
        m_targetLatitude  == 0 &&
        m_targetObjectId == -1)
        return true;
    else return false;
}


void MovingObject::RenderGhost( int teamId )
{
    if( m_lastSeenTime[teamId] != 0)
    {
        if( m_movementType == MovementTypeAir ||
            m_movementType == MovementTypeSea )
        {		
            Fixed predictionTime = m_ghostFadeTime - m_lastSeenTime[teamId];
            predictionTime += Fixed::FromDouble(g_predictionTime) * g_app->GetWorld()->GetTimeScaleFactor();
            float predictedLongitude = (m_lastKnownPosition[teamId].x + m_lastKnownVelocity[teamId].x * predictionTime).DoubleValue();
            float predictedLatitude = (m_lastKnownPosition[teamId].y + m_lastKnownVelocity[teamId].y * predictionTime).DoubleValue();

            float size = GetSize().DoubleValue();       
            float thisSize = size;

            int transparency = int(255 * ( m_lastSeenTime[teamId] / m_ghostFadeTime ).DoubleValue() );
            Colour col = Colour(150, 150, 150, transparency);
            float angle = 0.0f;

            if( m_movementType == MovementTypeAir )            
            {
                angle = atan( -m_lastKnownVelocity[teamId].x.DoubleValue() / m_lastKnownVelocity[teamId].y.DoubleValue() );
                if( m_lastKnownVelocity[teamId].y < 0 ) angle += M_PI;
                size *= 0.5f;
                thisSize *= 0.5f;
            }
            else
            {
                Team *team = g_app->GetWorld()->GetTeam(m_teamId);
                if( team->m_territories[0] >= 3 )
                {
                    thisSize = size*-1;
                }       
            }

            Image *bmpImage = g_resource->GetImage( bmpImageFilename );
            g_renderer->Blit( bmpImage, predictedLongitude, predictedLatitude, thisSize, size, col, angle);
        }
        else
        {
            WorldObject::RenderGhost( teamId );
        }
    }
}

void MovingObject::Land( int targetId )
{
}

void MovingObject::ClearWaypoints()
{
    m_targetLongitude = 0;
    m_targetLatitude = 0;
    m_finalTargetLongitude = 0;
    m_finalTargetLatitude = 0;
    m_targetLongitudeAcrossSeam = 0;
    m_targetLatitudeAcrossSeam = 0;
    m_targetNodeId = -1;
    m_isLanding = -1;
//    m_movementBlips.EmptyAndDelete();
}


int MovingObject::IsValidMovementTarget( Fixed longitude, Fixed latitude )
{
    if( latitude > 100 || latitude < -100 )
    {
        return TargetTypeInvalid;
    }

    //
    // Are we part of a fleet?

    Fleet *fleet = g_app->GetWorld()->GetTeam(m_teamId)->GetFleet( m_fleetId );
    if( fleet )
    {
        return( fleet->IsValidFleetPosition( longitude, latitude ) );
    }


    Fixed rangeSqd = g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, longitude, latitude );      
    if( rangeSqd > m_range * m_range )
    {
        return TargetTypeOutOfRange;
    }

    bool validPosition = IsValidPosition( longitude, latitude );

    if( validPosition )
    {
        return TargetTypeValid;
    }
    else
    {
        return TargetTypeInvalid;
    }
}


void MovingObject::AutoLand()
{
    if( m_isLanding == -1 )
    {
        int target = GetClosestLandingPad();

        if( target != -1 )
        {
            WorldObject *landingPad = g_app->GetWorld()->GetWorldObject(target);
            if( m_range - 15 < g_app->GetWorld()->GetDistance( m_longitude, m_latitude, landingPad->m_longitude, landingPad->m_latitude) )
            {
                Land( target );
            }
        }
    }
}


int MovingObject::GetClosestLandingPad()
{
    int nearestId = -1;
    int nearestWithNukesId = -1;

    Fixed nearestSqd = Fixed::MAX;
    Fixed nearestWithNukesSqd = Fixed::MAX;

    //
    // Look for any carrier or airbase with room
    // Favour objects with nukes if we are a bomber

    for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
    {
        if( g_app->GetWorld()->m_objects.ValidIndex(i) )
        {
            WorldObject *obj = g_app->GetWorld()->m_objects[i];
            if( obj->m_teamId == m_teamId )
            {
                if( obj->m_type == WorldObject::TypeCarrier ||
                    obj->m_type == WorldObject::TypeAirBase )
                {
                    int roomInside = 0;
                    if( m_type == TypeFighter ) roomInside = obj->m_maxFighters - obj->m_states[0]->m_numTimesPermitted;
                    if( m_type == TypeBomber ) roomInside = obj->m_maxBombers - obj->m_states[1]->m_numTimesPermitted;

                    if( roomInside > 0 )
                    {
                        Fixed distSqd = g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, obj->m_longitude, obj->m_latitude );
                        if( distSqd < nearestSqd )
                        {
                            nearestSqd = distSqd;
                            nearestId = obj->m_objectId;
                        }

                        if( m_type == TypeBomber && 
                            obj->m_nukeSupply > 0 &&
                            distSqd < nearestWithNukesSqd )
                        {
                            nearestWithNukesSqd = distSqd;
                            nearestId = obj->m_objectId;
                        }
                    }
                }
            }
        }
    }

    //
    // Fighter - go for nearest landing pad

    if( m_type == TypeFighter && 
        nearestId != -1 )
    {
        return nearestId;
    }

    //
    // Bomber - go for nearest with nukes if in range

    if( m_type == TypeBomber && 
        nearestWithNukesId != -1 &&
        nearestWithNukesSqd < m_range * m_range )
    {
        return nearestWithNukesId;
    }

    //
    // Bomber - no nukes within range, so go for nearest

    if( m_type == TypeBomber &&
        nearestId != -1 )
    {
        return nearestId;
    }


    return -1;
}


//int MovingObject::GetClosestLandingPad()
//{
//    int type = -1;
//    int cap = 0;
//    if( m_type == TypeFighter )
//    {
//        type = 0;
//    }
//    else if( m_type == TypeBomber )
//    {
//        type = 1;
//    }
//
//    float carrierDistance = 9999.9f;
//    float airbaseDistance = 9999.9f;
//    int nearestCarrier = -1;
//    int nearestAirbase = -1;
//
//    for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
//    {
//        if( g_app->GetWorld()->m_objects.ValidIndex(i) )
//        {
//            WorldObject *obj = g_app->GetWorld()->m_objects[i];
//            if( obj->m_teamId == m_teamId )
//            {
//                if( obj->m_type == WorldObject::TypeCarrier ||
//                    obj->m_type == WorldObject::TypeAirBase )
//                {
//                    if( type == 0 )
//                    {
//                        cap = obj->m_maxFighters;
//                    }
//                    else
//                    {
//                        cap = obj->m_maxBombers;
//                    }
//
//                    if( obj->m_states[type]->m_numTimesPermitted < cap )
//                    {
//                        float dist = g_app->GetWorld()->GetDistance( m_longitude, m_latitude, obj->m_longitude, obj->m_latitude );
//                        if( obj->m_type == WorldObject::TypeCarrier )
//                        {
//                            if( dist < carrierDistance )
//                            {
//                                carrierDistance = dist;
//                                nearestCarrier = obj->m_objectId;
//                            }
//                        }
//                        else
//                        {
//                            if( dist < airbaseDistance )
//                            {
//                                airbaseDistance = dist;
//                                nearestAirbase = obj->m_objectId;
//                            }
//                        }
//                    }
//                }
//            }
//        }
//    }
//
//    int target = -1;
//    float distance = 99999.9f;
//
//    WorldObject *base = g_app->GetWorld()->GetWorldObject(nearestAirbase);
//    if( base )
//    {
//        distance = g_app->GetWorld()->GetDistance( m_longitude, m_latitude, base->m_longitude, base->m_latitude );
//        target = nearestAirbase;
//    }
//
//    
//    WorldObject *carrier = g_app->GetWorld()->GetWorldObject(nearestCarrier);
//    if( carrier )
//    {
//        if( g_app->GetWorld()->GetDistance( m_longitude, m_latitude, carrier->m_longitude, carrier->m_latitude ) < distance &&
//            carrier->m_states[type]->m_numTimesPermitted < cap )
//        {
//            target = nearestCarrier;
//        }
//    }
//
//    return target;
//}

void MovingObject::Retaliate( int attackerId )
{
    WorldObject *obj = g_app->GetWorld()->GetWorldObject( attackerId );
    if( obj && !g_app->GetWorld()->IsFriend( m_teamId, attackerId ) &&
        !g_app->GetWorld()->GetTeam( m_teamId )->m_ceaseFire[ obj->m_teamId ])
    {
        if( UsingGuns() && m_targetObjectId == -1 )
        {            
            if( g_app->GetWorld()->GetAttackOdds( m_type, obj->m_type ) > 0 )
            {
                m_targetObjectId = attackerId;
                m_isRetaliating = true;
            }

            if( m_fleetId != -1 )
            {
                g_app->GetWorld()->GetTeam( m_teamId )->m_fleets[ m_fleetId ]->FleetAction( m_targetObjectId );
            }
        }
        if( obj->m_type == WorldObject::TypeSub )
        {
            Fleet *fleet = g_app->GetWorld()->GetTeam( m_teamId )->GetFleet( m_fleetId );
            if( fleet )
            {
                fleet->CounterSubs();
            }
        }
    }

}

void MovingObject::Ping()
{   
    int animId = g_app->GetMapRenderer()->CreateAnimation( MapRenderer::AnimationTypeSonarPing, m_objectId,
														   m_longitude.DoubleValue(), m_latitude.DoubleValue() );
    AnimatedIcon *icon = g_app->GetMapRenderer()->m_animations[animId];
    ((SonarPing *)icon)->m_teamId = m_teamId;
    
    if( g_app->GetWorld()->m_myTeamId == -1 ||
        g_app->GetWorld()->m_myTeamId == m_teamId ||
        g_app->GetMapRenderer()->m_renderEverything ||
        g_app->GetWorld()->IsVisible( m_longitude, m_latitude, g_app->GetWorld()->m_myTeamId) )
    {
        g_soundSystem->TriggerEvent( SoundObjectId(m_objectId), "SonarPing" );
    }
    

    Fleet *fleet = g_app->GetWorld()->GetTeam( m_teamId )->GetFleet( m_fleetId );
    if( fleet == NULL ||
        fleet->m_currentState == Fleet::FleetStateAggressive )
    {
        for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
        {
            if( g_app->GetWorld()->m_objects.ValidIndex(i) )
            {
                MovingObject *obj = (MovingObject *)g_app->GetWorld()->m_objects[i];
                if( obj->IsMovingObject() &&
                    obj->m_movementType == MovementTypeSea &&
                    obj->m_objectId != m_objectId &&
                    !obj->m_visible[ m_teamId ] &&
                    m_teamId != obj->m_teamId )
                {
                    Fixed distanceSqd = g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, obj->m_longitude, obj->m_latitude );

                    if( distanceSqd <= GetActionRangeSqd() )
                    {
                        if( obj->m_movementType == MovementTypeSea )
                        {
                            for( int j = 0; j < g_app->GetWorld()->m_teams.Size(); ++j )
                            {
                                Team *team = g_app->GetWorld()->m_teams[j];
                                if( g_app->GetWorld()->IsFriend(team->m_teamId, m_teamId ) )
                                {
                                    obj->m_lastSeenTime[team->m_teamId] = obj->m_ghostFadeTime;
                                    obj->m_lastKnownPosition[team->m_teamId] = Vector3<Fixed>( obj->m_longitude, obj->m_latitude, 0 );                                    
                                    //obj->m_visible[team->m_teamId] = (obj->m_type != WorldObject::TypeSub );
                                    obj->m_seen[team->m_teamId] = true;
                                    obj->m_lastKnownVelocity[team->m_teamId].Zero();
                                }
                            }
                        }

                        if( m_type == WorldObject::TypeSub )
                        {
                            for( int j = 0; j < g_app->GetWorld()->m_teams.Size(); ++j )
                            {
                                Team *team = g_app->GetWorld()->m_teams[j];
                                if( g_app->GetWorld()->IsVisible( m_longitude, m_latitude, team->m_teamId ) )
                                {
                                    m_seen[team->m_teamId] = true;
                                    m_lastSeenTime[team->m_teamId] = m_ghostFadeTime;
                                    m_lastKnownPosition[team->m_teamId] = Vector3<Fixed>( m_longitude, m_latitude, 0 );
                                    m_lastKnownVelocity[team->m_teamId].Zero();
                                }
                            }
                        }

                        if( !g_app->GetWorld()->GetTeam( m_teamId )->m_ceaseFire[ obj->m_teamId ] )
                        {
                            if( m_targetObjectId == -1 )
                            {
                                m_targetObjectId = obj->m_objectId;                  
                            }
                            if( g_app->GetWorld()->GetTeam( m_teamId )->m_type == Team::TypeAI )
                            {
                                if( obj->m_type == WorldObject::TypeSub )
                                {
                                    g_app->GetWorld()->GetTeam( m_teamId )->AddEvent( Event::TypeSubDetected, obj->m_objectId, obj->m_teamId, obj->m_fleetId, obj->m_longitude, obj->m_latitude );
                                }
                                else
                                {
                                    g_app->GetWorld()->GetTeam( m_teamId )->AddEvent( Event::TypeEnemyIncursion, obj->m_objectId, obj->m_teamId, obj->m_fleetId, obj->m_longitude, obj->m_latitude );
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

void MovingObject::SetSpeed( Fixed speed )
{
    m_speed = speed;
}

int MovingObject::GetTarget( Fixed range )
{
    LList<int> farTargets;
    LList<int> closeTargets;
    for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
    {
        if( g_app->GetWorld()->m_objects.ValidIndex(i) )
        {
            WorldObject *obj = g_app->GetWorld()->m_objects[i];
            if( obj->m_teamId != TEAMID_SPECIALOBJECTS )
            {
                if( !g_app->GetWorld()->IsFriend( obj->m_teamId, m_teamId ) &&
                    g_app->GetWorld()->GetAttackOdds( m_type, obj->m_type ) > 0 &&
                    obj->m_visible[m_teamId] &&
                    !g_app->GetWorld()->GetTeam( m_teamId )->m_ceaseFire[ obj->m_teamId ] )
                {
                    Fixed distanceSqd = g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, obj->m_longitude, obj->m_latitude );
                    if( distanceSqd < GetActionRangeSqd() )
                    {
                        closeTargets.PutData(obj->m_objectId );
                    }
                    else if( distanceSqd < range * range )
                    {
                        farTargets.PutData( obj->m_objectId );
                    }
                }
            }
        }
    }
    if( closeTargets.Size() > 0 )
    {
        int targetIndex = syncrand() % closeTargets.Size();
        return closeTargets[ targetIndex ];
    }
    else if( farTargets.Size() > 0 )
    {
        int targetIndex = syncrand() % farTargets.Size();
        return farTargets[ targetIndex ];
    }
    return -1;
}


char *MovingObject::LogState()
{
    char longitude[64];
    char latitude[64];
    char velX[64];
    char velY[64];
    char timer[64];
    char retarget[64];
    char ai[64];
    char speed[64];
    char targetLong[64];
    char targetLat[64];
    char seenTime[64];

    static char s_result[10240];
    snprintf( s_result, 10240, "obj[%d] [%10s] team[%d] fleet[%d] long[%s] lat[%s] velX[%s] velY[%s] state[%d] target[%d] life[%d] timer[%s] retarget[%s] ai[%s] speed[%s] targetNode[%d] targetLong[%s] targetLat[%s]",
                m_objectId,
                GetName(m_type),
                m_teamId,
                m_fleetId,
                HashDouble( m_longitude.DoubleValue(), longitude ),
                HashDouble( m_latitude.DoubleValue(), latitude ),
                HashDouble( m_vel.x.DoubleValue(), velX ),
                HashDouble( m_vel.y.DoubleValue(), velY ),
                m_currentState,
                m_targetObjectId,
                m_life,
                HashDouble( m_stateTimer.DoubleValue(), timer ),
                HashDouble( m_retargetTimer.DoubleValue(), retarget ),
                HashDouble( m_aiTimer.DoubleValue(), ai ),
                HashDouble( m_speed.DoubleValue(), speed ),
                m_targetNodeId,
                HashDouble( m_targetLongitude.DoubleValue(), targetLong ),
                HashDouble( m_targetLatitude.DoubleValue(), targetLat ) );

    for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
    {
        Team *team = g_app->GetWorld()->m_teams[i];
        char thisTeam[512];
        sprintf( thisTeam, "\n\tTeam %d visible[%d] seen[%d] pos[%s %s] vel[%s %s] seen[%s] state[%d]",
            team->m_teamId,
            m_visible[team->m_teamId],
            m_seen[team->m_teamId],
            HashDouble( m_lastKnownPosition[team->m_teamId].x.DoubleValue(), longitude ),
            HashDouble( m_lastKnownPosition[team->m_teamId].y.DoubleValue(), latitude ),
            HashDouble( m_lastKnownVelocity[team->m_teamId].x.DoubleValue(), velX ),
            HashDouble( m_lastKnownVelocity[team->m_teamId].y.DoubleValue(), velY ),
            HashDouble( m_lastSeenTime[team->m_teamId].DoubleValue(), seenTime ),
            m_lastSeenState[team->m_teamId] );

        strcat( s_result, thisTeam );
    }

    return s_result;
}

