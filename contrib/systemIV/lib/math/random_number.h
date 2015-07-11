#ifndef _included_randomnumbers_h
#define _included_randomnumbers_h

#include "lib/math/fixed.h"

/*
 * =============================================
 *	Basic random number generators
 *
 */

#define APP_RAND_MAX    32767


void    AppSeedRandom   (unsigned int seed);                                
int     AppRandom       ();                                                 // 0 <= result < APP_RAND_MAX


float   frand           (float range = 1.0f);                               // 0 <= result < range
float   sfrand          (float range = 1.0f);                               // -range < result < range




/*
 * =============================================
 *	Network Syncronised random number generators
 *  
 *  These must only be called in deterministic
 *  net-safe code.
 *
 *  Don't call the underscored functions directly -
 *  instead call syncrand/syncfrand and the preprocessor
 *  will do the rest.
 *
 */


void            syncrandseed    ( unsigned long seed = 0 );
unsigned long   _syncrand       ();
Fixed           _syncfrand      ( Fixed range = 1 );                     // 0 <= result < range
Fixed           _syncsfrand     ( Fixed range = 1 );                     // -range < result < range

// Added by Robin for AI API
// get and set seed state
unsigned long   getSeed         ( int index );
void            setSeed         ( int index, unsigned long value );



/*
 * ==============================================
 *  Debug versions of those Net sync'd generators
 *
 *  Handy for tracking down Net Syncronisation errors
 *  To use : #define TRACK_SYNC_RAND in universal_include.h
 *         : then call DumpSyncRandLog when you know its gone wrong
 *
 *
 */

unsigned long   DebugSyncRand   (char *_file, int _line );
Fixed           DebugSyncFrand  (char *_file, int _line, Fixed _min, Fixed _max );
void            DumpSyncRandLog (char *_filename);
void 			FlushSyncRandLog ();


#ifdef TRACK_SYNC_RAND
    #define         syncrand()      DebugSyncRand(__FILE__,__LINE__)
    #define         syncfrand(x)    DebugSyncFrand(__FILE__,__LINE__,0,x)
    #define         syncsfrand(x)   DebugSyncFrand(__FILE__,__LINE__,-x/2,x/2)
#else
    #define        syncrand          _syncrand
    #define        syncfrand(x)      _syncfrand(x)
    #define        syncsfrand(x)     _syncsfrand(x)
#endif


// Temporary until Net Sync problem is fixed

void            SyncRandLog     (char *_message, ...);

/*
 * =============================================
 *	Statistics based random number generatores
 */


Fixed       RandomNormalNumber  ( Fixed _mean, Fixed _range );	            // result ~ N ( mean, range/3 ), 					
                                                                            // mean - range < result < mean + range	
  
Fixed       RandomApplyVariance ( Fixed _num, Fixed _variance );			// Applies +-percentage Normally distributed variance to num
// variance should be in fractional form eg 1.0, 0.5 etc                
                                                                            // _num - _variance * _num < result < _num + _variance * _num



#endif
