#include "global.h"
#include "Dialog.h"
#include "DialogDriver.h"
#include "PrefsManager.h"
#include "RageUtil.h"

/* Hmm.  I don't want this to depend on other arch drivers--we should be able to test
 * this without linking to them.  We probably don't actually have to do that just by
 * #including the headers in arch_Win32.h, etc., but it's still messy ... */
#if defined(WIN32)
#include "DialogDriver_Win32.h"
#endif

#if defined(DARWIN)
#include "DialogDriver_Cocoa.h"
#endif

static DialogDriver *g_pImpl = NULL;
static DialogDriverNull g_pNullDriver;
static bool g_bWindowed = false;

void Dialog::Init()
{
	ASSERT( g_pImpl == NULL );

	CString drivers = "win32,cocoa,null";
	CStringArray DriversToTry;
	split(drivers, ",", DriversToTry, true);

	ASSERT( DriversToTry.size() != 0 );

	CString Driver;
	for( unsigned i = 0; g_pImpl == NULL && i < DriversToTry.size(); ++i )
	{
		try {
			Driver = DriversToTry[i];

#if defined(HAVE_DIALOG_WIN32)
			if( !DriversToTry[i].CompareNoCase("Win32") ) g_pImpl = new DialogDriver_Win32;
#endif
#if defined(HAVE_DIALOG_COCOA)
			if( !DriversToTry[i].CompareNoCase("Cocoa") ) g_pImpl = new DialogDriver_Cocoa;
#endif
			if( !DriversToTry[i].CompareNoCase("Null") ) g_pImpl = new DialogDriverNull;
		}
		catch(...)//const RageException &e)
		{
//			if( LOG )
//				LOG->Info("Couldn't load driver %s: %s", DriversToTry[i].c_str(), e.what());
		}
	}
}

void Dialog::Shutdown()
{
	ASSERT( g_pImpl != NULL );

}

static bool MessageIsIgnored( CString ID )
{
	vector<CString> list;
	split( PREFSMAN->m_sIgnoredMessageWindows, ",", list );
	for( unsigned i = 0; i < list.size(); ++i )
		if( !ID.CompareNoCase(list[i]) )
			return true;
	return false;
}

void Dialog::IgnoreMessage( CString ID )
{
	if( ID == "" )

	if( MessageIsIgnored(ID) )
		return;

	vector<CString> list;
	split( PREFSMAN->m_sIgnoredMessageWindows, ",", list );
	list.push_back( ID );
	PREFSMAN->m_sIgnoredMessageWindows = join( ",", list );
	PREFSMAN->SaveGlobalPrefsToDisk();
}

void Dialog::Error( CString sMessage, CString ID )
{
	if( ID != "" && MessageIsIgnored( ID ) )
		return;

	g_pImpl->Error( sMessage, ID );
}

void Dialog::SetWindowed( bool bWindowed )
{
	g_bWindowed = bWindowed;
}

void Dialog::OK( CString sMessage, CString ID )
{
	if( ID != "" && MessageIsIgnored( ID ) )
		return;

	// only show Dialog if windowed
	if( !g_bWindowed )
		g_pNullDriver.OK( sMessage, ID );
	else
		g_pImpl->OK( sMessage, ID );	// call derived version
}

Dialog::Result Dialog::AbortRetryIgnore( CString sMessage, CString ID )
{
	if( ID != "" && MessageIsIgnored( ID ) )
		return Dialog::AbortRetryIgnore( sMessage, ID );

	// only show Dialog if windowed
	if( !g_bWindowed )
		return g_pNullDriver.AbortRetryIgnore( sMessage, ID );
	else
		return g_pImpl->AbortRetryIgnore( sMessage, ID );	// call derived version
}

Dialog::Result Dialog::RetryCancel( CString sMessage, CString ID )
{
	if( ID != "" && MessageIsIgnored( ID ) )
		return Dialog::RetryCancel( sMessage, ID );

	// only show Dialog if windowed
	if( !g_bWindowed )
		return g_pNullDriver.RetryCancel( sMessage, ID );
	else
		return g_pImpl->RetryCancel( sMessage, ID );	// call derived version
}

/*
 * (c) 2003-2004 Glenn Maynard, Chris Danford
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