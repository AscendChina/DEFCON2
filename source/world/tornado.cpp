#include "lib/universal_include.h"

#include <math.h>

#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/render/renderer.h"
#include "lib/math/vector3.h"
#include "lib/math/random_number.h"

#include "app/app.h"
#include "app/globals.h"

#include "renderer/map_renderer.h"

#include "interface/interface.h"

#include "world/world.h"
#include "world/tornado.h"
#include "world/team.h"
#include "world/nuke.h"


Tornado::Tornado()
:   MovingObject(),
    m_targetObjectId(-1)
{
    SetType( TypeTornado );

    strcpy( bmpImageFilename, "graphics/tornado.bmp" );

    m_radarRange = 0;
    m_speed = Fixed::Hundredths(10);
    m_turnRate = Fixed::Hundredths(4);
    m_selectable = true;
    m_maxHistorySize = 10;
    m_range = 600;
	m_angle = 0.0f;
    m_movementType = MovementTypeAir;
	AddState( "WOOSH", 0, 0, 5, Fixed::MAX, false );

    InitialiseTimers();
}


void Tornado::Action( int targetObjectId, float longitude, float latitude )
{
    
}

bool Tornado::Update()
{   
	for(int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i)
	{
		if( g_app->GetWorld()->m_objects.ValidIndex(i) )
		{
			if( g_app->GetWorld()->m_objects[i]->m_type == WorldObject::TypeNuke )
			{
				Nuke *nuke = (Nuke *)g_app->GetWorld()->m_objects[i];
				Fixed distance = g_app->GetWorld()->GetDistance( m_longitude, m_latitude, nuke->m_longitude, nuke->m_latitude);
				if( distance <= GetActionRange() )
				{
					Fixed targetLongitude = syncsfrand(360);
					Fixed targetLatitude = syncsfrand(180);  
					nuke->SetWaypoint(targetLongitude, targetLatitude);
				}
			}
			else if( g_app->GetWorld()->m_objects[i]->m_type == WorldObject::TypeTornado )
			{
				if( m_size < 40 )
				{
					Tornado *tornado = (Tornado *)g_app->GetWorld()->m_objects[i];
					Fixed distance = g_app->GetWorld()->GetDistance( m_longitude, m_latitude, tornado->m_longitude, tornado->m_latitude);
					if( distance <= GetActionRange() )
					{
						if( tornado->m_objectId != m_objectId )
						{
							if( tornado->GetSize() > m_size )
							{
								tornado->SetSize( tornado->GetSize() + Fixed::Hundredths(20) );
								m_size -= Fixed::Hundredths(20);
							}
						}
					}
				}
			}
		}
	}

    bool arrived = MoveToWaypoint();
    if( arrived )
    {
        GetNewTarget();
    }

	m_size	-= Fixed::FromDouble(0.001f);

    bool amIDead = MovingObject::Update();  
	
	if( m_size < Fixed::Hundredths(50) ) amIDead = true;

    return amIDead;
}

Fixed Tornado::GetActionRange()
{
    return m_size / Fixed::FromDouble(1.5f);
}


void Tornado::Render()
{
    Fixed predictionTime = Fixed::FromDouble(g_predictionTime) * g_app->GetWorld()->GetTimeScaleFactor();
    float predictedLongitude = ( m_longitude + m_vel.x * predictionTime ).DoubleValue();
    float predictedLatitude = ( m_latitude + m_vel.y * predictionTime ).DoubleValue(); 

    Colour colour       = COLOUR_SPECIALOBJECTS;         

    m_angle += 0.05f;

    Image *bmpImage = g_resource->GetImage( bmpImageFilename );
    g_renderer->Blit( bmpImage, predictedLongitude + m_vel.x.DoubleValue() * 10,
					  predictedLatitude + m_vel.y.DoubleValue() * 10, m_size.DoubleValue()/2, m_size.DoubleValue()/2,
					  colour, m_angle );
}

Fixed Tornado::GetSize()
{
	return m_size;
}

void Tornado::SetSize( Fixed size )
{
	m_size = size;
}

void Tornado::GetNewTarget()
{
    Fixed targetLongitude = syncsfrand(360);
	Fixed targetLatitude = syncsfrand(180);  
    SetWaypoint( targetLongitude, targetLatitude );
}
