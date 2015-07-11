#ifndef _included_hash_h
#define _included_hash_h

/*
 *	Hash data using SHA1
 *  Can pass in a seed hash token
 *
 *  result is written into
 */


void HashData( char *_data, int _hashToken, char *_result );                        // No more than 500 bytes of data please

void HashData( char *_data, char *_result );                                        // Any amount of data is Ok


#endif
