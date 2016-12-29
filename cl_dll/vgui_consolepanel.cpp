//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date: 2006-10-26 18:02:06 $
//
//-----------------------------------------------------------------------------
// $Log: vgui_consolepanel.cpp,v $
// Revision 1.2  2006-10-26 18:02:06  Angry_Lawyer
// *** empty log message ***
//
//
// $NoKeywords: $
//=============================================================================//
#include "cbase.h"
#include "iconsole.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CConPanel;

class CConsole : public IConsole
{
private:
	CConPanel *conPanel;
public:
	CConsole( void )
	{
		conPanel = NULL;
	}
	
	void Create( vgui::VPANEL parent )
	{
		/*
		conPanel = new CConPanel( parent );
		*/
	}

	void Destroy( void )
	{
		/*
		if ( conPanel )
		{
			conPanel->SetParent( (vgui::Panel *)NULL );
			delete conPanel;
		}
		*/
	}
};

static CConsole g_Console;
IConsole *console = ( IConsole * )&g_Console;
