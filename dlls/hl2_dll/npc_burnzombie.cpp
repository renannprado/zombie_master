//=============================================================================
// Copyright (c) Zombie Master Development Team. All rights reserved.
// The use and distribution terms for this software are covered by the MIT
// License (http://opensource.org/licenses/mit-license.php) which
// can be found in the file LICENSE.TXT at the root of this distribution. By
// using this software in any fashion, you are agreeing to be bound by the
// terms of this license. You must not remove this notice, or any other, from
// this software.
//
// Note that due to the number of files included in the SDK, it is not feasible
// to include this notice in all of them. All original files or files 
// containing large modifications should contain this notice. If in doubt,
// assume the above notice applies, and refer to the included LICENSE.TXT text.
//=============================================================================
//
// Purpose: A horrible, screaming, igniting Zombie.  It gets close to a player
//			and torches itself, causing it to go into a frenzy.
//
//=============================================================================//
#include "cbase.h"

#include "doors.h"

#include "simtimer.h"
#include "npc_BaseZombie.h"
#include "ai_hull.h"
#include "ai_navigator.h"
#include "ai_memory.h"
#include "gib.h"
//#include "soundenvelope.h"
#include "props.h"
#include "fire.h"
#include "explode.h"
#include "engine/IEngineSound.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"


//#define TGB_DISMEMBER 0

ConVar	sk_burnzombie_health( "sk_burnzombie_health","110");
#define BURNZOMBIE_ENEMY_DIST		250	// How close we must be to our enemy before we blow up
//ConVar	happy_zombies( "happyzombies","0");
//ConVar	debug_skinchange_test( "debug_skinchange_test","0", FCVAR_CHEAT);

//static ConVar zm_popcost_immolator("zm_popcost_immolator", "5", FCVAR_NOTIFY | FCVAR_REPLICATED, "Population points taken up by immolators");

//=============================================================================
//=============================================================================
//LAWYER:  Before you yell at me, TGB, for cargo-cult coding, a lot of the Zombie "Run around when on fire"
//			seems really botched together.  Whenever I try to clear crap from this file,
//			Immolators seem to lose the ability ro run.  So it's going to be a slow process.
class CNPC_BurnZombie : public CAI_BlendingHost<CNPC_BaseZombie>
{
	DECLARE_DATADESC();
	DECLARE_CLASS( CNPC_BurnZombie, CAI_BlendingHost<CNPC_BaseZombie> );

	//TGB: we have a client of this now
	DECLARE_SERVERCLASS();

public:
	CNPC_BurnZombie()
	{
		//TGB CYCLEDEBUG
		//TGB: keeping all of this to basezombie
		//UseClientSideAnimation(); //LAWYER:  Hack

	}

	void Spawn( void );
	void Precache( void );

	Class_T Classify( void );
	Disposition_t IRelationType( CBaseEntity *pTarget );

	bool CanSwatPhysicsObjects( void ) { return false; } //TGB: can't swat

	void SetZombieModel( void );

	//TGB: set randomised zombie expression
	//void SetRandomExpression ( void );

//	void MoanSound( envelopePoint_t *pEnvelope, int iEnvelopeSize );
	bool ShouldBecomeTorso( const CTakeDamageInfo &info, float flDamageThreshold ){return false;}
	bool CanBecomeLiveTorso() { return false; }
	void TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr ); // qck edit

	void GatherConditions( void );

	int SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode );
	int TranslateSchedule( int scheduleType );

	//TGB: use default flinch checks
	//void CheckFlinches() {} // Zombie has custom flinch code

	void PostscheduleThink( void );

	Activity NPC_TranslateActivity( Activity newActivity );

	void OnStateChange( NPC_STATE OldState, NPC_STATE NewState );

	void StartTask( const Task_t *pTask );
	void RunTask( const Task_t *pTask );

	void Ignite( float flFlameLifetime, bool bNPCOnly = true, float flSize = 0.0f, bool bCalledByLevelDesigner = false );
	void Extinguish();
	int OnTakeDamage_Alive( const CTakeDamageInfo &inputInfo );
//	bool IsSquashed( const CTakeDamageInfo &info );
	void BuildScheduleTestBits( void );

	void PrescheduleThink( void );
	int SelectSchedule ( void );

	void PainSound( const CTakeDamageInfo &info );
	void DeathSound(const CTakeDamageInfo &info);
	void AlertSound( void );
	void IdleSound( void );
	void AttackSound( void );
	void AttackHitSound( void );
	void AttackMissSound( void );
	void FootstepSound( bool fRightFoot );
	void FootscuffSound( bool fRightFoot );

	const char *GetMoanSound( int nSound );
	//virtual void Event_Killed( const CTakeDamageInfo &info );
	
	void Detonate( void );
	void StartFires( void );

	int GetPopCost() { return zm_popcost_immolator.GetInt(); }

	int	GetZombieType() { return TYPE_IMMOLATOR; }
public:
	DEFINE_CUSTOM_AI;

protected:
	static const char *pMoanSounds[];


private:

	bool				 m_bIsSlumped;
	
	Vector				 m_vPositionCharged;
};

LINK_ENTITY_TO_CLASS( npc_burnzombie, CNPC_BurnZombie );
//LINK_ENTITY_TO_CLASS( npc_zombie_torso, CNPC_BurnZombie );

//---------------------------------------------------------
//---------------------------------------------------------
const char *CNPC_BurnZombie::pMoanSounds[] =
{
	 "NPC_BaseZombie.Moan1",
	 "NPC_BaseZombie.Moan2",
	 "NPC_BaseZombie.Moan3",
	 "NPC_BaseZombie.Moan4",
};

//=========================================================
// Conditions
//=========================================================
enum
{
	COND_BLOCKED_BY_DOOR = LAST_BASE_ZOMBIE_CONDITION,
	COND_DOOR_OPENED,
	COND_ZOMBIE_CHARGE_TARGET_MOVED,
};

//=========================================================
// Schedules
//=========================================================
enum
{
	SCHED_ZOMBIE_BASH_DOOR = LAST_BASE_ZOMBIE_SCHEDULE,
	SCHED_ZOMBIE_WANDER_ANGRILY,
	SCHED_ZOMBIE_CHARGE_ENEMY,
	SCHED_ZOMBIE_FAIL,
};

//=========================================================
// Tasks
//=========================================================
enum
{
	TASK_ZOMBIE_EXPRESS_ANGER = LAST_BASE_ZOMBIE_TASK,
	TASK_ZOMBIE_YAW_TO_DOOR,
	TASK_ZOMBIE_ATTACK_DOOR,
	TASK_ZOMBIE_CHARGE_ENEMY,
};

//-----------------------------------------------------------------------------

int ACT_BURNZOMBIE_TANTRUM;
int ACT_BURNZOMBIE_WALLPOUND;

BEGIN_DATADESC( CNPC_BurnZombie )

	DEFINE_FIELD( m_bIsSlumped, FIELD_BOOLEAN ),
	DEFINE_FIELD( m_vPositionCharged, FIELD_POSITION_VECTOR ),

END_DATADESC()

IMPLEMENT_SERVERCLASS_ST( CNPC_BurnZombie, DT_NPC_BurnZombie )
END_SEND_TABLE()


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_BurnZombie::Precache( void )
{
	BaseClass::Precache();

	PrecacheModel( "models/zombie/burnzie.mdl" );

	PrecacheScriptSound( "Zombie.FootstepRight" );
	PrecacheScriptSound( "Zombie.FootstepLeft" );
	PrecacheScriptSound( "Zombie.FootstepLeft" );
	PrecacheScriptSound( "Zombie.ScuffRight" );
	PrecacheScriptSound( "Zombie.ScuffLeft" );
	PrecacheScriptSound( "Zombie.AttackHit" );
	PrecacheScriptSound( "Zombie.AttackMiss" );
	PrecacheScriptSound( "NPC_BurnZombie.Pain" );
	PrecacheScriptSound( "NPC_BurnZombie.Die" );
	PrecacheScriptSound( "NPC_BurnZombie.Alert" );
	PrecacheScriptSound( "NPC_BurnZombie.Idle" );
	//PrecacheScriptSound( "NPC_BurnZombie.Attack" ); //don't have this
	PrecacheScriptSound( "NPC_BurnZombie.Scream" );

}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
Class_T CNPC_BurnZombie::Classify( void )
{
	if ( m_bIsSlumped )
		return CLASS_NONE;

	return BaseClass::Classify();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
Disposition_t CNPC_BurnZombie::IRelationType( CBaseEntity *pTarget )
{
	// Slumping should not affect Zombie's opinion of others
	if ( m_bIsSlumped )
	{
		m_bIsSlumped = false;
		Disposition_t result = BaseClass::IRelationType( pTarget );
		m_bIsSlumped = true;
		return result;
	}
	
	//qck: Special check to make sure zombies don't get pissy about snipers taking pot shots at them. 
	if (pTarget->IsNPC())
	{
		return D_NU;
	}

	return BaseClass::IRelationType( pTarget );
}

void CNPC_BurnZombie::TraceAttack( const CTakeDamageInfo &info, const Vector &vecDir, trace_t *ptr )
{

	CTakeDamageInfo infoCopy = info;

	UTIL_BloodSpray( ptr->endpos, vecDir, BLOOD_COLOR_RED, RandomInt( 4, 8 ), FX_BLOODSPRAY_ALL);
	//SpawnBlood(ptr->endpos, vecDir, BloodColor(), info.GetDamage());// a little surface blood.
	TraceBleed( info.GetDamage(), vecDir, ptr, info.GetDamageType() );

	BaseClass::TraceAttack( infoCopy, vecDir, ptr );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_BurnZombie::Spawn( void )
{
	Precache();

	m_fIsTorso = false;

	m_fIsHeadless = false;

	SetBloodColor( BLOOD_COLOR_RED );
	m_iHealth			= sk_burnzombie_health.GetFloat();
	//TGB: 0.2 is about 80 degrees to either side
//	m_flFieldOfView		= 0.2;
	//TGB: try bigger
	m_flFieldOfView		= -0.975f;

	m_bIsSlumped = false;

	CapabilitiesClear();

	//CapabilitiesAdd( bits_CAP_ANIMATEDFACE );

	//GetNavigator()->SetRememberStaleNodes( false );

	BaseClass::Spawn();

	m_flNextMoanSound = gpGlobals->curtime + random->RandomFloat( 1.0, 4.0 );

	//TGB: make sure we start doing burn damage as soon as we're on fire
	m_flNextBurnFrame = 0;
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_BurnZombie::PrescheduleThink( void )
{
	//LAWYER:  Check for drowning!
	if (this->GetWaterLevel() >= 2)
	{
		this->Extinguish();
		CTakeDamageInfo info;

		info.SetDamage( this->m_iHealth );
		info.SetAttacker( this );
		info.SetInflictor( (CBaseEntity *)this );
		info.SetDamageType( DMG_GENERIC );

		this->TakeDamage( info );

	}

	//LAWYER:  Check for ignition!
	if ( GetEnemy() != NULL )
	{
		float flDist = (GetEnemy()->GetAbsOrigin() - GetAbsOrigin()).Length();
		if (flDist < BURNZOMBIE_ENEMY_DIST)
		{
			//LAWYER:  Time to go up in flames!  Muhahaha!
			if (!this->IsOnFire())
			{
				this->Ignite(100.0f);

				SetIdealActivity(ACT_WALK_ON_FIRE);
			}
		}
	}
	//LAWYER:  Heat field effect.  No longer duplicated, because other zombies no longer propogate fire

	trace_t		tr;
	Vector		vecSpot;
	Vector		vecSource = this->GetAbsOrigin();
	vecSource.z += 32;
	CBaseEntity *pObject = NULL;

//	const int MASK_RADIUS_DAMAGE = MASK_SHOT&(~CONTENTS_HITBOX);

	if ( m_flNextBurnFrame <= gpGlobals->curtime && IsOnFire())
	{	
		while ( ( pObject = gEntList.FindEntityInSphere( pObject, this->GetAbsOrigin(), 132 ) ) != NULL )
		{
			vecSpot = pObject->BodyTarget( vecSource, false );
			UTIL_TraceLine( vecSource, vecSpot, MASK_SOLID, NULL, COLLISION_GROUP_NONE, &tr );

			if ( tr.fraction == 1.0)
			{
				if ((pObject->IsPlayer() || pObject->IsNPC()) && pObject!=this)
				{
					//LAWYER:  It's alive.  Give it a bit of burnination!
					CBaseCombatCharacter *pOther = dynamic_cast<CBaseCombatCharacter *>(pObject);
					if (pOther)
					{
						if (!pOther->IsOnFire() && pOther->GetWaterLevel() < 3) //Don't do damage to stuff on fire.  Stuff underwater is immune
						{
							CTakeDamageInfo info( this, this, 10, DMG_BURN );
							if (pOther->GetHealth() <= pOther->GetMaxHealth()/5)
							{
								pOther->Ignite(100.0f);

							}
					
							pOther->TakeDamage( info );
						}
					}
				}
				else
				{
					CBreakableProp *pProp = dynamic_cast<CBreakableProp *>(pObject);
					if (pProp)
					{
						pProp->Ignite(100.0f,false);
					}
				}
			}
			
		}
		m_flNextBurnFrame = gpGlobals->curtime + 1.0f;
	}

	BaseClass::PrescheduleThink();
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
int CNPC_BurnZombie::SelectSchedule ( void )
{
	if( HasCondition( COND_PHYSICS_DAMAGE ) )
	{
		return SCHED_FLINCH_PHYSICS;
	}

	return BaseClass::SelectSchedule();
}

//-----------------------------------------------------------------------------
// Purpose: Sound of a footstep
//-----------------------------------------------------------------------------
void CNPC_BurnZombie::FootstepSound( bool fRightFoot )
{
	if( fRightFoot )
	{
		EmitSound(  "Zombie.FootstepRight" );
	}
	else
	{
		EmitSound( "Zombie.FootstepLeft" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Sound of a foot sliding/scraping
//-----------------------------------------------------------------------------
void CNPC_BurnZombie::FootscuffSound( bool fRightFoot )
{
	if( fRightFoot )
	{
		EmitSound( "Zombie.ScuffRight" );
	}
	else
	{
		EmitSound( "Zombie.ScuffLeft" );
	}
}

//-----------------------------------------------------------------------------
// Purpose: Play a random attack hit sound
//-----------------------------------------------------------------------------
void CNPC_BurnZombie::AttackHitSound( void )
{
	EmitSound( "Zombie.AttackHit" );
}

//-----------------------------------------------------------------------------
// Purpose: Play a random attack miss sound
//-----------------------------------------------------------------------------
void CNPC_BurnZombie::AttackMissSound( void )
{
	// Play a random attack miss sound
	EmitSound( "Zombie.AttackMiss" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_BurnZombie::PainSound( const CTakeDamageInfo &info )
{
	// We're constantly taking damage when we are on fire. Don't make all those noises!
	if ( IsOnFire() )
	{
		return;
	}

	EmitSound( "NPC_BurnZombie.Pain" );
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void CNPC_BurnZombie::DeathSound(const CTakeDamageInfo &info) 
{
	EmitSound( "NPC_BurnZombie.Die" );
}

//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
void CNPC_BurnZombie::AlertSound( void )
{
	//TGB: alert sound makes the zombie useless for surprising peeps, so off for now as requested by testies
// 	EmitSound( "NPC_BurnZombie.Alert" );
// 
// 	// Don't let a moan sound cut off the alert sound.
// 	m_flNextMoanSound += random->RandomFloat( 2.0, 4.0 );
}

//-----------------------------------------------------------------------------
// Purpose: Returns a moan sound for this class of zombie.
//-----------------------------------------------------------------------------
const char *CNPC_BurnZombie::GetMoanSound( int nSound )
{
	return pMoanSounds[ nSound % ARRAYSIZE( pMoanSounds ) ];
}

//-----------------------------------------------------------------------------
// Purpose: Play a random idle sound.
//-----------------------------------------------------------------------------
void CNPC_BurnZombie::IdleSound( void )
{
	if( GetState() == NPC_STATE_IDLE && random->RandomFloat( 0, 1 ) == 0 )
	{
		// Moan infrequently in IDLE state.
		return;
	}

	if( m_bIsSlumped )
	{
		// Sleeping zombies are quiet.
		return;
	}

	EmitSound( "NPC_BurnZombie.Idle" );
}

//-----------------------------------------------------------------------------
// Purpose: Play a random attack sound.
//-----------------------------------------------------------------------------
void CNPC_BurnZombie::AttackSound( void )
{
//	EmitSound( "Zombie.Attack" );
}

//-----------------------------------------------------------------------------
// Purpose: Returns the classname (ie "npc_headcrab") to spawn when our headcrab bails.
//-----------------------------------------------------------------------------
/*const char *CNPC_BurnZombie::GetHeadcrabClassname( void )
{
	return "npc_headcrab";
}
*/
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
/*const char *CNPC_BurnZombie::GetHeadcrabModel( void )
{
	return "models/headcrabclassic.mdl";
}
*/
//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
//const char *CNPC_BurnZombie::GetLegsModel( void )
//{
//	return "models/zombie/classic_legs.mdl";
//}
//
////-----------------------------------------------------------------------------
////-----------------------------------------------------------------------------
//const char *CNPC_BurnZombie::GetTorsoModel( void )
//{
//	return "models/zombie/classic_torso.mdl";
//}


//---------------------------------------------------------
//---------------------------------------------------------
void CNPC_BurnZombie::SetZombieModel( void )
{
	Hull_t lastHull = GetHullType();

	SetModel("models/zombie/burnzie.mdl");

	SetHullType( HULL_HUMAN );

	SetHullSizeNormal( true );
	SetDefaultEyeOffset();
	SetActivity( ACT_IDLE );

	//set random skin, will have no effect if the model doesn't specify more than one (which it doesn't by def.)
	m_nSkin = random->RandomInt( 0, 3 );
	

	// hull changed size, notify vphysics
	// UNDONE: Solve this generally, systematically so other
	// NPCs can change size
	if ( lastHull != GetHullType() )
	{
		if ( VPhysicsGetObject() )
		{
			SetupVPhysicsHull();
		}
	}

}


//---------------------------------------------------------
// Classic zombie only uses moan sound if on fire.
//---------------------------------------------------------
/*void CNPC_BurnZombie::MoanSound( envelopePoint_t *pEnvelope, int iEnvelopeSize )
{
	if( IsOnFire() )
	{
		BaseClass::MoanSound( pEnvelope, iEnvelopeSize );
	}
}
*/
//---------------------------------------------------------
//---------------------------------------------------------

//---------------------------------------------------------
//---------------------------------------------------------
void CNPC_BurnZombie::GatherConditions( void )
{
	BaseClass::GatherConditions();

	static int conditionsToClear[] = 
	{
		COND_ZOMBIE_CHARGE_TARGET_MOVED,
	};

	ClearConditions( conditionsToClear, ARRAYSIZE( conditionsToClear ) );


	if ( ConditionInterruptsCurSchedule( COND_ZOMBIE_CHARGE_TARGET_MOVED ) )
	{
		if ( GetNavigator()->IsGoalActive() )
		{
			const float CHARGE_RESET_TOLERANCE = 60.0;
			if ( !GetEnemy() ||
				 ( m_vPositionCharged - GetEnemyLKP()  ).Length() > CHARGE_RESET_TOLERANCE )
			{
				SetCondition( COND_ZOMBIE_CHARGE_TARGET_MOVED );
			}
				 
		}
	}
}

//---------------------------------------------------------
//---------------------------------------------------------

int CNPC_BurnZombie::SelectFailSchedule( int failedSchedule, int failedTask, AI_TaskFailureCode_t taskFailCode )
{

	/* TGB: burnzie can't actually do these
	if ( failedSchedule != SCHED_ZOMBIE_CHARGE_ENEMY && 
		 IsPathTaskFailure( taskFailCode ) &&
		 random->RandomInt( 1, 100 ) < 50 )
	{
		return SCHED_ZOMBIE_CHARGE_ENEMY;
	}

	if ( failedSchedule != SCHED_ZOMBIE_WANDER_ANGRILY &&
		 ( failedSchedule == SCHED_TAKE_COVER_FROM_ENEMY || 
		   failedSchedule == SCHED_CHASE_ENEMY_FAILED ) )
	{
		return SCHED_ZOMBIE_WANDER_ANGRILY;
	}
	*/

	return BaseClass::SelectFailSchedule( failedSchedule, failedTask, taskFailCode );
}

//---------------------------------------------------------
//---------------------------------------------------------

int CNPC_BurnZombie::TranslateSchedule( int scheduleType )
{
	if ( scheduleType == SCHED_COMBAT_FACE && IsUnreachable( GetEnemy() ) )
		return SCHED_TAKE_COVER_FROM_ENEMY;

	if ( !m_fIsTorso && scheduleType == SCHED_FAIL )
		return SCHED_ZOMBIE_FAIL;

	return BaseClass::TranslateSchedule( scheduleType );
}

//---------------------------------------------------------

void CNPC_BurnZombie::PostscheduleThink( void )
{
	int sequence = GetSequence();
	if ( sequence != -1 )
	{
		m_bIsSlumped = ( strncmp( GetSequenceName( sequence ), "slump", 5 ) == 0 );
	}
}

//-----------------------------------------------------------------------------
// Purpose:
// Input  :
//-----------------------------------------------------------------------------
/*void CNPC_BurnZombie::Event_Killed( const CTakeDamageInfo &info )
{

	BaseClass::Event_Killed( info );
}*/
//---------------------------------------------------------

Activity CNPC_BurnZombie::NPC_TranslateActivity( Activity newActivity )
{
	newActivity = BaseClass::NPC_TranslateActivity( newActivity );

	if ( newActivity == ACT_RUN )
		return ACT_WALK;

	return newActivity;
}

//---------------------------------------------------------
//---------------------------------------------------------
void CNPC_BurnZombie::OnStateChange( NPC_STATE OldState, NPC_STATE NewState )
{
	BaseClass::OnStateChange( OldState, NewState );
}

//---------------------------------------------------------
//---------------------------------------------------------

void CNPC_BurnZombie::StartTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{
	case TASK_ZOMBIE_EXPRESS_ANGER:
		{
			if ( random->RandomInt( 1, 4 ) == 2 )
			{
				SetIdealActivity( (Activity)ACT_BURNZOMBIE_TANTRUM );
			}
			else
			{
				TaskComplete();
			}

			break;
		}

	case TASK_ZOMBIE_CHARGE_ENEMY:
		{
			if ( !GetEnemy() )
				TaskFail( FAIL_NO_ENEMY );
			else if ( GetNavigator()->SetVectorGoalFromTarget( GetEnemy()->GetLocalOrigin() ) )
			{
				m_vPositionCharged = GetEnemy()->GetLocalOrigin();
				TaskComplete();
			}
			else
				TaskFail( FAIL_NO_ROUTE );
			break;
		}

	default:
		BaseClass::StartTask( pTask );
		break;
	}
}

//---------------------------------------------------------
//---------------------------------------------------------

void CNPC_BurnZombie::RunTask( const Task_t *pTask )
{
	switch( pTask->iTask )
	{

	case TASK_ZOMBIE_CHARGE_ENEMY:
		{
			break;
		}

	case TASK_ZOMBIE_EXPRESS_ANGER:
		{
			if ( IsActivityFinished() )
			{
				TaskComplete();
			}
			break;
		}

	default:
		BaseClass::RunTask( pTask );
		break;
	}
}


//---------------------------------------------------------
// Zombies should scream continuously while burning, so long
// as they are alive.
//---------------------------------------------------------
void CNPC_BurnZombie::Ignite( float flFlameLifetime, bool bNPCOnly, float flSize, bool bCalledByLevelDesigner )
{
	if( !IsOnFire() && IsAlive() )
	{
		EmitSound( "NPC_BurnZombie.Scream" );
		BaseClass::Ignite( flFlameLifetime, bNPCOnly, flSize, bCalledByLevelDesigner );

		RemoveSpawnFlags( SF_NPC_GAG );
		
/*		MoanSound( envZombieMoanIgnited, ARRAYSIZE( envZombieMoanIgnited ) );

		if( m_pMoanSound )
		{
			ENVELOPE_CONTROLLER.SoundChangePitch( m_pMoanSound, 120, 1.0 );
			ENVELOPE_CONTROLLER.SoundChangeVolume( m_pMoanSound, 1, 1.0 );
		}*/
	}
}

//---------------------------------------------------------
// If a zombie stops burning and hasn't died, quiet him down
//---------------------------------------------------------
void CNPC_BurnZombie::Extinguish()
{
/*	if( m_pMoanSound )
	{
		ENVELOPE_CONTROLLER.SoundChangeVolume( m_pMoanSound, 0, 2.0 );
		ENVELOPE_CONTROLLER.SoundChangePitch( m_pMoanSound, 100, 2.0 );
		m_flNextMoanSound = gpGlobals->curtime + random->RandomFloat( 2.0, 4.0 );
	}
*/
	BaseClass::Extinguish();
}

//---------------------------------------------------------
//---------------------------------------------------------
int CNPC_BurnZombie::OnTakeDamage_Alive( const CTakeDamageInfo &inputInfo )
{
	CTakeDamageInfo info = inputInfo;

	// --------------------------------------------
	//	Don't take as much damage from fire
	// --------------------------------------------
	if (info.GetDamageType() == DMG_BURN)
	{
		info.ScaleDamage( 0.05 );
	}

	//LAWYER:  We need to check for low health, for an ignite feature

	if (((this->GetHealth() - inputInfo.GetDamage()) < (this->GetMaxHealth() / 10)))
	{
		if (!this->IsOnFire())
		{
			this->Ignite(100.0f); //Ignite me!
		}
		//LAWYER:  Chance of assplosion!
		if (random->RandomInt(1,10) > 7 && this->GetWaterLevel() < 2) //1 in five, not when watered. //TGB: made 3 in 10 for small powerup
		{
			Detonate();
		}
		
	}
	//LAWYER: this can't go in event_killed, because it'll come out with null pointers
	if (((this->GetHealth() - inputInfo.GetDamage()) <= 0))
	{
		StartFires(); //Start some localised fires
	}
	return BaseClass::OnTakeDamage_Alive( inputInfo );
}

//---------------------------------------------------------
//---------------------------------------------------------
#define ZOMBIE_SQUASH_MASS	300.0f  // Anything this heavy or heavier squashes a zombie good. (show special fx)

//---------------------------------------------------------
//---------------------------------------------------------
void CNPC_BurnZombie::BuildScheduleTestBits( void )
{
	BaseClass::BuildScheduleTestBits();

	if( !m_fIsTorso && !IsCurSchedule( SCHED_FLINCH_PHYSICS ) )
	{
		SetCustomInterruptCondition( COND_PHYSICS_DAMAGE );
	}
}
//LAWYER:  Explosion function, because it's awesome.
void CNPC_BurnZombie::Detonate( void)
{
	ExplosionCreate( this->WorldSpaceCenter(), this->GetAbsAngles(), this, 50, 50, 
	SF_ENVEXPLOSION_NOSPARKS | SF_ENVEXPLOSION_NODLIGHTS | SF_ENVEXPLOSION_SURFACEONLY | SF_ENVEXPLOSION_NOSOUND,
	0.0f, this );
	EmitSound("PropaneTank.Burst");


}

void CNPC_BurnZombie::StartFires(void)
{
	QAngle vecTraceAngles;
	Vector vecTraceDir;
	trace_t firetrace;

	for(int i = 0 ; i < 5 ; i++ )
	{
		// build a little ray
		vecTraceAngles[PITCH]	= random->RandomFloat(45, 135);
		vecTraceAngles[YAW]		= random->RandomFloat(0, 360);
		vecTraceAngles[ROLL]	= 0.0f;

		AngleVectors( vecTraceAngles, &vecTraceDir );

		Vector vecStart, vecEnd;

		vecStart = WorldSpaceCenter();
		//TGB: 	0000392, trace too long causing flames to appear on ground even when zombie is standing high up
		vecEnd = vecStart + vecTraceDir * 128; //TGB: was: 256

		UTIL_TraceLine( vecStart, vecEnd, MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &firetrace );

		Vector	ofsDir = ( firetrace.endpos - GetAbsOrigin() );
		float	offset = VectorNormalize( ofsDir );

		if ( offset > 128 )
			offset = 128;

		//Get our scale based on distance
		//float scale	 = 0.4f + ( 0.75f * ( 1.0f - ( offset / 128.0f ) ) );
		float growth = 0.8f + ( 0.75f * ( offset / 128.0f ) );
		float scale = 125.0f;

		if( firetrace.fraction != 1.0 )
		{
			FireSystem_StartFire( firetrace.endpos, scale, growth, 20.0f, (SF_FIRE_START_ON|SF_FIRE_SMOKELESS|SF_FIRE_NO_GLOW), (CBaseEntity*) this, FIRE_NATURAL );
		}
	}
}
//=============================================================================

//TGB: from what I can tell from the console errors, the game already knows about all these schedules through npc_zombie, somehow
AI_BEGIN_CUSTOM_NPC( npc_burnzombie, CNPC_BurnZombie )
/*
	//DECLARE_CONDITION( COND_BLOCKED_BY_DOOR )
	//DECLARE_CONDITION( COND_DOOR_OPENED )
	DECLARE_CONDITION( COND_ZOMBIE_CHARGE_TARGET_MOVED )

	DECLARE_TASK( TASK_ZOMBIE_EXPRESS_ANGER )
	//DECLARE_TASK( TASK_ZOMBIE_YAW_TO_DOOR )
	//DECLARE_TASK( TASK_ZOMBIE_ATTACK_DOOR )
	DECLARE_TASK( TASK_ZOMBIE_CHARGE_ENEMY )
	
	DECLARE_ACTIVITY( ACT_BURNZOMBIE_TANTRUM );
	DECLARE_ACTIVITY( ACT_BURNZOMBIE_WALLPOUND );
	
	DEFINE_SCHEDULE
	( 
		SCHED_ZOMBIE_BASH_DOOR,

		"	Tasks"
		"		TASK_SET_ACTIVITY				ACTIVITY:ACT_BURNZOMBIE_TANTRUM"
		"		TASK_SET_FAIL_SCHEDULE			SCHEDULE:SCHED_TAKE_COVER_FROM_ENEMY"
		"		TASK_ZOMBIE_YAW_TO_DOOR			0"
		"		TASK_FACE_IDEAL					0"
		"		TASK_ZOMBIE_ATTACK_DOOR			0"
		""
		"	Interrupts"
		//"		COND_ZOMBIE_RELEASECRAB"
		"		COND_ENEMY_DEAD"
		"		COND_NEW_ENEMY"
		"		COND_DOOR_OPENED"
	)

	DEFINE_SCHEDULE
	(
		SCHED_ZOMBIE_WANDER_ANGRILY,

		"	Tasks"
		"		TASK_WANDER						480240" // 48 units to 240 units.
		"		TASK_WALK_PATH					0"
		"		TASK_WAIT_FOR_MOVEMENT			4"
		""
		"	Interrupts"
		//"		COND_ZOMBIE_RELEASECRAB"
		"		COND_ENEMY_DEAD"
		"		COND_NEW_ENEMY"
		//"		COND_DOOR_OPENED"
	)

	DEFINE_SCHEDULE
	(
		SCHED_ZOMBIE_CHARGE_ENEMY,


		"	Tasks"
		"		TASK_ZOMBIE_CHARGE_ENEMY		0"
		"		TASK_WALK_PATH					0"
		"		TASK_WAIT_FOR_MOVEMENT			0"
		"		TASK_PLAY_SEQUENCE				ACTIVITY:ACT_MELEE_ATTACK1"
		""
		"	Interrupts"
		//"		COND_ZOMBIE_RELEASECRAB"
		"		COND_ENEMY_DEAD"
		"		COND_NEW_ENEMY"
		//"		COND_DOOR_OPENED"
		"		COND_ZOMBIE_CHARGE_TARGET_MOVED"
	)

	DEFINE_SCHEDULE
	(
		SCHED_ZOMBIE_FAIL,

		"	Tasks"
		"		TASK_STOP_MOVING		0"
		"		TASK_SET_ACTIVITY		ACTIVITY:ACT_BURNZOMBIE_TANTRUM"
		"		TASK_WAIT				1"
		"		TASK_WAIT_PVS			0"
		""
		"	Interrupts"
		"		COND_CAN_RANGE_ATTACK1 "
		"		COND_CAN_RANGE_ATTACK2 "
		"		COND_CAN_MELEE_ATTACK1 "
		"		COND_CAN_MELEE_ATTACK2"
		"		COND_GIVE_WAY"
		//"		COND_DOOR_OPENED"
	)
*/
AI_END_CUSTOM_NPC()

//=============================================================================
