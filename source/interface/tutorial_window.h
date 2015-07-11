#ifndef _included_tutorialwindow_h
#define _included_tutorialwindow_h

#include "interface/components/core.h"



class TutorialWindow : public EclWindow
{
public:
    bool m_explainingSomething;

public:
    TutorialWindow();

    void Create();
    void Render( bool _hasFocus );

};

class TutorialChapterWindow : public InterfaceWindow
{
public:
    TutorialChapterWindow();

    void Create();
};


#endif

