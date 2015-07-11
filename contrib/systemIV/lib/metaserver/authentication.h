
/*
 *	Authentication system
 *
 *
 */


#ifndef _included_authentication_h
#define _included_authentication_h


#define AUTHENTICATION_KEYLEN       32


enum
{
	AuthenticationWrongPlatform     = -9,                            // Trying to use Ambrosia key on PC, or vice versa
    AuthenticationKeyDead			= -8,							 // The key has been replaced with another key
	AuthenticationVersionNotFound   = -7,							 // The version was not found
	AuthenticationKeyInactive	    = -6,							 // The key was found, but has not been made active yet
    AuthenticationVersionRevoked    = -5,					 		 // Version is disabled
    AuthenticationKeyNotFound       = -4,                            // The key could not be found in the list of known keys
    AuthenticationKeyRevoked        = -3,                            // The key is no longer valid
    AuthenticationKeyBanned         = -2,                            // The key has been banned due to misuse
    AuthenticationKeyInvalid        = -1,                            // The key is not algorithmically correct
    AuthenticationUnknown           = 0,                             // The key has not yet been tested
    AuthenticationAccepted          = 1                              // The key is fine
};



/*
 *	Basic key generation / load / save
 *
 */
 

void    Authentication_GenerateKey      ( char *_key, bool _demo=false );   
void	Authentication_SaveKey			( const char *_key, const char *_filename );
void    Authentication_LoadKey          ( char *_key, const char *_filename );

void    Authentication_SetKey           ( char *_key );
void    Authentication_GetKey           ( char *_key );
bool    Authentication_IsKeyFound       ();
void    Authentication_GetKeyHash       ( char *_key );
int     Authentication_GetKeyId         ( char *_key );


/*
 *  Special key type stuff ( eg Demo Key etc )
 *
 */

bool    Authentication_IsDemoKey        ( char *_key );
void    Anthentication_EnforceDemo      ();                             // If set, will not allow full keys to be used


/*
 *	Simple key authentication
 *  Performs an entirely local key check (ie no net access)
 *
 */

int     Authentication_SimpleKeyCheck   ( char *_key );                       


/*
 *	Hash Key methods
 *  For safely transmitting keys without them being stolen
 *
 */

void    Authentication_SetHashToken     ( int _hashToken );
int     Authentication_GetHashToken     ();
void    Authentication_GetKeyHash       ( char *_sourceKey, char *_destKey, int _hashToken );
bool    Authentication_IsHashKey        ( char *_key );


/*
 *	Methods to get my authentication status on a key
 *  The set method is intended to be used by the MetaServer when messages come in
 *
 */


void    Authentication_RequestStatus    ( char *_key, int _keyId=-1, char *_ip=NULL );                     // The IP is used ONLY for logging
int     Authentication_GetStatus        ( char *_key );
void    Authentication_SetStatus        ( char *_key, int _keyId, int _status );


char    *Authentication_GetStatusString ( int _status );
char    *Authentication_GetStatusStringLanguagePhrase ( int _status );




#endif


