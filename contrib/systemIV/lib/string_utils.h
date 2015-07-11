#ifndef __STRING_UTILS_H
#define __STRING_UTILS_H




char    *newStr         ( const char *s);		                                    // Make a copy of s, use delete[] to reclaim storage

void    StringReplace   ( const char *target, const char *string, char *subject );  // replace target with string in subject

void    SafetyString    ( char *string );                                           // Removes dangerous characters like %, replaces with safe characters

void    ReplaceExtendedCharacters ( char *string );                                 // Replace characters above 127 by a space (' ')

void    StripExtendedCharacters ( char *string );                                   // Removes characters above 127

void    StripTrailingWhitespace ( char *string );                                   // Removes trailing /n, /r, space 

#endif // __STRING_UTILS_H
