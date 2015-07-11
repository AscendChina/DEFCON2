#include "lib/universal_include.h"
    
#include <math.h>

#include "lib/resource/resource.h"
#include "lib/resource/image.h"
#include "lib/render/renderer.h"
#include "lib/math/vector3.h"
#include "lib/math/random_number.h"
#include "lib/sound/soundsystem.h"

#include "app/app.h"
#include "app/globals.h"

#include "renderer/map_renderer.h"

#include "world/world.h"
#include "world/explosion.h"
#include "world/depthcharge.h"
#include "world/gunfire.h"
#include "world/team.h"
#include "world/city.h"


DepthCharge::DepthCharge( Fixed range )
:   GunFire( range ),
    m_timer(40)
{
    m_speed = 0;
    m_turnRate = Fixed::Hundredths(80);
    m_maxHistorySize = -1;
    m_movementType = MovementTypeSea;
    
    strcpy( bmpImageFilename, "graphics/depthcharge.bmp" );
}

bool DepthCharge::Update()
{
    m_timer -= g_app->GetWorld()->GetTimeScaleFactor() * SERVER_ADVANCE_PERIOD;
    if( m_timer < 0 ) m_timer = 0;

    return (m_timer == 0);
}

void DepthCharge::Render()
{
    float predictedTimer = m_timer.DoubleValue() - g_app->GetWorld()->GetTimeScaleFactor().DoubleValue() * g_predictionTime;

    float size = (predictedTimer+30)/60.0f;
    if( g_app->GetMapRenderer()->GetZoomFactor() <=0.25f )
    {
        size *= g_app->GetMapRenderer()->GetZoomFactor() * 4;
    }
    Colour colour = g_app->GetWorld()->GetTeam( m_teamId )->GetTeamColour();

    colour.m_a = 200 * ( predictedTimer / 40.0f );
    colour.m_a = max(colour.m_a, (unsigned char) 50);
    
    float angle = sinf(g_gameTime*1.5f) * 0.2f;
    Image *bmpImage = g_resource->GetImage( bmpImageFilename );
    g_renderer->Blit( bmpImage, m_longitude.DoubleValue(), m_latitude.DoubleValue(), size, size, colour, angle);
}

void DepthCharge::Impact()
{
    Explosion *explosion = new Explosion();
    explosion->SetTeamId( m_teamId );
    explosion->SetPosition( m_longitude, m_latitude );
    explosion->m_intensity = 100;
    explosion->m_underWater = true;
    for( int i = 0; i < g_app->GetWorld()->m_teams.Size(); ++i )
    {
        Team *team = g_app->GetWorld()->m_teams[i];
        explosion->m_visible[team->m_teamId] = true;
    }
    int expId = g_app->GetWorld()->m_explosions.PutData( explosion );
    explosion->m_objectId = expId;

    g_soundSystem->TriggerEvent( "Object_carrier", "DepthCharge", Vector3<Fixed>(m_longitude, m_latitude,0) );

    for( int i = 0; i < g_app->GetWorld()->m_objects.Size(); ++i )
    {
        if( g_app->GetWorld()->m_objects.ValidIndex(i) )
        {
            WorldObject *wobj = g_app->GetWorld()->m_objects[i];

            if( wobj->m_type == WorldObject::TypeSub &&
                wobj->m_teamId != m_teamId )
            {
                Fixed rangeSqd = g_app->GetWorld()->GetDistanceSqd( m_longitude, m_latitude, wobj->m_longitude, wobj->m_latitude);
                if( rangeSqd <= 5 * 5 )
                {
                    if( wobj->m_currentState == 2 )
                    {
                        m_attackOdds = 0;
                    }
                    Fixed factor = 2 - rangeSqd/(5 * 5);
                    int odds = (m_attackOdds * factor).IntValue();
                    if( odds > syncrand() % 100 )
                    {
                        wobj->m_life = 0;  
						wobj->m_lastHitByTeamId = m_teamId;
                    }
                }
            }
        }
    }
}
