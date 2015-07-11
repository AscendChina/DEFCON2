#include "lib/universal_include.h"

#ifdef WIN32
#include <io.h>
#include <direct.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>                     // needed for errno definition
#include <ctype.h>
#include <stdarg.h>

#include "lib/debug_utils.h"
#include "lib/string_utils.h"
#include "filesys_utils.h"


//*****************************************************************************
// Misc directory and filename functions
//*****************************************************************************


static bool FilterMatch( const char *_filename, const char *_filter )
{
    while (*_filename && tolower(*_filename) == tolower(*_filter)) {
        _filename++;
        _filter++;
    }

    if (tolower(*_filename) == tolower(*_filter))
        return true;

    switch(*_filter++) {
        case '*':
            while (*_filename) {
                _filename++;
                if (FilterMatch(_filename, _filter))
                    return true;
            }
            return false;
        
        case '?':
            if (!*_filename)
                return false;
            _filename++;
            return FilterMatch(_filename, _filter);
            
        default:
            return false;
    }
}

// Finds all the filenames in the specified directory that match the specified
// filter. Directory should be like "data/textures" or "data/textures/".
// Filter can be NULL or "" or "*.bmp" or "map_*" or "map_*.txt"
// Set FullFilename to true if you want results like "data/textures/blah.bmp" 
// or false for "blah.bmp"
LList <char *> *ListDirectory( const char *_dir, const char *_filter, bool _fullFilename )
{
    if(_filter == NULL || _filter[0] == '\0')
    {
        _filter = "*";
    }

    if(_dir == NULL || _dir[0] == '\0')
    {
        _dir = "";
    }

    // Create a DArray for our results
    LList <char *> *result = new LList<char *>();

    // Now add on all files found locally
#ifdef WIN32
    char searchstring [256];
    AppAssert(strlen(_dir) + strlen(_filter) < sizeof(searchstring) - 1);
    sprintf( searchstring, "%s%s", _dir, _filter );

    _finddata_t thisfile;
    long fileindex = _findfirst( searchstring, &thisfile );

    int exitmeplease = 0;

    while( fileindex != -1 && !exitmeplease ) 
    {
        if( strcmp( thisfile.name, "." ) != 0 &&
            strcmp( thisfile.name, ".." ) != 0 &&
            !(thisfile.attrib & _A_SUBDIR) )
        {
            char *newname = NULL;
            if( _fullFilename ) 
            {
                int len = strlen(_dir) + strlen(thisfile.name);
                newname = new char [len + 1];
                sprintf( newname, "%s%s", _dir, thisfile.name );      
            }
            else
            {
                int len = strlen(thisfile.name);
                newname = new char [len + 1];
                sprintf( newname, "%s", thisfile.name );
            }

            result->PutData( newname );
        }

        exitmeplease = _findnext( fileindex, &thisfile );
    }
#else
    DIR *dir = opendir(_dir[0] ? _dir : ".");
    if (dir == NULL)
        return result;
    for (struct dirent *entry; (entry = readdir(dir)) != NULL; ) {
        if (FilterMatch(entry->d_name, _filter)) {
            char fullname[strlen(_dir) + strlen(entry->d_name) + 2];
            sprintf(fullname, "%s%s%s", 
                _dir, 
                _dir[0] ? "/" : "",
                entry->d_name);
            if (!IsDirectory(fullname)) {
                result->PutData( 
                    newStr(_fullFilename ? fullname : entry->d_name));
            }
        }
    }
    closedir(dir);
#endif

    return result;
}


LList <char *> *ListSubDirectoryNames(const char *_dir)
{
    LList<char *> *result = new LList<char *>();

#ifdef WIN32
    _finddata_t thisfile;
    long fileindex;
	char *dir = ConcatPaths( _dir, "*.*", NULL );
	
	fileindex = _findfirst( dir, &thisfile );

    int exitmeplease = 0;

    while( fileindex != -1 && !exitmeplease ) 
    {
        if( strcmp( thisfile.name, "." ) != 0 &&
            strcmp( thisfile.name, ".." ) != 0 &&
            (thisfile.attrib & _A_SUBDIR) )
        {
            char *newname = strdup( thisfile.name );   
            result->PutData( newname );
        }

        exitmeplease = _findnext( fileindex, &thisfile );
    }
	
	delete[] dir;
#else

    DIR *dir = opendir(_dir);
    if (dir == NULL)
        return result;
    for (struct dirent *entry; (entry = readdir(dir)) != NULL; ) {
        if (entry->d_name[0] == '.')
            continue;
            
        char fullname[strlen(_dir) + strlen(entry->d_name) + 2];
        sprintf(fullname, "%s%s%s", 
            _dir, 
            _dir[0] ? "/" : "",
            entry->d_name);
            
        if (IsDirectory(fullname))
            result->PutData( strdup(entry->d_name) );
    }
    closedir(dir);
    
#endif
    return result;
}


bool DoesFileExist(const char *_fullPath)
{
#ifndef WIN32
    char *path = newStr(_fullPath);
    FILE *f = fopen(FindCaseInsensitive(path), "r");
    delete [] path;
#else
    FILE *f = fopen(_fullPath, "r");
#endif
    if(f) 
    {
        fclose(f);
        return true;
    }

    return false;
}


#define FILE_PATH_BUFFER_SIZE 256
static char s_filePathBuffer[FILE_PATH_BUFFER_SIZE + 1];

char *ConcatPaths(const char *_firstComponent, ...)
{
	va_list components;
	const char *component;
	char *buffer, *returnBuffer;
	
	buffer = strdup(_firstComponent);
	
	va_start(components, _firstComponent);
	while (component = va_arg(components, const char *))
	{
		buffer = (char *)realloc(buffer, strlen(buffer) + 1 + strlen(component) + 1);
		strcat(buffer, "/");
		strcat(buffer, component);
	}
	va_end(components);
	
	returnBuffer = newStr(buffer);
	free(buffer);
	return returnBuffer;
}

char *GetDirectoryPart(const char *_fullFilePath)
{
    strncpy(s_filePathBuffer, _fullFilePath, FILE_PATH_BUFFER_SIZE);
    
    char *finalSlash = strrchr(s_filePathBuffer, '/');
    if( finalSlash )
    {
        *(finalSlash+1) = '\x0';
        return s_filePathBuffer;
    }

    return NULL;
}


char *GetFilenamePart(const char  *_fullFilePath)
{
    const char *filePart = strrchr(_fullFilePath, '/') + 1;
    if( filePart )
    {
        strncpy(s_filePathBuffer, filePart, FILE_PATH_BUFFER_SIZE);
        return s_filePathBuffer;
    }
    return NULL;
}


const char *GetExtensionPart(const char *_fullFilePath)
{
    return strrchr(_fullFilePath, '.') + 1;
}


char *RemoveExtension(const char *_fullFileName)
{
    strcpy( s_filePathBuffer, _fullFileName );

    char *dot = strrchr(s_filePathBuffer, '.');
    if( dot ) *dot = '\x0';
        
    return s_filePathBuffer;
}


bool AreFilesIdentical(const char *_name1, const char *_name2)
{
    FILE *in1 = fopen(_name1, "r");
    FILE *in2 = fopen(_name2, "r");
    bool rv = true;
    bool exitNow = false;

    if (!in1 || !in2)
    {
        rv = false;
        exitNow = true;
    }

    while (exitNow == false && !feof(in1) && !feof(in2))
    {
        int a = fgetc(in1);
        int b = fgetc(in2);
        if (a != b) 
        {
            rv = false;
            exitNow = true;
            break;
        }
    }

    if (exitNow == false && feof(in1) != feof(in2))
    {
        rv = false;
    }

    if (in1) fclose(in1);
    if (in2) fclose(in2);

    return rv;
}


bool CreateDirectory(const char *_directory)
{
#ifdef WIN32
    int result = _mkdir ( _directory );
    if( result == 0 ) return true;                              // Directory was created
    if( result == -1 && errno == 17 /* EEXIST */ ) return true;              // Directory already exists    
    else return false;
#else
    if (access(_directory, W_OK|X_OK|R_OK) == 0)
        return true;
    return mkdir(_directory, 0755) == 0;
#endif
}

bool CreateDirectoryRecursively(const char *_directory)
{
	const char *p;
	char *buffer;
	bool error = false;
	
	if ( strlen(_directory) == 0 )
		return false;

	buffer = new char[ strlen(_directory) + 1 ];
	p = _directory;
	if (*p == '/')
		p++;
	
	p = strchr(p, '/');
	while (p != NULL && !error)
	{
		memcpy(buffer, _directory, p - _directory);
		buffer[ p-_directory ] = '\0';
		error = !CreateDirectory(buffer);
		p = strchr(p+1, '/');
	}
	
	if (error)
		return false;
	else
		return CreateDirectory(_directory);
}

void DeleteThisFile(const char *_filename)
{
#ifdef WIN32
    bool result = DeleteFile( _filename );
#else
    unlink( _filename );
#endif
}

bool IsDirectory(const char *_fullPath)
{
#ifdef WIN32
    AppAssert( false );
    // To do
    return false;
#else
    struct stat s;
    int rc = stat(_fullPath, &s);
    if (rc != 0)
        return false;
    return (s.st_mode & S_IFDIR);
#endif
}

#ifndef TARGET_OS_LINUX
const char *FindCaseInsensitive ( const char *_fullPath )
{
    return _fullPath;
}
#else
#if 1
const char *FindCaseInsensitive ( const char *_fullPath )
{

    if ( !_fullPath )
        return NULL;

    static char retval[PATH_MAX];
    char *dir = NULL, *file = NULL;

    if ( (dir = GetDirectoryPart(_fullPath)) != NULL )
    {
        // Make our own copy of the result, since GetDirectoryPart
        // and GetFilenamePart use the same variable for temp
        // storage.
        dir = newStr(dir);
    }
    if ( !dir )
    {
        // No directory provided. Assume working directory.
        file = newStr(_fullPath);
    } else {
        // Kill the last slash
        dir[strlen(dir) - 1] = '\0';
        file = newStr(GetFilenamePart(_fullPath));
    }
    LList <char *> *files = ListDirectory(dir, file);

    delete [] dir; delete [] file; dir = file = NULL;

    // We shouldn't have found more than one match.
    AppAssert(files->Size() <= 1);

    // No results, so maybe the file does not exist.
    if ( files->Size() == 0 )
       return _fullPath;

    // Copy the corrected path back, and prepare to return it.
    memset ( retval, 0, sizeof ( retval ) );
    AppAssert ( strlen ( files->GetData(0) ) < PATH_MAX );
    strcpy ( retval, files->GetData(0) );

    // Negate the possibility of a memory access violation.
    // This way, we can simply strcpy the result inline without
    // worrying about a buffer overflow.
    AppAssert(strlen(retval) == strlen(_fullPath));

    while ( files->Size() )
	{
		char *data = files->GetData(0);
		files->RemoveData(0);
		delete [] data;
	}

    delete files;

    return retval;
}
#else
const char *FindCaseInsensitive( char *_fullPath )
{
    AppAssert(strlen(_fullPath) < PATH_MAX);
    
    static char pathSoFar[PATH_MAX];
    pathSoFar[0] = '\0';
    
    while (true) {
        char *delimiter;
        
        // Skip to the next / or end-of-string
        for (delimiter = (char *)_fullPath; *delimiter && *delimiter != '/'; delimiter++);
        
        char component[PATH_MAX];
        AppAssert ( strlen(_fullPath) < PATH_MAX );
        strcpy( component, _fullPath );
        component[delimiter - _fullPath] = '\0';
    
        // Search for a match
        LList<char *> *matches = ListDirectory( pathSoFar, component, true );
        bool found = false;
    
        if (matches->Size() > 0) {
            strcpy(pathSoFar, matches->GetData( 0 ));
            found = true;
        }
    
        matches->EmptyAndDelete();
    
        // Failed to find a match, just return the original
        if (!found)
            return _fullPath;
        
        // Got to the end of the path, return it
        if (!*delimiter) 
            return pathSoFar;
        
        _fullPath = delimiter + 1;
    }
}
#endif

#endif
