#include "lib/universal_include.h"
#include "lib/string_utils.h"

#include <unrar/unrar.h>

#include "file_system.h"
#include "filesys_utils.h"
#include "text_stream_readers.h"
#include "binary_stream_readers.h"
#include "lib/string_utils.h"

FileSystem *g_fileSystem = NULL;


FileSystem::FileSystem()
{
}


FileSystem::~FileSystem()
{
}


void FileSystem::ParseArchives( const char *_dir, const char *_filter )
{
    LList<char *> *results = ListDirectory( _dir, _filter, false );
    for( int i = 0; i < results->Size(); ++i )
    {
        char fullFilename[512];
        snprintf( fullFilename, sizeof(fullFilename), "%s%s", _dir, results->GetData( i ) );
        fullFilename[ sizeof(fullFilename) - 1 ] = '\0';
        ParseArchive( fullFilename );
    }
    results->EmptyAndDelete();
    delete results;
}


void FileSystem::ParseArchive( const char *_filename )
{
	UncompressedArchive	*mainData = NULL;

    AppDebugOut( "Parsing archive %s...", _filename );

	try
	{
		mainData = new UncompressedArchive(_filename,NULL);
        if( mainData && mainData->m_numFiles > 0 )
        {
            AppDebugOut( "DONE\n" );
        }
        else
        {
            AppDebugOut( "NOT FOUND\n" );
        }
	}
	catch( ... )
	{
        AppDebugOut( "FAILED\n" );
		return;
	}

	for (int i = 0; i < mainData->m_numFiles; ++i)
	{
		MemMappedFile *file = mainData->m_files[i];
		if (file->m_size > 0)
		{
			strlwr(file->m_filename);
			
			// Subsequent archives may override existing resources
			
			MemMappedFile *oldFile = m_archiveFiles.GetData(file->m_filename);
			if (oldFile) 
            {
				m_archiveFiles.RemoveData(file->m_filename);
				delete oldFile;
			}
			
			m_archiveFiles.PutData(file->m_filename, file);
		}
	}

	delete mainData;
}


TextReader *FileSystem::GetTextReader(const char *_filename)
{
	TextReader *reader = NULL;

    for( int i = 0; i < m_searchPath.Size(); ++i )
    {
        char fullFilename[512];
        snprintf( fullFilename, sizeof(fullFilename), "%s%s", m_searchPath[i], _filename );
        fullFilename[ sizeof(fullFilename) - 1 ] = '\0';
        if( DoesFileExist(fullFilename) )
        {
            reader = new TextFileReader(fullFilename);
            break;
        }
    }

    if( !reader )
    {
        char fullFilename[512];
        snprintf( fullFilename, sizeof(fullFilename), "localisation/%s", _filename );
        fullFilename[ sizeof(fullFilename) - 1 ] = '\0';
        if (DoesFileExist(fullFilename)) 
        {
            reader = new TextFileReader(fullFilename);
        }
    }

    if( !reader )
    {
        if (DoesFileExist(_filename))
        {
            reader = new TextFileReader(_filename);	    
        }
    }

	if( !reader )
	{
		MemMappedFile *mmfile = m_archiveFiles.GetData(_filename);
		if (mmfile) reader = new TextDataReader((char*)mmfile->m_data, mmfile->m_size, _filename);		
	}

	return reader;
}


BinaryReader *FileSystem::GetBinaryReader(const char *_filename)
{
	BinaryReader *reader = NULL;

    for( int i = 0; i < m_searchPath.Size(); ++i )
    {
        char fullFilename[512];
        snprintf( fullFilename, sizeof(fullFilename), "%s%s", m_searchPath[i], _filename );
        fullFilename[ sizeof(fullFilename) - 1 ] = '\0';
        if( DoesFileExist(fullFilename) )
        {
            reader = new BinaryFileReader(fullFilename);
            break;
        }
    }

    if( !reader )
    {
        char fullFilename[512];
        snprintf( fullFilename, sizeof(fullFilename), "localisation/%s", _filename );
        fullFilename[ sizeof(fullFilename) - 1 ] = '\0';
        if (DoesFileExist(fullFilename)) 
        {
            reader = new BinaryFileReader(fullFilename);
        }
    }

    if( !reader )
    {
	    if (DoesFileExist(_filename)) 
        {
            reader = new BinaryFileReader(_filename);
        }
    }

    if( !reader )
	{
		MemMappedFile *mmfile = m_archiveFiles.GetData(_filename);
		if (mmfile) reader = new BinaryDataReader(mmfile->m_data, mmfile->m_size, _filename);		
	}

	return reader;
}



int WildCmp(char const *wild, char const *string) 
{
    char const *cp;
    char const *mp;

    while ((*string) && (*wild != '*')) 
    {
        if ((*wild != *string) && (*wild != '?'))
        {
            return 0;
        }
        wild++;
        string++;
    }

    while (*string) 
    {
        if (*wild == '*')
        {
            if (!*++wild)
            {
                return 1;
            }
            mp = wild;
            cp = string + 1;
        }
        else if ((*wild == *string) || (*wild == '?'))
        {
            wild++;
            string++;
        }
        else 
        {
            wild = mp;
            string = cp++;
        }
    }

    while (*wild == '*') 
    {
        wild++;
    }
    return !*wild;
}



// The string is copied and entered into llist
// in correct alphabetical order.
// The string wont be added if it is already present.
void OrderedInsert( LList<char *> *_llist, char *_newString )
{
    bool added = false;

    for( int i = 0; i < _llist->Size()-1; ++i )
    {
        char *thisString = _llist->GetData(i);
        if( strcmp( _newString, thisString ) == 0 )
        {
            // String duplicate
            return;
        }
        if( strcmp( _newString, thisString ) < 0 )
        {
            _llist->PutDataAtIndex( strdup(_newString), i );
            added = true;
            break;
        }
    }

    if( !added )
    {
        _llist->PutDataAtEnd( strdup(_newString) );
    }
}


static void ListArchiveFilterResults( LList<char *> *results, LList<char *> *results2 )
{
    for( int i = 0; i < results2->Size(); ++i )
    {
        char *filename = results2->GetData( i );
        OrderedInsert( results, filename );
    }
}


// Finds all the filenames in the specified directory that match the specified
// filter. Directory should be like "textures" or "textures/".
// Filter can be NULL or "" or "*.bmp" or "map_*" or "map_*.txt"
// Set _longResults to true if you want results like "textures/blah.bmp" 
// or false for "blah.bmp"
LList<char *> *FileSystem::ListArchive(char *_dir, char *_filter, bool fullFilename)
{
    LList<char *> *results = NULL;

    //
    // List the search directories

    for( int i = 0; i < m_searchPath.Size(); ++i )
    {
        char searchPathFilename[512];
        snprintf( searchPathFilename, sizeof(searchPathFilename), "%s%s", m_searchPath[i], _dir );
        searchPathFilename[ sizeof(searchPathFilename) - 1 ] = '\x0';
        LList<char *> *results2 = ListDirectory( searchPathFilename, _filter, fullFilename );
        if( !results )
        {
            results = results2;
        }
        else
        {
            ListArchiveFilterResults( results, results2 );
            results2->EmptyAndDelete();
            delete results2;
        }
    }


    //
    // List the localisation directory

    char localisationFilename[512];
    snprintf( localisationFilename, sizeof(localisationFilename), "localisation/%s", _dir );
    localisationFilename[ sizeof(localisationFilename) - 1 ] = '\0';
    LList<char *> *results3 = ListDirectory( localisationFilename, _filter, fullFilename );
    if( !results )
    {
        results = results3;
    }
    else
    {
        ListArchiveFilterResults( results, results3 );
        results3->EmptyAndDelete();
        delete results3;
    }


    //
    // List the base data directory

    LList<char *> *results4 = ListDirectory( _dir, _filter, fullFilename );
    ListArchiveFilterResults( results, results4 );
    results4->EmptyAndDelete();
    delete results4;


    //
    // List the pre-loaded resource files

    if (m_archiveFiles.Size() > 0)
    {
        if(_filter == NULL || _filter[0] == '\0')
        {
            _filter = "*";
        }

        DArray <char *> *unfilteredResults = m_archiveFiles.ConvertIndexToDArray();

        for (int i = 0; i < unfilteredResults->Size(); ++i)
        {
            if (!unfilteredResults->ValidIndex(i)) continue;

            char *fullname = unfilteredResults->GetData(i);
            char *dirPart = (char *) GetDirectoryPart( fullname );
            if( stricmp( dirPart, _dir ) == 0 )
            {
                char *filePart = (char *) GetFilenamePart( fullname );
                int result = WildCmp(_filter, filePart);
                if (result != 0)
                {
                    if( !fullFilename )  OrderedInsert(results, filePart);
                    else                 OrderedInsert(results, fullname);
                }
            }
        }

        delete unfilteredResults;
    }

    return results;
}


void FileSystem::ClearSearchPath()
{
    m_searchPath.EmptyAndDelete();
}


void FileSystem::AddSearchPath( char *_path )
{
    m_searchPath.PutData( newStr( _path ) );
}

