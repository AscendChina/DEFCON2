#ifndef __included_macutil_h
#define __included_macutil_h

char *GetLocale();
char *GetKeyboardLayout ();
char *GetBundleResourcePath( char *_resourceName );
bool OpenBundleResource( char *_name, char *_type );

#endif