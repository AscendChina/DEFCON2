#include "lib/universal_include.h"
#include "lib/gucci/window_manager.h"
#include "statusicon.h"

#if defined(WIN32)
#include "lib/gucci/window_manager_win32.h"
#include "trayicon.h"
#elif defined(TARGET_OS_MACOSX)
#include "dockicon.h"
#elif defined(TARGET_OS_LINUX)
#include "x11icon.h"
#endif

StatusIcon *StatusIcon::Create()
{
#if defined(WIN32)
    GetWindowManagerWin32()->RegisterMessageHandler( TrayIcon::EventHandler );
    return new TrayIcon();
#elif defined(TARGET_OS_MACOSX)
    return new DockIcon();
#else
    return new X11Icon();
#endif
}
