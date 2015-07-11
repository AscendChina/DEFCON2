#include "lib/universal_include.h"
#include "lib/string_utils.h"
#include "lib/tosser/llist.h"
#include "lib/preferences.h"

#include "random_number.h"
#include <stdarg.h>

static LList<char *> s_syncrandlog;

static long holdrand = 1L;
static int s_callCount = 0;


void AppSeedRandom (unsigned int seed)
{
    holdrand = (long) seed;
}

int AppRandom ()
{
    return (((holdrand = holdrand * 214013L + 2531011L) >> 16) & 0x7fff);
}



void SyncRandLog(char *_message, ...)
{
#ifdef TRACK_SYNC_RAND
    static int s_maxSize = 10000;

    char buf[1024];
    va_list ap;
    va_start (ap, _message);
	vsnprintf(buf, sizeof(buf), _message, ap);

    s_syncrandlog.PutData( newStr ( buf ) );


    // Keep the size down (assuming we aren't doing heavy duty logging)

    while( s_syncrandlog.Size() > s_maxSize )
    {
        char *elemZero = s_syncrandlog[0];
        s_syncrandlog.RemoveData(0);
        delete [] elemZero;
    }
#endif
}


void LogSyncRand(char *_file, int _line, float _num )
{
#ifdef TRACK_SYNC_RAND
    char *fromPoint = strrchr( _file, '/' );
    if( !fromPoint ) fromPoint = strrchr( _file, '\\' );
    if( fromPoint ) fromPoint += 1;
    else fromPoint = _file;

    SyncRandLog( "%8d | %14.2f | %s line %d", s_callCount, _num, fromPoint, _line );
    
    ++s_callCount;
#endif
}


unsigned long DebugSyncRand(char *_file, int _line )
{    
    unsigned long result = _syncrand();

    LogSyncRand( _file, _line, result );

    return result;
}


Fixed DebugSyncFrand(char *_file, int _line, Fixed _min, Fixed _max )
{
    Fixed range = _max - _min;
    Fixed result = _min + int(_syncrand() & 0x7FFFFFFF) * (range/Fixed(0x7FFFFFFF));

    LogSyncRand( _file, _line, result.DoubleValue() );

    return result;
}


void DumpSyncRandLog (char *_filename)
{
    FILE *file = fopen(_filename, "wt" );

    if( !file ) return;
    
    for( int i = 0; i < s_syncrandlog.Size(); ++i )
    {
        char *thisEntry = s_syncrandlog[i];
        fprintf( file, "%s\n", thisEntry );
    }

    fclose(file);
}

void FlushSyncRandLog ()
{
    while( s_syncrandlog.ValidIndex ( 0 ) )
    {
        char *elemZero = s_syncrandlog[0];
        s_syncrandlog.RemoveData(0);
        delete [] elemZero;
    }
}



// ****************************************************************************
// Mersenne Twister Random Number Routines
// ****************************************************************************

/* This code was taken from
   http://www.math.keio.ac.jp/~matumoto/MT2002/emt19937ar.html
   
   C-program for MT19937, with initialization improved 2002/1/26.
   Coded by Takuji Nishimura and Makoto Matsumoto.

   Before using, initialize the state by using init_genrand(seed)  
   or init_by_array(init_key, key_length).

   Copyright (C) 1997 - 2002, Makoto Matsumoto and Takuji Nishimura,
   All rights reserved.                          

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

     1. Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.

     2. Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.

     3. The names of its contributors may not be used to endorse or promote 
        products derived from this software without specific prior written 
        permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   Any feedback is very welcome.
   http://www.math.keio.ac.jp/matumoto/emt.html
   email: matumoto@math.keio.ac.jp
*/

/* Period parameters */  
#define N 624
#define M 397
#define MATRIX_A 0x9908b0dfUL   /* constant vector a */
#define UPPER_MASK 0x80000000UL /* most significant w-r bits */
#define LOWER_MASK 0x7fffffffUL /* least significant r bits */

static unsigned long mt[N]; /* the array for the state vector  */
static int mti=N+1; /* mti==N+1 means mt[N] is not initialized */
static unsigned long mag01[2];

unsigned long getSeed( int index )
{
    if (index == N) 
        return mti;
    return mt[index];
}

void setSeed( int index, unsigned long value )
{
    if (index == N)
        mti = int( value );
    else
        mt[index] = value;
}

/* initializes mt[N] with a seed */
void syncrandseed(unsigned long s)
{
    if( s == (unsigned long) -1 ) s = 5489UL;
    
    s_callCount = 0;
    LogSyncRand( "SyncRandSeed called", 0, s );
    
    mag01[0]=0x0UL;
    mag01[1]=MATRIX_A;

    mt[0]= s & 0xffffffffUL;
    for (mti=1; mti<N; mti++) {
        mt[mti] = 
	    (1812433253UL * (mt[mti-1] ^ (mt[mti-1] >> 30)) + mti); 
        /* See Knuth TAOCP Vol2. 3rd Ed. P.106 for multiplier. */
        /* In the previous versions, MSBs of the seed affect   */
        /* only MSBs of the array mt[].                        */
        /* 2002/01/09 modified by Makoto Matsumoto             */
        mt[mti] &= 0xffffffffUL;
        /* for >32 bit machines */
    }
}


// Generates a random number on [0,0xffffffff]-interval
unsigned long _syncrand()
{
    unsigned long y;
    /* mag01[x] = x * MATRIX_A  for x=0,1 */
    if (mti >= N) { /* generate N words at one time */
        int kk;

        if (mti == N+1)         /* if init_genrand() has not been called, */
            syncrandseed();     /* a default initial seed is used */

        for (kk=0;kk<N-M;kk++) {
            y = (mt[kk]&UPPER_MASK)|(mt[kk+1]&LOWER_MASK);
            mt[kk] = mt[kk+M] ^ (y >> 1) ^ mag01[y & 0x1UL];
        }
        for (;kk<N-1;kk++) {
            y = (mt[kk]&UPPER_MASK)|(mt[kk+1]&LOWER_MASK);
            mt[kk] = mt[kk+(M-N)] ^ (y >> 1) ^ mag01[y & 0x1UL];
        }
        y = (mt[N-1]&UPPER_MASK)|(mt[0]&LOWER_MASK);
        mt[N-1] = mt[M-1] ^ (y >> 1) ^ mag01[y & 0x1UL];

        mti = 0;
    }
  
    y = mt[mti++];

    /* Tempering */
    y ^= (y >> 11);
    y ^= (y << 7) & 0x9d2c5680UL;
    y ^= (y << 15) & 0xefc60000UL;
    y ^= (y >> 18);

    return y;
}


Fixed _syncfrand( Fixed range )
{
    Fixed result = int(syncrand() & 0x7FFFFFFF) * (range/Fixed(0x7FFFFFFF)); 
    return result;
}


Fixed _syncsfrand( Fixed range )
{
    Fixed result = (syncfrand(1) - Fixed::FromDouble(0.5)) * range; 
    return result;
}


float frand(float range)
{ 
    return range * ((float)AppRandom() / (float)APP_RAND_MAX); 
}


float sfrand(float range)
{
    return (0.5f - (float)AppRandom() / (float)(APP_RAND_MAX)) * range; 
}



Fixed RandomNormalNumber ( Fixed mean, Fixed range )	
{	
	// result ~ N ( mean, range/3 )

	Fixed s = 0;

	for ( int i = 0; i < 12; ++i )
    {
		s += syncfrand(1);
    }
    
	s = ( s-6 ) * ( range/3 ) + mean;
    
	if ( s < mean - range ) s = mean - range;
	if ( s > mean + range ) s = mean + range;

	return s;
}


Fixed RandomApplyVariance ( Fixed num, Fixed variance )
{
	Fixed variancefactor = 1 + RandomNormalNumber ( 0, variance );
	num *= variancefactor;								

	return num;
}
