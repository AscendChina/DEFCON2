#ifndef _included_splashscreen_renderer_h
#define _included_splashscreen_renderer_h

#include "lib/resource/image.h"

class SplashScreenRenderer
{
public:
	enum State
	{
		FadingIn,
		FullyShown,
		FadingOut,
		Idle
	};
	
protected:
	Image *m_logo;
	Colour m_colour;
	float m_timer;
	State m_state;
	
public:
	SplashScreenRenderer( char *filename );
	void Update();
	void Render();
	
	State GetState() { return m_state; }
	void SetState(State _state);
	float TimeInCurrentState();
};

#endif