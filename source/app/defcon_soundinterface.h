#ifndef _included_defconsoundinterface_h
#define _included_defconsoundinterface_h


#include "lib/sound/soundsystem_interface.h"


class DefconSoundInterface : public SoundSystemInterface
{
public:
    bool    GetCameraPosition       ( Vector3<float> &_pos, Vector3<float> &_front, Vector3<float> &_up, Vector3<float> &_vel );

    bool    ListObjectTypes         ( LList<char *> *_list );
    bool    ListObjectEvents        ( LList<char *> *_list, char *_objType );
        
    bool    DoesObjectExist         ( SoundObjectId _id );
    char   *GetObjectType           ( SoundObjectId _id );
    bool    GetObjectPosition       ( SoundObjectId _id, Vector3<float> &_pos, Vector3<float> &_vel );

    bool    ListProperties          ( LList<char *> *_list );
    bool    GetPropertyRange        ( char *_property, float *_min, float *_max );
    float   GetPropertyValue        ( char *_property, SoundObjectId _id );    
};



#endif
