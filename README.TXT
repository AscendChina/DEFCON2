Start DefconX.sln (located in targets\msvc ) in Visual Studio 2010. Before compiling you must have installed DirectX SDK libraries .  Download MS DX libraries from http://www.microsoft.com/en-us/download/details.aspx?id=6812

You may run into DirectX Sound compile external link errors if you are using Windows 7 or Windows 64 bit. But I have been able to compile in Windows XP in VMWare. Consider compiling in Windows XP or Windows 7 32-bit instead of 64-bit. 

Add Include and Libdir in Visual Studio properties. 

1) Project->Properties->Configuration Properties-> C/C++->Additional Include Directories->edit --> $(DXSDK_DIR)\Include

2) Linker->Additional Library Directories->edit --> $(DXSDK_DIR)\Lib\x86;%(AdditionalLibraryDirectories)



Change resources/Script1.rc to avoid installing MFC Library(500Mb)
//#include "afxres.h"
#include "windows.h"


Compile in release mode. Debug has issues. And Start Defcon.exe in top directory.



If you need some authkey codes to test out the full version of the game, here are three working ones:

SABDMM-QWALZV-LJASSN-MBCFPE-GLE
MCLVIF-JZJBAP-UJGENT-KRCUIM-AXT
IBKVZO-GABISC-RWPVQC-STVHGN-JSE



