#include "universal_include.h"
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

bool spawn(const char *program, char * const *args)
{
	pid_t pid = fork();
	if (pid < 0) 
		return false;

	if (pid > 0)
		return true;  // Return to the parent

	// Become a session leader and close the file descriptors

	setsid();
	close(0);
	close(1);
	close(2);

	int devnullr = open("/dev/null", O_RDONLY);
	int devnullw = open("/dev/null", O_WRONLY);

	dup2(devnullr, 0);
	dup2(devnullw, 1);
	dup2(devnullw, 2);

	close(devnullr);
	close(devnullw);

	pid_t pid2 = fork();

	if (pid2 > 0) // Exit second parent
		_exit(0);  // Don't run atexit

	// launch the program
	execv(program, args);
	_exit(0);
}
