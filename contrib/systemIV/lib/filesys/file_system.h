#ifndef _included_filesystem_h
#define _included_filesystem_h


/*
 * ===========
 * FILE SYSTEM
 * ===========
 *
 *	A file access system designed to allow an app 
 *  to open files for reading/writing.
 *  Will transparently load from compressed archives 
 *  if they are present.
 *
 */

class MemMappedFile;
class TextReader;
class BinaryReader;

#include "lib/tosser/btree.h"
#include "lib/tosser/llist.h"



class FileSystem 
{
protected:
	BTree <MemMappedFile *>	m_archiveFiles;
    
public:
    LList <char *> m_searchPath;                                                        // Use to set up mods

public:
    FileSystem();
    ~FileSystem();


    void            ParseArchive		( const char *_filename );
    void            ParseArchives		( const char *_dir, const char *_filter );

	TextReader		*GetTextReader	    ( const char *_filename );	                    // Caller must delete the TextReader when done
	BinaryReader	*GetBinaryReader    ( const char *_filename );	                    // Caller must delete the BinaryReader when done

    LList<char *>   *ListArchive        (char *_dir, char *_filter, bool fullFilename = true);


    void            ClearSearchPath     ();
    void            AddSearchPath       ( char *_path );                                // Must be added in order
};




extern FileSystem *g_fileSystem;

#endif
