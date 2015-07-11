#include "lib/universal_include.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/ucontext.h>
#include <unistd.h>
#include <errno.h>

#include "app.h"
#include "lib/debug_utils.h"

#ifndef NO_WINDOW_MANAGER
#include "lib/gucci/window_manager.h"
#include <SDL/SDL.h>
#endif

static char *s_pathToProgram = 0;

void SetupPathToProgram(const char *program)
{
	// binreloc gives us the full path
	s_pathToProgram = strcpy(new char[strlen(program)+1], program);
}

static void fatalSignal(int signum, siginfo_t *siginfo, void *arg)
{
	static char msg[64];
	sprintf(msg, "Got a fatal signal: %d\n", signum);
	ucontext_t *u = (ucontext_t *) arg;

/*
	// TODO: Get this commented code block fixed up nicely.
	GenerateBlackBox(msg, (unsigned *) u->uc_mcontext.gregs[REG_EBP]);

	fprintf(stderr, 
			"\n\nDarwinia has unexpectedly encountered a fatal error.\n"
			"A full description of the error can be found in the file\n"
			"blackbox.txt in the current working directory\n\n");

#ifndef NO_WINDOW_MANAGER		
	g_windowManager->UncaptureMouse();
	g_windowManager->UnhideMousePointer();
	SDL_Quit();
#endif
*/

}


void SetupMemoryAccessHandlers()
{
	struct sigaction sa;

	sa.sa_sigaction = fatalSignal;
	sa.sa_flags = SA_SIGINFO | SA_RESETHAND;
	sigemptyset(&sa.sa_mask);
	
	sigaction(SIGSEGV, &sa, 0);
	sigaction(SIGBUS, &sa, 0);
}
