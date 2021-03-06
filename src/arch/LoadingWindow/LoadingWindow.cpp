#include "global.h"
#include "LoadingWindow.h"
#include "Preference.h"
#include "RageLog.h"
#include "arch/arch_default.h"

static Preference<bool> g_bShowLoadingWindow( "ShowLoadingWindow", true );

DriverList LoadingWindow::m_pDriverList;

REGISTER_LOADING_WINDOW( Null );

LoadingWindow *LoadingWindow::Create()
{
	if( !g_bShowLoadingWindow )
		return new LoadingWindow_Null;

#if defined(UNIX) && !defined(HAVE_GTK)
	return new LoadingWindow_Null;
#endif

	/* don't load Null by default. */
	const CString drivers = "xbox,win32,cocoa,gtk,sdl";
	vector<CString> DriversToTry;
	split( drivers, ",", DriversToTry, true );

	ASSERT( DriversToTry.size() != 0 );

	CString Driver;
	LoadingWindow *ret = NULL;

	/* try all the drivers until we get a match */
	for( unsigned i = 0; i < DriversToTry.size() && ret == NULL; ++i )
	{
		Driver = DriversToTry[i];
		ret = dynamic_cast<LoadingWindow*>(LoadingWindow::m_pDriverList.Create( Driver ));

		if( ret == NULL )
			continue;

		CString sError = ret->Init();
		if( !sError.empty() )
		{
			LOG->Info( "Couldn't load driver %s: %s", Driver.c_str(), sError.c_str() );
			SAFE_DELETE( ret );
		}
	}

	if( ret )
		LOG->Info( "Loading window: %s", Driver.c_str() );

	return ret;
}

/*
 * Copyright (c) 2009 BoXoRRoXoRs
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

