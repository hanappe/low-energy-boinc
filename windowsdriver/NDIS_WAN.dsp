# Microsoft Developer Studio Project File - Name="NDIS_WAN" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=NDIS_WAN - Win32 Win732
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "NDIS_WAN.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "NDIS_WAN.mak" CFG="NDIS_WAN - Win32 Win732"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "NDIS_WAN - Win32 Win732" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName "Perforce Project"
# PROP Scc_LocalPath "."
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe
# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "NDIS_WAN___Win32_Win732"
# PROP BASE Intermediate_Dir "NDIS_WAN___Win32_Win732"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Checked_Win732"
# PROP Intermediate_Dir "Checked_Win732"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /Gz /W3 /Z7 /Od /Gy /X /I "C:\WINDDK\3790\INC\DDK\WXP" /I "C:\WINDDK\3790\INC\CRT" /I "C:\WINDDK\3790\INC\WXP" /I "C:\WINDDK\3790\INC\DDK\WDM\WXP" /I "..\..\..\Modules\include" /I "..\..\..\Modules\GtNdis\\" /FI"C:\WINDDK\3790\INC\WXP\warning.h" /D "_WIN32" /D "DBG" /D "DEBUG" /D DDK_TARGET_OS=winxp /D _ddkspec=wxp /D _NT_TARGET_VERSION=0x501 /D NT_UP=0 /D PROCESSOR_ARCHITECTURE=x86 /D CPU=x86 /D _BUILDARCH=x86 /D NTDEBUGFILES=1 /D NTDEBUG=ntsd /D NTDEBUGTYPE=windbg /D DDKBUILDENV=chk /D WINVER=0x501 /D _X86_=1 /D "LOWER_FILTER" /FR /YX"precomp.h" /FD /Zel -cbstring /QIfdiv- /QIf /GF /c
# ADD CPP /nologo /Gz /ML /W3 /Z7 /Od /Gy /X /I "C:\WinDDK\7600.16385.1\inc\ddk" /I "C:\WinDDK\7600.16385.1\inc\crt" /I "C:\WinDDK\7600.16385.1\inc\api" /FI"C:\WinDDK\7600.16385.1\inc\api\warning.h" /D "_WIN32" /D "DBG" /D "DEBUG" /D DDK_TARGET_OS=win7 /D _ddkspec=win7 /D _NT_TARGET_VERSION=0x601 /D NT_UP=0 /D PROCESSOR_ARCHITECTURE=x86 /D CPU=x86 /D _BUILDARCH=x86 /D NTDEBUGFILES=1 /D NTDEBUG=ntsd /D NTDEBUGTYPE=windbg /D DDKBUILDENV=chk /D WINVER=0x601 /D _X86_=1 /D X86ARCH=1 /FAs /FR /FD /GF /GS- /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /o "NUL" /win32
# ADD BASE RSC /l 0x809 /x /i "C:\WINDDK\3790\inc\wxp" /i "..\..\..\Modules\GtNdis" /i "..\..\..\..\Modules\GtNdis\\" /d "_DEBUG" /d "NDIS50"
# ADD RSC /l 0x809 /x /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo /o"Checked_XP_Lower\browse.bsc"
# ADD BSC32 /nologo /o"Checked_Win732\browse.bsc"
LINK32=link.exe
# ADD BASE LINK32 ntoskrnl.lib hal.lib ntdll.lib usbd.lib ntstrsafe.lib /nologo /base:"0x10000" /version:4.0 /entry:"DriverEntry" /incremental:no /debug /machine:IX86 /nodefaultlib /out:"Checked_XP\xgfltr.sys" /libpath:"C:\WINDDK\3790\LIB\WXP\I386" /driver /IGNORE:4001,4037,4039,4065,4078,4087,4089,4096 /MERGE:_PAGE=PAGE /MERGE:_TEXT=.text /SECTION:INIT,d /MERGE:.rdata=.text /FULLBUILD /RELEASE /FORCE:MULTIPLE /OPT:REF /subsystem:native
# SUBTRACT BASE LINK32 /pdb:none
# ADD LINK32 ntoskrnl.lib hal.lib ntdll.lib usbd.lib ntstrsafe.lib /nologo /base:"0x10000" /version:4.0 /entry:"DriverEntry" /incremental:no /debug /machine:IX86 /nodefaultlib /out:"Checked_Win732\SonyFltr.sys" /libpath:"C:\WinDDK\7600.16385.1\lib\win7\i386" /driver /IGNORE:4001,4037,4039,4065,4078,4087,4089,4096 /MERGE:_PAGE=PAGE /MERGE:_TEXT=.text /SECTION:INIT,d /MERGE:.rdata=.text /FULLBUILD /RELEASE /FORCE:MULTIPLE /OPT:REF /subsystem:native /MANIFEST:NO
# SUBTRACT LINK32 /pdb:none
# Begin Target

# Name "NDIS_WAN - Win32 Win732"
# Begin Group "Source Files"

# PROP Default_Filter ".c;.cpp"
# Begin Source File

SOURCE=.\ACPI.c
# End Source File
# Begin Source File

SOURCE=.\entry.c
# End Source File
# Begin Source File

SOURCE=.\helper_asm.c
# End Source File
# Begin Source File

SOURCE=.\Thread.c
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter ".rc;.mc"
# End Group
# Begin Source File

SOURCE=.\SonyFilter.h
# End Source File
# End Target
# End Project
