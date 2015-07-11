#include "lib/universal_include.h"
#include "lib/language_table.h"
#include "lib/sound/soundsystem.h"
#include "lib/math/math_utils.h"

#include <math.h>

#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/render/renderer.h"
#include "lib/math/vector3.h"
#include "lib/math/random_number.h"
#include "lib/profiler.h"

#include "app/app.h"
#include "app/globals.h"

#include "renderer/map_renderer.h"

#include "world/world.h"
#include "world/nuke.h"
#include "world/team.h"
#include "world/city.h"
#include "world/bomber.h"


Nuke::Nuke()
:   MovingObject(),
    m_prevDistanceToTarget( Fixed::MAX ),
    m_newLongitude(0),
    m_newLatitude(0),
	m_isMirv(false),
    m_targetLocked(false)
{
    SetType( TypeNuke );

    strcpy( bmpImageFilename, "graphics/nuke.bmp" );

    m_radarRange = 2;
    m_speed = Fixed::Hundredths(30); //speed up
    m_selectable = true;
    m_maxHistorySize = -1;
    m_range = Fixed::MAX;
    m_turnRate = Fixed::Hundredths(1);
    m_movementType = MovementTypeAir;

    AddState( LANGUAGEPHRASE("state_ontarget"), 0, 0, 0, Fixed::MAX, false );
    AddState( LANGUAGEPHRASE("state_disarm"), 10, 0, 0, 0, false );

	//AddState( LANGUAGEPHRASE("state_splitmirv"), 1, 0, 0, 0, false );

	
	AddState( LANGUAGEPHRASE("state_splitmirv"), 1, 0, 0, 0, false );
	

}


void Nuke::Action( int targetObjectId, Fixed longitude, Fixed latitude )
{
    //m_newLongitude = longitude;
    //m_newLatitude = latitude;
}

void Nuke::SetWaypoint( Fixed longitude, Fixed latitude )
{
    if( m_targetLocked )
    {
        return;
    }
    MovingObject::SetWaypoint( longitude, latitude );

    Vector3<Fixed> target( m_targetLongitude, m_targetLatitude, 0 );
    Vector3<Fixed> pos( m_longitude, m_latitude, 0 );
    m_totalDistance = (target - pos).Mag();

    if( m_targetLongitude >= m_longitude )
    {
        m_curveDirection = 1;
    }
    else
    {
        m_curveDirection = -1;
    }
}


bool Nuke::Update()
{
    //
    // Are we disarmed?

    if( m_currentState == 1 && m_stateTimer <= 0 )
    {
        if( m_teamId == g_app->GetWorld()->m_myTeamId )
        {
            char message[64];
            sprintf( message, LANGUAGEPHRASE("message_disarmed") );
            g_app->GetWorld()->AddWorldMessage( m_longitude, m_latitude, m_teamId, message, WorldMessage::TypeObjectState );
        }
        return true;
    }


    //
    // Is our waypoint changing?

    Fixed timePerUpdate = SERVER_ADVANCE_PERIOD * g_app->GetWorld()->GetTimeScaleFactor();
    
    if( m_newLongitude != 0 || m_newLatitude != 0 )
    {
        Fixed factor1 = m_turnRate * timePerUpdate * 2;
        Fixed factor2 = 1 - factor1;
        m_targetLongitude = ( m_newLongitude * factor1 ) + ( m_targetLongitude * factor2 );
        m_targetLatitude = ( m_newLatitude * factor1 ) + ( m_targetLatitude * factor2 );

        if( ( m_targetLongitude - m_newLongitude ).abs() < 1 &&
			( m_targetLatitude - m_newLatitude ).abs() < 1 )
        {
            SetWaypoint( m_newLongitude, m_newLatitude );
            m_newLongitude = 0;
            m_newLatitude = 0;
        }
    }


    //
    // Move towards target

    Vector3<Fixed> target( m_targetLongitude, m_targetLatitude, 0 );
    Vector3<Fixed> pos( m_longitude, m_latitude, 0 );
    Fixed remainingDistance = (target - pos).Mag();
    Fixed fractionDistance = 1 - remainingDistance / m_totalDistance;

    Vector3<Fixed> front = (target - pos).Normalise();  
    Fixed fractionNorth = 5 * m_latitude.abs() / ( 200 / 2 );
    fractionNorth = max( fractionNorth, 3 );
    
    front.RotateAroundZ( Fixed::PI / (fractionNorth * m_curveDirection) );
    front.RotateAroundZ( fractionDistance * (-m_curveDirection * Fixed::PI/fractionNorth) );

    if( pos.y > 85 )
    {
        // We are dangerously far north
        // Make sure we dont go off the top of the world
        Fixed extremeFractionNorth = (pos.y - 85) / 15;
        Clamp( extremeFractionNorth, Fixed(0), Fixed(1) );
        front.y *= ( 1 - extremeFractionNorth );
        front.Normalise();
    }

    m_vel = Vector3<Fixed>(front * (m_speed/2 + m_speed/2 * fractionDistance * fractionDistance));
       
    Fixed newLongitude = m_longitude + m_vel.x * timePerUpdate;
    Fixed newLatitude = m_latitude + m_vel.y * timePerUpdate;
    Fixed newDistance = g_app->GetWorld()->GetDistance( newLongitude, newLatitude, m_targetLongitude, m_targetLatitude);

    if( newLongitude <= -180 ||
        newLongitude >= 180 )
    {
        m_longitude = newLongitude;
        CrossSeam();

        newLongitude = m_longitude;
        newDistance = g_app->GetWorld()->GetDistance( newLongitude, newLatitude, m_targetLongitude, m_targetLatitude );
    }

    if( newDistance < 2 &&
        newDistance >= remainingDistance )
    {
        m_targetLongitude = 0;
        m_targetLatitude = 0;
        m_vel.Zero();
        g_app->GetWorld()->CreateExplosion( m_teamId, m_longitude, m_latitude, 100 );
        g_soundSystem->TriggerEvent( SoundObjectId(m_objectId), "Detonate" );
        return true;
    }
    else
    {
        m_range -= Vector3<Fixed>( m_vel.x * Fixed(timePerUpdate), m_vel.y * Fixed(timePerUpdate), 0 ).Mag();
        if( m_range <= 0 )
        {
            m_life = 0;
			m_lastHitByTeamId = -1;
            g_app->GetWorld()->AddOutOfFueldMessage( m_objectId );
        }
        m_longitude = newLongitude;
        m_latitude = newLatitude;
    }

	if (m_isMirv)
	{
		SetMirv(true);
	}

	//if (m_isMirv && fractionDistance.DoubleValue() > 0.5) {
	if (m_isMirv && ( (fractionDistance.DoubleValue() > 0.6) || (m_currentState == 2) )   ) {
		m_currentState == 0;
		SetMirv(false);

		Fixed target_long[9];
		Fixed target_lat[9];
		LList<City *> viableCities;
		City *originalTarget = GetTargettedCity();
		WorldObject *originalTargetObject = GetTargettedObject();
		Fixed mirv_range = Fixed::FromDouble(1); // Change this value to change the range of the MIRV warheads

		// Find target cities that aren't us or our allies
		for ( int i = 0; i < g_app->GetWorld()->m_cities.Size(); ++i )
		{
			if ( g_app->GetWorld()->m_cities.ValidIndex(i) )
			{
				City *city = g_app->GetWorld()->m_cities[i];
				
				if ((!g_app->GetWorld()->IsFriend( city->m_teamId, m_teamId) || ( originalTargetObject != NULL && originalTargetObject->m_teamId == city->m_teamId)) && 
					g_app->GetWorld()->GetDistanceSqd( city->m_longitude, city->m_latitude, m_longitude, m_latitude, true) <= m_range * m_range ) // 1.5 is the range of the mirv warhead
				{
					InsertIntoCityListOrderd(&viableCities, city);
				}
			}
		}
		// Put orginal target at the end of the list as a fallback if no other cities can be found
		if (originalTarget) // check the nuke was actually targetting a city
		{
			viableCities.PutDataAtEnd(originalTarget);
		}

		if (viableCities.Size() > 0) {
			for ( int m = 0; m < 9; ++m )
			{
				target_long[m] = viableCities.GetData(m % (viableCities.Size()))->m_longitude;
				target_lat[m] = viableCities.GetData(m % (viableCities.Size()))->m_latitude;
			}
		} else { // If there's still no viable cities to hit just fire everything at the original target
			for ( int m = 0; m < 9; ++m )
			{
				target_long[m] = m_targetLongitude;
				target_lat[m] = m_targetLatitude;
			}
		}

		for ( int w = 0; w < 9; ++w ) {
			g_app->GetWorld()->LaunchNuke( m_teamId, m_objectId, target_long[w] , target_lat[w], this->m_range ); 
		}
	}

    return MovingObject::Update();
}


WorldObject *Nuke::GetTargettedObject()
{
	int targetID = g_app->GetWorld()->GetNearestObject(m_teamId, m_targetLongitude, m_targetLatitude);
	return g_app->GetWorld()->GetWorldObject(targetID);
}

City *Nuke::GetTargettedCity()
{
	for ( int i = 0; i < g_app->GetWorld()->m_cities.Size(); ++i )
	{
		if ( g_app->GetWorld()->m_cities.ValidIndex(i) )
		{
			City *city = g_app->GetWorld()->m_cities[i];

			if (city->m_objectId == this->m_targetObjectId) {
				return city;
			}
		}
	}

	return NULL;
}

void Nuke::InsertIntoCityListOrderd(LList<City *> *llist, City *city)
{
	if (llist->Size() > 0) {
		for (int insertAt = 0; insertAt < llist->Size(); ++insertAt) {
			City *compareCity;

			compareCity = llist->GetData(insertAt);

			Fixed curDistance =  g_app->GetWorld()->GetDistanceSqd( city->m_longitude, city->m_latitude, m_targetLongitude, m_targetLatitude, true);
			Fixed nextDistance;

			if (insertAt+1 < llist->Size())
			{
				compareCity = llist->GetData(insertAt+1);
				nextDistance = g_app->GetWorld()->GetDistanceSqd( compareCity->m_longitude, compareCity->m_latitude, m_targetLongitude, m_targetLatitude, true);
			} else {
				nextDistance = Fixed::MAX;
			}

			if (curDistance < nextDistance) {
				llist->PutDataAtIndex(city, insertAt);
				break;
			}
		}
	} else {
		llist->PutData(city);
	}
}

void Nuke::Render()
{
    MovingObject::Render();
}

void Nuke::SetMirv(bool isMirv)
{
	m_isMirv = isMirv;

	if (m_isMirv)
		strcpy( bmpImageFilename, "graphics/mirv.bmp" );
	else
		strcpy( bmpImageFilename, "graphics/nuke.bmp" );
}

void Nuke::FindTarget( int team, int targetTeam, int launchedBy, Fixed range, Fixed *longitude, Fixed *latitude )
{
    int objectId = -1;
    Nuke::FindTarget( team, targetTeam, launchedBy, range, longitude, latitude, &objectId );
}

void Nuke::FindTarget( int team, int targetTeam, int launchedBy, Fixed range, Fixed *longitude, Fixed *latitude, int *objectId )
{
    START_PROFILE("Nuke::FindTarget");
    
    WorldObject *launcher = g_app->GetWorld()->GetWorldObject(launchedBy);
    if( !launcher ) 
    {
        END_PROFILE("Nuke::FindTarget");
        return;
    }

    LList<int> validTargets;
        
    for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
    {
        if( g_app->GetWorld()->m_objects.ValidIndex(i) )
        {
            WorldObject *obj = g_app->GetWorld()->m_objects[i];
            if( obj->m_teamId == targetTeam &&
                obj->m_seen[team] &&
                !obj->IsMovingObject() )
            {
                Fixed distanceSqd = g_app->GetWorld()->GetDistanceSqd( launcher->m_longitude, launcher->m_latitude, obj->m_longitude, obj->m_latitude);
                if( distanceSqd <= range * range )
                {                    
                    int numTargetedNukes = CountTargetedNukes( team, obj->m_longitude, obj->m_latitude );
                    
                    if( (obj->m_type == WorldObject::TypeRadarStation && numTargetedNukes < 2 ) ||
                        (obj->m_type != WorldObject::TypeRadarStation && numTargetedNukes < 4 ) )
                    {
                        validTargets.PutData(obj->m_objectId);
                    }
                }
            }
        }
    }

    if( validTargets.Size() > 0 )
    {
        int targetId = syncrand() % validTargets.Size();
        int objIndex = validTargets[ targetId ];
        WorldObject *obj = g_app->GetWorld()->GetWorldObject(objIndex);
        if( obj )
        {
            *longitude = obj->m_longitude;
            *latitude = obj->m_latitude;
            *objectId = obj->m_objectId;
            END_PROFILE("Nuke::FindTarget");
            return;
        }
    }

    Team *friendlyTeam = g_app->GetWorld()->GetTeam( team );

    int maxPop = 500000;        // Don't bother hitting cities with less than 0.5M survivors

    for( int i = 0; i < g_app->GetWorld()->m_cities.Size(); ++i )
    {
        if( g_app->GetWorld()->m_cities.ValidIndex(i) )
        {
            City *city = g_app->GetWorld()->m_cities[i];

            if( !g_app->GetWorld()->IsFriend( city->m_teamId, team) && 
				g_app->GetWorld()->GetDistanceSqd( city->m_longitude, city->m_latitude, launcher->m_longitude, launcher->m_latitude) <= range * range)               
            {
                int numTargetedNukes = CountTargetedNukes(team, city->m_longitude, city->m_latitude);
                int estimatedPop = City::GetEstimatedPopulation( team, i, numTargetedNukes );
                if( estimatedPop > maxPop )
                {
                    maxPop = estimatedPop;
                    *longitude = city->m_longitude;
                    *latitude = city->m_latitude;
                    *objectId = -1;
                }
            }
        }
    }
    
    END_PROFILE("Nuke::FindTarget");
}

int Nuke::CountTargetedNukes( int teamId, Fixed longitude, Fixed latitude )
{
    int targetedNukes = 0;
    for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
    {
        if( g_app->GetWorld()->m_objects.ValidIndex(i) )
        {
            if( g_app->GetWorld()->m_objects[i]->m_type == WorldObject::TypeNuke )
            {
                MovingObject *obj = (MovingObject *)g_app->GetWorld()->m_objects[i];
                Fixed targetLongitude = obj->m_targetLongitude;
                if( targetLongitude > 180 )
                {
                    targetLongitude -= 360;
                }
                else if( targetLongitude < -180 )
                {
                    targetLongitude += 360;
                }

                if( obj->m_teamId == teamId &&
                    targetLongitude == longitude &&
                    obj->m_targetLatitude == latitude )
                {
                    ++targetedNukes;
                }
            }
            else if( g_app->GetWorld()->m_objects[i]->m_type == WorldObject::TypeBomber )
            {
                Bomber *obj = (Bomber *)g_app->GetWorld()->m_objects[i];
                if( obj->m_teamId == teamId &&
                    obj->m_nukeTargetLongitude == longitude &&
                    obj->m_nukeTargetLatitude == latitude )
                {
                    ++targetedNukes;
                }
            }
            else if( g_app->GetWorld()->m_objects[i]->m_type == WorldObject::TypeSub ||
                     g_app->GetWorld()->m_objects[i]->m_type == WorldObject::TypeSilo)
            {
                WorldObject *obj = g_app->GetWorld()->m_objects[i];
                int nukeState = 0;
                if( obj->m_type == WorldObject::TypeSub ) 
                {
                    nukeState = 2;
                }
                if( obj->m_teamId == teamId &&
                    obj->m_currentState == nukeState )
                {
                    for( int j = 0; j < obj->m_actionQueue.Size(); ++j )
                    {
                        if( obj->m_actionQueue[j]->m_longitude == longitude &&
                            obj->m_actionQueue[j]->m_latitude == latitude )
                        {
                            targetedNukes++;
                        }
                    }
                }
            }
            else if( g_app->GetWorld()->m_objects[i]->m_type == WorldObject::TypeAirBase ||
                     g_app->GetWorld()->m_objects[i]->m_type == WorldObject::TypeCarrier )
            {
                WorldObject *obj = g_app->GetWorld()->m_objects[i];
                if( obj->m_teamId == teamId &&
                    obj->m_currentState == 1 )
                {
                    for( int j = 0; j < obj->m_actionQueue.Size(); ++j )
                    {
                        if( obj->m_actionQueue[j]->m_longitude == longitude &&
                            obj->m_actionQueue[j]->m_latitude == latitude )
                        {
                            targetedNukes++;
                        }
                    }
                }
            }
        }
    }
    return targetedNukes;
}


void Nuke::CeaseFire( int teamId )
{
    if( g_app->GetMapRenderer()->IsValidTerritory( teamId, m_targetLongitude, m_targetLatitude, false ) )
    {
        SetState(1);
    }
}

int Nuke::IsValidMovementTarget( Fixed longitude, Fixed latitude )
{
    return TargetTypeInvalid;
}

void Nuke::LockTarget()
{
    m_targetLocked = true;
}
