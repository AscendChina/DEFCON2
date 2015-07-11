
/*
 * =========
 * DIRECTORY
 * =========
 *
 * A registry like data structure that can store arbitrary
 * data in a hierarchical system.
 *
 * Can be easily converted into bytestreams for network use.
 *
 */

#ifndef _included_genericdirectory_h
#define _included_genericdirectory_h

#include <iostream>

#include "lib/math/fixed.h"
#include "lib/tosser/darray.h"
#include "lib/tosser/llist.h"

class DirectoryData;

class Directory
{
public:
    char        *m_name;
    DArray      <Directory *>       m_subDirectories;
    DArray      <DirectoryData *>   m_data;

public:
    Directory();
    Directory( Directory *_copyMe );
    ~Directory();

    void SetName ( char *newName );

    //
    // Basic data types 

    void    CreateData      ( char *dataName, int   value );           
    void    CreateData      ( char *dataName, float value );         
	void	CreateData		( char *dataName, Fixed value );
    void    CreateData      ( char *dataName, unsigned char value );
    void    CreateData      ( char *dataName, char  value );          
    void    CreateData      ( char *dataName, char *value );
    void    CreateData      ( char *dataName, bool  value );
    void    CreateData      ( char *dataName, void *data, int dataLen );

    int             GetDataInt          ( char *dataName );                       // These are safe read functions.
    float           GetDataFloat        ( char *dataName );                       // All return some value.
	Fixed			GetDataFixed		( char *dataName );
    unsigned char   GetDataUChar        ( char *dataName );
    char            GetDataChar         ( char *dataName );
    char            *GetDataString      ( char *dataName );                       // You should strdup this
    bool            GetDataBool         ( char *dataName );
    void            *GetDataVoid        ( char *dataName, int *_dataLen );

    void    RemoveData      ( char *dataName );
    bool    HasData         ( char *dataName, int _mustBeType =-1 );


    //
    // Tosser data types

    void    CreateData      ( char *dataName, DArray    <int> *darray );
    void    CreateData      ( char *dataName, LList     <int> *llist );
    void    CreateData      ( char *dataName, LList     <Directory *> *llist );

    void    GetDataDArray   ( char *dataName, DArray    <int> *darray );
    void    GetDataLList    ( char *dataName, LList     <int> *llist );
    void    GetDataLList    ( char *dataName, LList     <Directory *> *llist );

    
    //
    // Low level interface stuff

    Directory       *GetDirectory       ( char *dirName );
    DirectoryData   *GetData            ( char *dataName );
    Directory       *AddDirectory       ( char *dirName );                              // Will create all neccesary subdirs recursivly
    void             AddDirectory       ( Directory *dir );
    void             RemoveDirectory    ( char *dirName );                              // Directory is NOT deleted
    void             CopyData           ( Directory *dir, 
                                          bool overWrite = false,                       // Overwrite existing data if found
                                          bool directories = true);                     // Copy subdirs as well

    //
    // Saving / Loading to streams

    bool Read  ( std::istream &input );                                                      // returns false if an error occurred while reading
    void Write ( std::ostream &output );

    bool Read   ( char *input, int length );
    char *Write ( int &length );                                                        // Creates new string

    static void WriteDynamicString ( std::ostream &output, char *string );   				// Works with NULL
    static char *ReadDynamicString ( std::istream &input );									// Assigns space for string
    
    static void WritePackedInt     ( std::ostream &output, int _value );
    static int  ReadPackedInt      ( std::istream &input );

    static void WriteVoidData      ( std::ostream &output, void *data, int dataLen );       
    static void *ReadVoidData      ( std::istream &input, int *dataLen );				    // Assigns space for data


    //
    // Other 

    void DebugPrint ( int indent );
	void WriteXML ( std::ostream &o, int indent = 0 );

    int  GetByteSize();

    static char     *GetFirstSubDir     ( char *dirName );                      // eg returns "bob" from "bob/hello/poo". Caller must delete.
    static char     *GetOtherSubDirs    ( char *dirName );                      // eg returns "hello/poo" from above.  Doesn't create new data.

};



/*
 * ==============
 * DIRECTORY DATA
 * ==============
 */

#define DIRECTORY_TYPE_UNKNOWN  0
#define DIRECTORY_TYPE_INT      1
#define DIRECTORY_TYPE_FLOAT    2
#define DIRECTORY_TYPE_CHAR     3
#define DIRECTORY_TYPE_STRING   4
#define DIRECTORY_TYPE_BOOL     5
#define DIRECTORY_TYPE_VOID     6
#define DIRECTORY_TYPE_FIXED    7

#define DIRECTORY_SAFEINT        -1
#define DIRECTORY_SAFEFLOAT      -1.0f
#define DIRECTORY_SAFECHAR       '?'
#define DIRECTORY_SAFESTRING     "[SAFESTRING]"
#define DIRECTORY_SAFEBOOL       false


class DirectoryData
{
public:
    char    *m_name;
    int     m_type;
    
    int     m_int;                  // type 1
    float   m_float;                // type 2
    char    m_char;                 // type 3
    char    *m_string;              // type 4
    bool    m_bool;                 // type 5
    char    *m_void;                // type 6  
	
#ifdef FLOAT_NUMERICS
	double  m_fixed;                // type 7 (stored as raw double to guarantee memory representation)
#elif defined(FIXED64_NUMERICS)
	long long m_fixed;
#endif
    int     m_voidLen;

public:
    DirectoryData();
    ~DirectoryData();

    void SetName ( char *newName );
    void SetData ( int newData );
    void SetData ( float newData );
	void SetData ( Fixed newData );
    void SetData ( char newData );
    void SetData ( char *newData );
    void SetData ( bool newData );
    void SetData ( void *newData, int dataLen );

    void SetData ( DirectoryData *data );

    bool IsInt()    { return (m_type == DIRECTORY_TYPE_INT      ); }
    bool IsFloat()  { return (m_type == DIRECTORY_TYPE_FLOAT    ); }
	bool IsFixed()  { return (m_type == DIRECTORY_TYPE_FIXED    ); }
    bool IsChar()   { return (m_type == DIRECTORY_TYPE_CHAR     ); }
    bool IsString() { return (m_type == DIRECTORY_TYPE_STRING   ); }
    bool IsBool()   { return (m_type == DIRECTORY_TYPE_BOOL     ); }
    bool IsVoid()   { return (m_type == DIRECTORY_TYPE_VOID     ); }

    void DebugPrint ( int indent );
	void WriteXML ( std::ostream &o, int indent = 0 );

    // Saving / Loading to streams

    bool Read  ( std::istream &input );                              // returns false if something went wrong
    void Write ( std::ostream &output );

};


#endif
