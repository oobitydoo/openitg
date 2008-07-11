#include "global.h"
#include "RageLog.h"
#include "RageUtil.h"
#include "RageTimer.h"
#include "RageInput.h" /* for g_sInputType */
#include "ProfileManager.h"
#include "SongManager.h"
#include "LuaManager.h"
#include "DiagnosticsUtil.h"

#include "XmlFile.h"
#include "ProductInfo.h"
#include "io/ITGIO.h"
#include "io/USBDevice.h"

/* Include dongle support if we're going to use it...
 * Maybe we should set a HAVE_DONGLE directive? */
#ifdef ITG_ARCADE
extern "C" {
#include "ibutton/ownet.h"
#include "ibutton/shaib.h" 
}
#endif

// include Linux networking functions/types
#ifndef WIN32
extern "C" {
#include <ifaddrs.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
}
#endif

// "Stats" is mounted to "Data" in the VFS for non-arcade
#if defined(ITG_ARCADE) && !defined(WIN32)
#define STATS_DIR_PATH CString("/rootfs/stats/")
#else
#define STATS_DIR_PATH CString("Data/")
#endif

extern CString g_sInputType;

int DiagnosticsUtil::GetNumCrashLogs()
{
	CStringArray aLogs;
	GetDirListing( STATS_DIR_PATH + "crashinfo-*.txt" , aLogs );
	return aLogs.size();
}

int DiagnosticsUtil::GetNumMachineEdits()
{
	CStringArray aEdits;
	CString sDir = PROFILEMAN->GetProfileDir(PROFILE_SLOT_MACHINE) + EDIT_SUBDIR;
	GetDirListing( sDir , aEdits );
	return aEdits.size();
}

// XXX: Win32 compatibility
CString DiagnosticsUtil::GetIP()
{
#ifndef WIN32
	struct ifaddrs *ifaces;
	if (getifaddrs(&ifaces) != 0) return "Network interface error (getifaddrs() failed)";

	CString result = "";

	for ( struct ifaddrs *iface = ifaces; iface; iface = iface->ifa_next )
	{
		// 0x1000 = uses broadcast
		if ((iface->ifa_flags & 0x1000) == 0 || (iface->ifa_addr->sa_family != AF_INET)) continue;

		struct sockaddr_in *sad = NULL;
		struct sockaddr_in *snm = NULL;

		sad = (struct sockaddr_in *)iface->ifa_addr;
		snm = (struct sockaddr_in *)iface->ifa_netmask;
		result += inet_ntoa(((struct sockaddr_in *)sad)->sin_addr);
		result += CString(", Netmask: ") + inet_ntoa(((struct sockaddr_in *)snm)->sin_addr);
		freeifaddrs(ifaces);
		return result;
	}

	freeifaddrs(ifaces);
#else
	// Win32 code goes here
#endif

	/* fall through */
	return "Network interface disabled";
}

int DiagnosticsUtil::GetRevision()
{
	CString sPath = STATS_DIR_PATH + "patch/patch.xml";

	// Create the XML Handler, and clear it, for practice.
	XNode *xml = new XNode;
	xml->Clear();
	xml->m_sName = "patch";
	
	// Check for the file existing
	if( !IsAFile(sPath) )
	{
		LOG->Warn( "There is no patch file (patch.xml)" );
		SAFE_DELETE( xml ); 
		return 1;
	}
	
	// Make sure you can read it
	if( !xml->LoadFromFile(sPath) )
	{
		LOG->Warn( "patch.xml unloadable" );
		SAFE_DELETE( xml ); 
		return 1;
	}
	
	// Check the node <Revision>x</Revision>
	if( !xml->GetChild( "Revision" ) )
	{
		LOG->Warn( "Revision node missing! (patch.xml)" );
		SAFE_DELETE( xml ); 
		return 1;
	}
	
	int iRevision = atoi( xml->GetChild("Revision")->m_sValue );

	return iRevision;
}

int DiagnosticsUtil::GetNumMachineScores()
{
	CString sXMLPath = STATS_DIR_PATH + "/MachineProfile/Stats.xml";

	// Create the XML Handler and clear it, for practice
	XNode *xml = new XNode;
	xml->Clear();
	
	// Check for the file existing
	if( !IsAFile(sXMLPath) )
	{
		LOG->Warn( "There is no Stats.xml file!" );
		SAFE_DELETE( xml ); 
		return 0;
	}
	
	// Make sure you can read it
	if( !xml->LoadFromFile(sXMLPath) )
	{
		LOG->Trace( "Stats.xml unloadable!" );
		SAFE_DELETE( xml ); 
		return 0;
	}
	
	const XNode *pData = xml->GetChild( "SongScores" );
	
	if( pData == NULL )
	{
		LOG->Warn( "Error loading scores: <SongScores> node missing" );
		SAFE_DELETE( xml ); 
		return 0;
	}
	
	unsigned int iScoreCount = 0;
	
	// Named here, for LoadFromFile() renames it to "Stats"
	xml->m_sName = "SongScores";
	
	// For each pData Child, or the Child in SongScores...
	FOREACH_CONST_Child( pData , p )
		iScoreCount++;

	SAFE_DELETE( xml ); 

	return iScoreCount;
}

CString DiagnosticsUtil::GetSerialNumber()
{
	static CString g_SerialNum;

	if ( !g_SerialNum.empty() )
		return g_SerialNum;

#ifdef ITG_ARCADE
	SHACopr copr;
	CString sNewSerial;
	uchar spBuf[32];

#ifdef WIN32
	if ( (copr.portnum = owAcquireEx("COM1")) == -1 )
#else
	if ( (copr.portnum = owAcquireEx("/dev/ttyS0")) == -1 )
#endif
	{
		LOG->Warn("Failed to get machine serial, unable to acquire port");
		return "????????";
	}
	FindNewSHA(copr.portnum, copr.devAN, true);
	ReadAuthPageSHA18(copr.portnum, 9, spBuf, NULL, false);
	owRelease(copr.portnum);

	sNewSerial = (char*)spBuf;
	TrimLeft(sNewSerial);
	TrimRight(sNewSerial);
	g_SerialNum = sNewSerial;

	return sNewSerial;
#endif

	g_SerialNum = GenerateDebugSerial();

	return g_SerialNum;
}

/* this allows us to use the serial numbers on builds for
 * more helpful debugging information. PRODUCT_BUILD_DATE
 * is defined in ProductInfo.h */
CString DiagnosticsUtil::GenerateDebugSerial()
{
	CString sSerial, sSystem, sBuildType;

	sSerial = "OITG-";

#if defined(WIN32)
	sSystem = "W"; /* Windows */
#elif defined(LINUX)
	sSystem = "L"; /* Unix/Linux */
#elif defined(DARWIN)
	sSystem = "M"; /* Mac OS */
#else
	sSystem = "U"; /* unknown */
#endif
	// "U-04292008-"
	sSerial += sSystem + "-" + CString(PRODUCT_BUILD_DATE) + "-";

#ifdef ITG_ARCADE
	sBuildType = "A";
#else
	sBuildType = "P";
#endif

	// "573-A"
	sSerial += "573-" + sBuildType;

	return sSerial;
}

bool DiagnosticsUtil::HubIsConnected()
{
	vector<USBDevice> vDevices;
	GetUSBDeviceList( vDevices );

	/* Hub can't be connected if there are no devices. */
	if( vDevices.size() == 0 )
		return false;

	for( unsigned i = 0; i < vDevices.size(); i++ )
		if( vDevices[i].IsHub() )
			return true;

	return false;
}

CString DiagnosticsUtil::GetInputType()
{
	return g_sInputType;
}

void SetProgramGlobal( lua_State* L )
{
	LUA->SetGlobal( "OPENITG", true );
}

// LUA bindings for diagnostics functions

#include "LuaFunctions.h"

LuaFunction_NoArgs( GetProductName,		CString( PRODUCT_NAME_VER ) ); // Return the product's name from ProductInfo.h
LuaFunction_NoArgs( GetUptime,			SecondsToHHMMSS( RageTimer::GetTimeSinceStart() ) ); 

LuaFunction_NoArgs( GetNumIOErrors,		ITGIO::m_iInputErrorCount );

// diagnostics enumeration functions
LuaFunction_NoArgs( GetNumCrashLogs,		DiagnosticsUtil::GetNumCrashLogs() );
LuaFunction_NoArgs( GetNumMachineScores,	DiagnosticsUtil::GetNumMachineScores() );
LuaFunction_NoArgs( GetNumMachineEdits, 	DiagnosticsUtil::GetNumMachineEdits() );
LuaFunction_NoArgs( GetRevision,		DiagnosticsUtil::GetRevision() );

// arcade diagnostics
LuaFunction_NoArgs( GetIP,			DiagnosticsUtil::GetIP() );
LuaFunction_NoArgs( GetSerialNumber,		DiagnosticsUtil::GetSerialNumber() );
LuaFunction_NoArgs( HubIsConnected,		DiagnosticsUtil::HubIsConnected() );
LuaFunction_NoArgs( GetInputType,		DiagnosticsUtil::GetInputType() );

// set "OPENITG" as a global boolean for usage in scripting
REGISTER_WITH_LUA_FUNCTION( SetProgramGlobal );
/*
 * (c) 2008 BoXoRRoXoRs
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, and/or sell copies of the Software, and to permit persons to
 * whom the Software is furnished to do so, provided that the above
 * copyright notice(s) and this permission notice appear in all copies of
 * the Software and that both the above copyright notice(s) and this
 * permission notice appear in supporting documentation.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF
 * THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS
 * INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT
 * OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */