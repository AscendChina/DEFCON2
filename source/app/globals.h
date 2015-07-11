
/*
 * =======
 * GLOBALS
 * =======
 */

#ifndef _included_globals_h
#define _included_globals_h


/* ============================================================================
 *   Global constants
 */


#define SERVER_ADVANCE_FREQ     (Fixed(10))                     // Num server ticks per second
#define SERVER_ADVANCE_PERIOD   (Fixed(1)/SERVER_ADVANCE_FREQ)  // Server ticks at this rate
#define IAMALIVE_PERIOD         SERVER_ADVANCE_PERIOD           // Clients must send IAmAlive this often


/* ============================================================================
 *   Global variables
 */

class  App;
extern App      *g_app;

class  NetMutex;
extern NetMutex g_renderLock;

extern double   g_gameTime;					                    // Updated from GetHighResTime every frame
extern double   g_startTime;
extern float    g_advanceTime;                                  // How long the last frame took
extern double   g_lastServerAdvance;                            // Time of last server advance
extern float    g_predictionTime;                               // Time between last server advance and start of render
extern int      g_lastProcessedSequenceId;


#endif
