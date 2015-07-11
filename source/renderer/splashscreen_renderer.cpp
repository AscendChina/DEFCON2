/*
 *  splashscreen_renderer.cpp
 *  Defcon
 *
 *  Created by Mike Blaguszewski on 3/15/07.
 *  Copyright 2007 Ambrosia Software, Inc.. All rights reserved.
 *
 */

#include "lib/universal_include.h"

#include "splashscreen_renderer.h"
#include "macutil.h"
#include "app/globals.h"

#include "lib/render/renderer.h"
#include "lib/gucci/window_manager.h"
#include "lib/resource/bitmap.h"
#include "lib/hi_res_time.h"

const float FADE_TIME = 0.5;

SplashScreenRenderer::SplashScreenRenderer( char *filename )
: m_logo( NULL ),
  m_state( Idle ),
  m_colour( Black )
{
    m_logo = g_resource->GetImage( filename );	
}

void SplashScreenRenderer::SetState(State _state)
{
	m_timer = GetHighResTime();
	m_state = _state;
	Update();
}

float SplashScreenRenderer::TimeInCurrentState()
{
	return GetHighResTime() - m_timer;
}

void SplashScreenRenderer::Update()
{
	float timeInCurrentState = GetHighResTime() - m_timer;
	
	switch (m_state)
	{
		case FadingIn:
			if (TimeInCurrentState() < FADE_TIME)
			{
				m_colour = White;
				m_colour.m_a *= ( timeInCurrentState / FADE_TIME );
			}
			else
			{
				SetState(FullyShown);
			}
			break;
		
		case FullyShown:
			m_colour = White;
			break;
			
		case FadingOut:
			if (TimeInCurrentState() < FADE_TIME)
			{
				m_colour = White;
				m_colour.m_a *= 1 - ( timeInCurrentState / FADE_TIME );
			}
			else
			{
				SetState(Idle);
			}
			break;
			
		case Idle:
			m_colour = Black;
			break;
	}
}

void SplashScreenRenderer::Render()
{
	float x = (g_windowManager->WindowW() - m_logo->m_bitmap->m_width) / 2;
	float y = (g_windowManager->WindowH() - m_logo->m_bitmap->m_height) / 2;
	
	g_renderer->Reset2DViewport();
	g_renderer->SetBlendMode(Renderer::BlendModeNormal);
	g_renderer->ClearScreen(true, true);
	g_renderer->Blit(m_logo, x, y, m_colour);
}