#include "lib/universal_include.h"
#include "lib/render/colour.h"
#include "lib/render/renderer.h"
#include "lib/math/math_utils.h"
#include "lib/hi_res_time.h"

#include "app/app.h"
#include "app/globals.h"

#include "lib/resource/resource.h"
#include "lib/gucci/input.h"

#include "renderer/map_renderer.h"
#include "renderer/animated_icon.h"

 

AnimatedIcon::AnimatedIcon()
:   m_longitude(0.0f),
    m_latitude(0.0f),
    m_fromObjectId(-1)
{
    m_visible.Initialise(MAX_TEAMS);
    m_visible.SetAll( false );

    m_startTime = GetHighResTime();
}

bool AnimatedIcon::Render()
{
    return true;
}


// ============================================================================


ActionMarker::ActionMarker()
:   AnimatedIcon(),
    m_combatTarget(false),
    m_targetType(WorldObject::TargetTypeInvalid)
{
}

bool ActionMarker::Render()
{
    float timePassed = GetHighResTime() - m_startTime;
    float size = 4.0 - timePassed * 8.0f;
    size = max( size, 0.0f );
    float rotation = g_gameTime * -4;
    
    Colour col = Colour(0,0,255,255);
    if( m_combatTarget ) col.Set(255,0,0,255);

    if( m_targetType >= WorldObject::TargetTypeValid )
    {
        WorldObject *obj = g_app->GetWorld()->GetWorldObject( m_fromObjectId );
        if( obj )
        {
            Colour lineCol = col;
            int thisAlpha = size * 64;
            Clamp( thisAlpha, 0, 255 );
            lineCol.m_a = thisAlpha;
            g_app->GetMapRenderer()->RenderActionLine( obj->m_longitude.DoubleValue(), obj->m_latitude.DoubleValue(), m_longitude, m_latitude, lineCol, 2.0f, false );
        }

        Image *img = g_resource->GetImage( "graphics/cursor_target.bmp");
        g_renderer->Blit( img, m_longitude, m_latitude, size, size, col, rotation);
    
        if( m_targetType > WorldObject::TargetTypeValid )
        {
            img = NULL;
            switch( m_targetType )
            {
                case WorldObject::TargetTypeLaunchFighter:      img = g_resource->GetImage( "graphics/fighter.bmp" );       break;
                case WorldObject::TargetTypeLaunchBomber:       img = g_resource->GetImage( "graphics/bomber.bmp" );        break;
                case WorldObject::TargetTypeLaunchNuke:         img = g_resource->GetImage( "graphics/nuke.bmp" );          break;
            }

            if( img )
            {
                g_renderer->Blit( img, m_longitude, m_latitude, size/2, size/2, col, 0 );
            }
        }
    }

    return ( timePassed > 0.5f );
}


// ============================================================================


SonarPing::SonarPing()
:   AnimatedIcon()
{
}
            
bool SonarPing::Render()
{
    float timePassed = GetHighResTime() - m_startTime;
    float size = timePassed * 2.5f;
    Clamp( size, 0.0f, 5.0f );

    if( g_app->GetWorld()->m_myTeamId == -1 ||
        m_visible[ g_app->GetWorld()->m_myTeamId ] )
    {
        int alpha = 255 - 255 * (size / 5.0f);
        alpha *= 0.5f;
        g_renderer->Circle( m_longitude, m_latitude, size, 40, Colour(255,255,255,alpha), 3.0f );
    }

    return( size == 5.0f );
}


// ============================================================================


NukePointer::NukePointer()
:   AnimatedIcon(),
    m_lifeTime(10.0f),
    m_targetId(-1),
    m_mode(0),
    m_offScreen(false),
    m_seen(false)
{
}


void NukePointer::Merge()
{
    //
    // Delete all nuke pointers currently at this location
    // (this new one will take over now)

    for( int i = 0; i < g_app->GetMapRenderer()->m_animations.Size(); ++i )
    {
        if( g_app->GetMapRenderer()->m_animations.ValidIndex(i) )
        {
            NukePointer *anim = (NukePointer *)g_app->GetMapRenderer()->m_animations[i];
            if( anim->m_animationType == MapRenderer::AnimationTypeNukePointer &&
                anim != this &&
                anim->m_targetId == m_targetId )
            {
                anim->m_lifeTime = 0.0f;
                anim->m_targetId = -1;
            }
        }
    }
}


bool NukePointer::Render()
{
    WorldObject *obj = g_app->GetWorld()->GetWorldObject( m_targetId );
    if( !obj ) 
    {
        return true;
    }

    int transparency = 200;
    if( m_lifeTime < 5.0f )
    {
        transparency *= m_lifeTime * 0.2f;
    }

    if( m_seen )
    {
        m_lifeTime -= g_advanceTime;
        m_lifeTime = max( m_lifeTime, 0.0f );
    }

    
    transparency = 255;
    
    Colour col = Colour(255,255,0,transparency);
    float size = 2.0f;
    if( g_app->GetMapRenderer()->GetZoomFactor() <= 0.25 )
    {
        size *= g_app->GetMapRenderer()->GetZoomFactor() * 4;
    }

    
    m_offScreen = !g_app->GetMapRenderer()->IsOnScreen(obj->m_longitude.DoubleValue(), obj->m_latitude.DoubleValue());
    if( !m_offScreen )
    {
        //m_mode = 0;
        if( !g_app->m_hidden )
        {
            m_seen = true;
        }

        if( m_mode == 0 )
        {
            Image *img = g_resource->GetImage( "graphics/nukesymbol.bmp" );
            g_renderer->Blit( img, obj->m_longitude.DoubleValue(), obj->m_latitude.DoubleValue(), size*2, size*2, col, 0 );
        }
    }

    if( m_offScreen && !m_seen )
    {
        size = 2 * g_app->GetMapRenderer()->GetDrawScale();

        if( m_mode == 1 )
        {
            m_mode = 0;
            return ( m_lifeTime <= 0.0f );
        }


        Fixed distance1 = g_app->GetWorld()->GetDistance( obj->m_longitude, 
                          obj->m_latitude, 
                          Fixed::FromDouble(g_app->GetMapRenderer()->m_middleX), 
                          Fixed::FromDouble(g_app->GetMapRenderer()->m_middleY), true );

        Fixed distance2 = g_app->GetWorld()->GetDistance( obj->m_longitude + 360, 
                          obj->m_latitude, 
                          Fixed::FromDouble(g_app->GetMapRenderer()->m_middleX), 
                          Fixed::FromDouble(g_app->GetMapRenderer()->m_middleY), true );

        Fixed distance3 = g_app->GetWorld()->GetDistance( obj->m_longitude - 360, 
                          obj->m_latitude, 
                          Fixed::FromDouble(g_app->GetMapRenderer()->m_middleX), 
                          Fixed::FromDouble(g_app->GetMapRenderer()->m_middleY), true );
		
		float longitude = obj->m_longitude.DoubleValue();
		float latitude = obj->m_latitude.DoubleValue();

        if( distance2 < distance1 ) longitude += 360;
        if( distance3 < distance1 ) longitude -= 360;


        g_app->GetMapRenderer()->FindScreenEdge( Vector3<float>(longitude, latitude, 0), &m_longitude, &m_latitude);
        Vector3<float> targetDir = (Vector3<float>( longitude, latitude, 0 ) -
									Vector3<float>( m_longitude, m_latitude, 0 )).Normalise();
        float angle = atan( -targetDir.x / targetDir.y );
        if( targetDir.y < 0.0f ) angle += M_PI;

        Image *img = g_resource->GetImage( "graphics/arrow.bmp" );
        g_renderer->Blit( img, m_longitude, m_latitude, size, size, col, angle );
    }
    
    return ( m_lifeTime <= 0.0f );
}

