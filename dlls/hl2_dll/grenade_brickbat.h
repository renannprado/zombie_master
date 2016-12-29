//========= Copyright � 1996-2005, Valve Corporation, All rights reserved. ============//
//
// Purpose:		Things thrown from the hand 
//
// $Workfile:     $
// $Date: 2006-10-26 18:02:39 $
//
//-----------------------------------------------------------------------------
// $Log: grenade_brickbat.h,v $
// Revision 1.2  2006-10-26 18:02:39  Angry_Lawyer
// *** empty log message ***
//
//
// $NoKeywords: $
//=============================================================================//

#ifndef	GRENADEBRICKBAT_H
#define	GRENADEBRICKBAT_H

#include "basegrenade_shared.h"

enum BrickbatAmmo_t;

class CGrenade_Brickbat : public CBaseGrenade
{
public:
	DECLARE_CLASS( CGrenade_Brickbat, CBaseGrenade );

	virtual void	Spawn( void );
	virtual void	SpawnBrickbatWeapon( void );
	virtual void	Detonate( void ) { return;};
	virtual bool	CreateVPhysics();
	void			BrickbatTouch( CBaseEntity *pOther );
	void			BrickbatThink( void );

	BrickbatAmmo_t	m_nType;
	bool			m_bExplodes;
	bool			m_bBounceToFlat;	// Bouncing to flatten

public:
	DECLARE_DATADESC();
};

#endif	//GRENADEBRICKBAT_H
