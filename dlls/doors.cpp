/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
/*

===== doors.cpp ========================================================

*/

#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "doors.h"
#include "movewith.h"
#include "weapons.h"

extern void SetMovedir(entvars_t* ev);

#define noiseMoving noise1
#define noiseArrived noise2

class CBaseDoor : public CBaseToggle
{
public:
	void Spawn( void );
	void Precache( void );
	virtual void PostSpawn( void );
	virtual void KeyValue( KeyValueData *pkvd );
	float InputByMonster(CBaseMonster* pMonster);
	NODE_LINKENT HandleLinkEnt(int afCapMask, bool nodeQueryStatic);
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual void Blocked( CBaseEntity *pOther );


	virtual int	ObjectCaps( void ) 
	{ 
		if (pev->spawnflags & SF_ITEM_USE_ONLY)
		{
			return (CBaseToggle::ObjectCaps() & ~FCAP_ACROSS_TRANSITION) | FCAP_IMPULSE_USE |
				(m_iDirectUse ? FCAP_ONLYDIRECT_USE : 0);
		}
		else
			return (CBaseToggle::ObjectCaps() & ~FCAP_ACROSS_TRANSITION);
	};
	virtual int	Save( CSave &save );
	virtual int	Restore( CRestore &restore );

	static	TYPEDESCRIPTION m_SaveData[];

	virtual void SetToggleState( int state );

	// used to selectivly override defaults
	void EXPORT DoorTouch( CBaseEntity *pOther );

	// local functions
	int DoorActivate( );
	void EXPORT DoorGoUp( void );
	void EXPORT DoorGoDown( void );
	void EXPORT DoorHitTop( void );
	void EXPORT DoorHitBottom( void );
	
	BYTE	m_bHealthValue;// some doors are medi-kit doors, they give players health
	
	BYTE	m_bMoveSnd;			// sound a door makes while moving
	BYTE	m_bStopSnd;			// sound a door makes when it stops

	locksound_t m_ls;			// door lock sounds
	
	BYTE	m_bLockedSound;		// ordinals from entity selection
	BYTE	m_bLockedSentence;	
	BYTE	m_bUnlockedSound;	
	BYTE	m_bUnlockedSentence;

	BOOL	m_iOnOffMode;
	BOOL	m_iImmediateMode;

	BOOL	m_iDirectUse;
};


TYPEDESCRIPTION	CBaseDoor::m_SaveData[] = 
{
	DEFINE_FIELD( CBaseDoor, m_bHealthValue, FIELD_CHARACTER ),
	DEFINE_FIELD( CBaseDoor, m_bMoveSnd, FIELD_CHARACTER ),
	DEFINE_FIELD( CBaseDoor, m_bStopSnd, FIELD_CHARACTER ),
	
	DEFINE_FIELD( CBaseDoor, m_bLockedSound, FIELD_CHARACTER ),
	DEFINE_FIELD( CBaseDoor, m_bLockedSentence, FIELD_CHARACTER ),
	DEFINE_FIELD( CBaseDoor, m_bUnlockedSound, FIELD_CHARACTER ),	
	DEFINE_FIELD( CBaseDoor, m_bUnlockedSentence, FIELD_CHARACTER ),	

	DEFINE_FIELD( CBaseDoor, m_iOnOffMode, FIELD_BOOLEAN ),
	DEFINE_FIELD( CBaseDoor, m_iImmediateMode, FIELD_BOOLEAN ),

	DEFINE_FIELD( CBaseDoor, m_iDirectUse, FIELD_BOOLEAN ),
};

IMPLEMENT_SAVERESTORE( CBaseDoor, CBaseToggle );


#define DOOR_SENTENCEWAIT	6
#define DOOR_SOUNDWAIT		3
#define BUTTON_SOUNDWAIT	0.5

// play door or button locked or unlocked sounds. 
// pass in pointer to valid locksound struct. 
// if flocked is true, play 'door is locked' sound,
// otherwise play 'door is unlocked' sound
// NOTE: this routine is shared by doors and buttons

void PlayLockSounds(entvars_t *pev, locksound_t *pls, int flocked, int fbutton)
{
	// LOCKED SOUND
	
	// CONSIDER: consolidate the locksound_t struct (all entries are duplicates for lock/unlock)
	// CONSIDER: and condense this code.
	float flsoundwait;

	if (fbutton)
		flsoundwait = BUTTON_SOUNDWAIT;
	else
		flsoundwait = DOOR_SOUNDWAIT;

	if (flocked)
	{
		int fplaysound = (pls->sLockedSound && gpGlobals->time > pls->flwaitSound);
		int fplaysentence = (pls->sLockedSentence && !pls->bEOFLocked && gpGlobals->time > pls->flwaitSentence);
		float fvol;

		if (fplaysound && fplaysentence)
			fvol = 0.25;
		else
			fvol = 1.0;

		// if there is a locked sound, and we've debounced, play sound
		if (fplaysound)
		{
			// play 'door locked' sound
			EMIT_SOUND(ENT(pev), CHAN_ITEM, (char*)STRING(pls->sLockedSound), fvol, ATTN_NORM);
			pls->flwaitSound = gpGlobals->time + flsoundwait;
		}

		// if there is a sentence, we've not played all in list, and we've debounced, play sound
		if (fplaysentence)
		{
			// play next 'door locked' sentence in group
			int iprev = pls->iLockedSentence;
			
			pls->iLockedSentence = SENTENCEG_PlaySequentialSz(ENT(pev), STRING(pls->sLockedSentence), 
					  0.85, ATTN_NORM, 0, 100, pls->iLockedSentence, FALSE);
			pls->iUnlockedSentence = 0;

			// make sure we don't keep calling last sentence in list
			pls->bEOFLocked = (iprev == pls->iLockedSentence);
		
			pls->flwaitSentence = gpGlobals->time + DOOR_SENTENCEWAIT;
		}
	}
	else
	{
		// UNLOCKED SOUND

		int fplaysound = (pls->sUnlockedSound && gpGlobals->time > pls->flwaitSound);
		int fplaysentence = (pls->sUnlockedSentence && !pls->bEOFUnlocked && gpGlobals->time > pls->flwaitSentence);
		float fvol;

		// if playing both sentence and sound, lower sound volume so we hear sentence
		if (fplaysound && fplaysentence)
			fvol = 0.25;
		else
			fvol = 1.0;

		// play 'door unlocked' sound if set
		if (fplaysound)
		{
			EMIT_SOUND(ENT(pev), CHAN_ITEM, (char*)STRING(pls->sUnlockedSound), fvol, ATTN_NORM);
			pls->flwaitSound = gpGlobals->time + flsoundwait;
		}

		// play next 'door unlocked' sentence in group
		if (fplaysentence)
		{
			int iprev = pls->iUnlockedSentence;
			
			pls->iUnlockedSentence = SENTENCEG_PlaySequentialSz(ENT(pev), STRING(pls->sUnlockedSentence), 
					  0.85, ATTN_NORM, 0, 100, pls->iUnlockedSentence, FALSE);
			pls->iLockedSentence = 0;

			// make sure we don't keep calling last sentence in list
			pls->bEOFUnlocked = (iprev == pls->iUnlockedSentence);
			pls->flwaitSentence = gpGlobals->time + DOOR_SENTENCEWAIT;
		}
	}
}

//
// Cache user-entity-field values until spawn is called.
//

void CBaseDoor::KeyValue( KeyValueData *pkvd )
{

	if (FStrEq(pkvd->szKeyName, "skin"))//skin is used for content type
	{
		pev->skin = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "movesnd"))
	{
		m_bMoveSnd = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "stopsnd"))
	{
		m_bStopSnd = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "healthvalue"))
	{
		m_bHealthValue = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "locked_sound"))
	{
		m_bLockedSound = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "locked_sentence"))
	{
		m_bLockedSentence = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "unlocked_sound"))
	{
		m_bUnlockedSound = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "unlocked_sentence"))
	{
		m_bUnlockedSentence = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "immediatemode"))
	{
		m_iImmediateMode = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "onoffmode"))
	{
		m_iOnOffMode = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "directuse"))
	{
		m_iDirectUse = atoi(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "WaveHeight"))
	{
		pev->scale = atof(pkvd->szValue) * (1.0/8.0);
		pkvd->fHandled = TRUE;
	}
	else
		CBaseToggle::KeyValue( pkvd );
}

/*QUAKED func_door (0 .5 .8) ? START_OPEN x DOOR_DONT_LINK TOGGLE
if two doors touch, they are assumed to be connected and operate as a unit.

TOGGLE causes the door to wait in both the start and end states for a trigger event.

START_OPEN causes the door to move to its destination when spawned, and operate in reverse.
It is used to temporarily or permanently close off an area when triggered (not usefull for
touch or takedamage doors).

"angle"         determines the opening direction
"targetname"	if set, no touch field will be spawned and a remote button or trigger
				field activates the door.
"health"        if set, door must be shot open
"speed"         movement speed (100 default)
"wait"          wait before returning (3 default, -1 = never return)
"lip"           lip remaining at end of move (8 default)
"dmg"           damage to inflict when blocked (2 default)
"sounds"
0)      no sound
1)      stone
2)      base
3)      stone chain
4)      screechy metal
*/

LINK_ENTITY_TO_CLASS( func_door, CBaseDoor );
//
// func_water - same as a door. 
//
LINK_ENTITY_TO_CLASS( func_water, CBaseDoor );

//MH	this new spawn function messes up SF_DOOR_START_OPEN
//		so replace it with the old one (below)
/*
void CBaseDoor::Spawn( )
{
	Precache();
	SetMovedir (pev);

	if ( pev->skin == 0 )
	{//normal door
		if ( FBitSet (pev->spawnflags, SF_DOOR_PASSABLE) )
			pev->solid		= SOLID_NOT;
		else
			pev->solid		= SOLID_BSP;
	}
	else
	{// special contents
		pev->solid		= SOLID_NOT;
		SetBits( pev->spawnflags, SF_DOOR_SILENT );	// water is silent for now
	}

	pev->movetype	= MOVETYPE_PUSH;
	SET_MODEL( ENT(pev), STRING(pev->model) );
	UTIL_SetOrigin(this, pev->origin);
	
	if (pev->speed == 0)
		pev->speed = 100;
	
	m_vecPosition1	= pev->origin;
	// Subtract 2 from size because the engine expands bboxes by 1 in all directions making the size too big
	m_vecPosition2	= m_vecPosition1 + (pev->movedir * (fabs( pev->movedir.x * (pev->size.x-2) ) + fabs( pev->movedir.y * (pev->size.y-2) ) + fabs( pev->movedir.z * (pev->size.z-2) ) - m_flLip));
	ASSERTSZ(m_vecPosition1 != m_vecPosition2, "door start/end positions are equal");
	if ( FBitSet (pev->spawnflags, SF_DOOR_START_OPEN) )
	{	// swap pos1 and pos2, put door at pos2
		UTIL_SetOrigin(this, m_vecPosition2);
		m_vecPosition2 = m_vecPosition1;
		m_vecPosition1 = pev->origin;
	}

	m_toggle_state = TS_AT_BOTTOM;
	
	// if the door is flagged for USE button activation only, use NULL touch function
	// (unless it's overridden, of course- LRC)
	if ( FBitSet ( pev->spawnflags, SF_DOOR_USE_ONLY ) &&
			!FBitSet ( pev->spawnflags, SF_DOOR_FORCETOUCHABLE ))
	{
		SetTouch ( NULL );
	}
	else // touchable button
		SetTouch(&CBaseDoor:: DoorTouch );
}
*/

//standard Spirit 1.0 spawn function
void CBaseDoor::Spawn( )
{
	Precache();
	SetMovedir (pev);

	if ( pev->skin == 0 )
	{//normal door
		if ( FBitSet (pev->spawnflags, SF_DOOR_PASSABLE) )
			pev->solid = SOLID_NOT;
		else
			pev->solid = SOLID_BSP;
	}
	else
	{// special contents
		pev->solid = SOLID_NOT;
		SetBits( pev->spawnflags, SF_DOOR_SILENT );	// water is silent for now
	}

	pev->movetype	= MOVETYPE_PUSH;
	SET_MODEL( ENT(pev), STRING(pev->model) );
	UTIL_SetOrigin(this, pev->origin);
	
	if (pev->speed == 0)
		pev->speed = 100;

	m_toggle_state = TS_AT_BOTTOM;

	// if the door is flagged for USE button activation only, use NULL touch function
	// (unless it's overridden, of course- LRC)
	if ( FBitSet ( pev->spawnflags, SF_DOOR_USE_ONLY ) &&
			!FBitSet ( pev->spawnflags, SF_DOOR_FORCETOUCHABLE ))
	{
		SetTouch ( NULL );
	}
	else // touchable button
		SetTouch( &CBaseDoor::DoorTouch );
}
//END
 
//LRC
void CBaseDoor :: PostSpawn( void )
{
	if (m_pMoveWith)
		m_vecPosition1 = pev->origin - m_pMoveWith->pev->origin;
	else
		m_vecPosition1 = pev->origin;

	// Subtract 2 from size because the engine expands bboxes by 1 in all directions
	m_vecPosition2	= m_vecPosition1 + (pev->movedir * (fabs( pev->movedir.x * (pev->size.x-2) ) + fabs( pev->movedir.y * (pev->size.y-2) ) + fabs( pev->movedir.z * (pev->size.z-2) ) - m_flLip));

	ASSERTSZ(m_vecPosition1 != m_vecPosition2, "door start/end positions are equal");
	if ( FBitSet (pev->spawnflags, SF_DOOR_START_OPEN) )
	{	// swap pos1 and pos2, put door at pos2
		if (m_pMoveWith)
		{
			m_vecSpawnOffset = m_vecSpawnOffset + (m_vecPosition2 + m_pMoveWith->pev->origin) - pev->origin;
			UTIL_AssignOrigin(this, m_vecPosition2 + m_pMoveWith->pev->origin);
		}
		else
		{
			m_vecSpawnOffset = m_vecSpawnOffset + m_vecPosition2 - pev->origin;
			UTIL_AssignOrigin(this, m_vecPosition2);
		}
		Vector vecTemp = m_vecPosition2;
		m_vecPosition2 = m_vecPosition1;
		m_vecPosition1 = vecTemp;
//		ALERT(at_console, "func_door postspawn: origin %f %f %f\n", pev->origin.x, pev->origin.y, pev->origin.z);
	}
}

//void CBaseDoor :: PostMoveWith( void )
//{
//	Vector vecTemp = m_vecPosition1 - m_pMoveWith->m_vecSpawnOffset;
//	ALERT(at_console, "door %s pmw: pos1 changes from (%f %f %f) to (%f %f %f)\n", STRING(pev->targetname), m_vecPosition1.x, m_vecPosition1.y, m_vecPosition1.z, vecTemp.x, vecTemp.y, vecTemp.z);
//	m_vecPosition1 = m_vecPosition1 - m_pMoveWith->m_vecSpawnOffset;
//	m_vecPosition2 = m_vecPosition2 - m_pMoveWith->m_vecSpawnOffset;
//}

void CBaseDoor :: SetToggleState( int state )
{
	if ( state == TS_AT_TOP )
    {
		if (m_pMoveWith)
			UTIL_AssignOrigin( this, m_vecPosition2 + m_pMoveWith->pev->origin);
		else
			UTIL_AssignOrigin( this, m_vecPosition2 );
    }
    else
    {
		if (m_pMoveWith)
			UTIL_AssignOrigin( this, m_vecPosition1 + m_pMoveWith->pev->origin);
		else
			UTIL_AssignOrigin( this, m_vecPosition1 );
    }
}


void CBaseDoor::Precache( void )
{
	char *pszSound;

// set the door's "in-motion" sound
	switch (m_bMoveSnd)
	{
	case	0:
		pev->noiseMoving = MAKE_STRING("common/null.wav");
		break;
	case	1:
		PRECACHE_SOUND ("doors/doormove1.wav");
		pev->noiseMoving = MAKE_STRING("doors/doormove1.wav");
		break;
	case	2:
		PRECACHE_SOUND ("doors/doormove2.wav");
		pev->noiseMoving = MAKE_STRING("doors/doormove2.wav");
		break;
	case	3:
		PRECACHE_SOUND ("doors/doormove3.wav");
		pev->noiseMoving = MAKE_STRING("doors/doormove3.wav");
		break;
	case	4:
		PRECACHE_SOUND ("doors/doormove4.wav");
		pev->noiseMoving = MAKE_STRING("doors/doormove4.wav");
		break;
	case	5:
		PRECACHE_SOUND ("doors/doormove5.wav");
		pev->noiseMoving = MAKE_STRING("doors/doormove5.wav");
		break;
	case	6:
		PRECACHE_SOUND ("doors/doormove6.wav");
		pev->noiseMoving = MAKE_STRING("doors/doormove6.wav");
		break;
	case	7:
		PRECACHE_SOUND ("doors/doormove7.wav");
		pev->noiseMoving = MAKE_STRING("doors/doormove7.wav");
		break;
	case	8:
		PRECACHE_SOUND ("doors/doormove8.wav");
		pev->noiseMoving = MAKE_STRING("doors/doormove8.wav");
		break;
	case	9:
		PRECACHE_SOUND ("doors/doormove9.wav");
		pev->noiseMoving = MAKE_STRING("doors/doormove9.wav");
		break;
	case	10:
		PRECACHE_SOUND ("doors/doormove10.wav");
		pev->noiseMoving = MAKE_STRING("doors/doormove10.wav");
		break;
	default:
		pev->noiseMoving = MAKE_STRING("common/null.wav");
		break;
	}

// set the door's 'reached destination' stop sound
	switch (m_bStopSnd)
	{
	case	0:
		pev->noiseArrived = MAKE_STRING("common/null.wav");
		break;
	case	1:
		PRECACHE_SOUND ("doors/doorstop1.wav");
		pev->noiseArrived = MAKE_STRING("doors/doorstop1.wav");
		break;
	case	2:
		PRECACHE_SOUND ("doors/doorstop2.wav");
		pev->noiseArrived = MAKE_STRING("doors/doorstop2.wav");
		break;
	case	3:
		PRECACHE_SOUND ("doors/doorstop3.wav");
		pev->noiseArrived = MAKE_STRING("doors/doorstop3.wav");
		break;
	case	4:
		PRECACHE_SOUND ("doors/doorstop4.wav");
		pev->noiseArrived = MAKE_STRING("doors/doorstop4.wav");
		break;
	case	5:
		PRECACHE_SOUND ("doors/doorstop5.wav");
		pev->noiseArrived = MAKE_STRING("doors/doorstop5.wav");
		break;
	case	6:
		PRECACHE_SOUND ("doors/doorstop6.wav");
		pev->noiseArrived = MAKE_STRING("doors/doorstop6.wav");
		break;
	case	7:
		PRECACHE_SOUND ("doors/doorstop7.wav");
		pev->noiseArrived = MAKE_STRING("doors/doorstop7.wav");
		break;
	case	8:
		PRECACHE_SOUND ("doors/doorstop8.wav");
		pev->noiseArrived = MAKE_STRING("doors/doorstop8.wav");
		break;
	default:
		pev->noiseArrived = MAKE_STRING("common/null.wav");
		break;
	}

	// get door button sounds, for doors which are directly 'touched' to open

	if (m_bLockedSound)
	{
		pszSound = ButtonSound( (int)m_bLockedSound );
		PRECACHE_SOUND(pszSound);
		m_ls.sLockedSound = ALLOC_STRING(pszSound);
	}

	if (m_bUnlockedSound)
	{
		pszSound = ButtonSound( (int)m_bUnlockedSound );
		PRECACHE_SOUND(pszSound);
		m_ls.sUnlockedSound = ALLOC_STRING(pszSound);
	}

	// get sentence group names, for doors which are directly 'touched' to open

	switch (m_bLockedSentence)
	{
		case 1: m_ls.sLockedSentence = MAKE_STRING("NA"); break; // access denied
		case 2: m_ls.sLockedSentence = MAKE_STRING("ND"); break; // security lockout
		case 3: m_ls.sLockedSentence = MAKE_STRING("NF"); break; // blast door
		case 4: m_ls.sLockedSentence = MAKE_STRING("NFIRE"); break; // fire door
		case 5: m_ls.sLockedSentence = MAKE_STRING("NCHEM"); break; // chemical door
		case 6: m_ls.sLockedSentence = MAKE_STRING("NRAD"); break; // radiation door
		case 7: m_ls.sLockedSentence = MAKE_STRING("NCON"); break; // gen containment
		case 8: m_ls.sLockedSentence = MAKE_STRING("NH"); break; // maintenance door
		case 9: m_ls.sLockedSentence = MAKE_STRING("NG"); break; // broken door
		
		default: m_ls.sLockedSentence = 0; break;
	}

	switch (m_bUnlockedSentence)
	{
		case 1: m_ls.sUnlockedSentence = MAKE_STRING("EA"); break; // access granted
		case 2: m_ls.sUnlockedSentence = MAKE_STRING("ED"); break; // security door
		case 3: m_ls.sUnlockedSentence = MAKE_STRING("EF"); break; // blast door
		case 4: m_ls.sUnlockedSentence = MAKE_STRING("EFIRE"); break; // fire door
		case 5: m_ls.sUnlockedSentence = MAKE_STRING("ECHEM"); break; // chemical door
		case 6: m_ls.sUnlockedSentence = MAKE_STRING("ERAD"); break; // radiation door
		case 7: m_ls.sUnlockedSentence = MAKE_STRING("ECON"); break; // gen containment
		case 8: m_ls.sUnlockedSentence = MAKE_STRING("EH"); break; // maintenance door
		
		default: m_ls.sUnlockedSentence = 0; break;
	}
}

//
// Doors not tied to anything (e.g. button, another door) can be touched, to make them activate.
//
void CBaseDoor::DoorTouch( CBaseEntity *pOther )
{
	entvars_t*	pevToucher = pOther->pev;
	
	// Ignore touches by anything but players
	if (!FClassnameIs(pevToucher, "player"))
		return;

	// If door has master, and it's not ready to trigger, 
	// play 'locked' sound

	if (m_sMaster && !UTIL_IsMasterTriggered(m_sMaster, pOther))
		PlayLockSounds(pev, &m_ls, TRUE, FALSE);
	
	// If door is somebody's target, then touching does nothing.
	// You have to activate the owner (e.g. button).
	//LRC- allow flags to override this
	if (!FStringNull(pev->targetname) && !FBitSet(pev->spawnflags,SF_DOOR_FORCETOUCHABLE))
	{
		// play locked sound
		PlayLockSounds(pev, &m_ls, TRUE, FALSE);
		return; 
	}
	
	m_hActivator = pOther;// remember who activated the door

	if (DoorActivate( ))
		SetTouch( NULL ); // Temporarily disable the touch function, until movement is finished.
}


//
// Used by SUB_UseTargets, when a door is the target of a button.
//
void CBaseDoor::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	m_hActivator = pActivator;

	if (!UTIL_IsMasterTriggered(m_sMaster, pActivator))
		return;
		
	//LRC:
	if (m_iOnOffMode)
	{
		if (useType == USE_ON)
		{
			if (m_toggle_state == TS_AT_BOTTOM)
			{
				PlayLockSounds(pev, &m_ls, FALSE, FALSE);
				DoorGoUp();
			}
			return;
		}
		else if (useType == USE_OFF)
		{
			if (m_toggle_state == TS_AT_TOP)
			{
				DoorGoDown();
			}
			return;
		}
	}

	// if not ready to be used, ignore "use" command.
	if (m_toggle_state == TS_AT_TOP)
	{
		if (!FBitSet(pev->spawnflags, SF_DOOR_NO_AUTO_RETURN))
			return;
	}
	else if (m_toggle_state != TS_AT_BOTTOM)
		return;

		DoorActivate();
}

float CBaseDoor::InputByMonster(CBaseMonster *pMonster)
{
	if (FBitSet(pev->spawnflags, SF_DOOR_NOMONSTERS))
		return 0.0f;

	BOOL originalMode = m_iOnOffMode;
	m_iOnOffMode = TRUE;
	Use(pMonster, pMonster, USE_ON, 0.0f);
	m_iOnOffMode = originalMode;
	return pev->nextthink - pev->ltime;
}

NODE_LINKENT CBaseDoor::HandleLinkEnt(int afCapMask, bool nodeQueryStatic)
{
	if (nodeQueryStatic) {
		return NLE_ALLOW;
	}

	const int toggleState = GetToggleState();

	// monster should try for it if the door is open and looks as if it will stay that way
	if( toggleState == TS_AT_TOP && (( pev->spawnflags & SF_DOOR_NO_AUTO_RETURN ) || (m_flWait == -1.0f)) )
	{
		return NLE_ALLOW;
	}

	if (!UTIL_IsMasterTriggered( m_sMaster, this )) {
		return NLE_PROHIBIT;
	}

	if( ( pev->spawnflags & SF_DOOR_USE_ONLY ) )
	{
		// door is use only.
		if( ( afCapMask & bits_CAP_OPEN_DOORS ) )
		{
			// let monster right through if he can open doors
			if (!( pev->spawnflags & SF_DOOR_NOMONSTERS ))
				return NLE_NEEDS_INPUT;
		}
		return NLE_PROHIBIT;
	}
	else
	{
		// door must be opened with a button or trigger field.
		if( ( afCapMask & bits_CAP_OPEN_DOORS ) )
		{
			if (!FStringNull(pev->targetname) && !FBitSet(pev->spawnflags, SF_DOOR_FORCETOUCHABLE))
			{
				return NLE_PROHIBIT;
			}
			if( !( pev->spawnflags & SF_DOOR_NOMONSTERS ) ) {
				return NLE_NEEDS_INPUT;
			}
		}

		return NLE_PROHIBIT;
	}
}

//
// Causes the door to "do its thing", i.e. start moving, and cascade activation.
//
int CBaseDoor::DoorActivate( )
{
	if (!UTIL_IsMasterTriggered(m_sMaster, m_hActivator))
		return 0;

	if (FBitSet(pev->spawnflags, SF_DOOR_NO_AUTO_RETURN) && m_toggle_state == TS_AT_TOP)
	{// door should close
		DoorGoDown();
	}
	else
	{// door should open

		if ( m_hActivator != NULL && m_hActivator->IsPlayer() )
		{// give health if player opened the door (medikit)
		// VARS( m_eoActivator )->health += m_bHealthValue;
	
			m_hActivator->TakeHealth( m_bHealthValue, DMG_GENERIC );

		}

		// play door unlock sounds
		PlayLockSounds(pev, &m_ls, FALSE, FALSE);
		
		DoorGoUp();
	}

	return 1;
}

extern Vector VecBModelOrigin( entvars_t* pevBModel );

//
// Starts the door going to its "up" position (simply ToggleData->vecPosition2).
//
void CBaseDoor::DoorGoUp( void )
{
	entvars_t	*pevActivator;

	// It could be going-down, if blocked.
	ASSERT(m_toggle_state == TS_AT_BOTTOM || m_toggle_state == TS_GOING_DOWN);

	// emit door moving and stop sounds on CHAN_STATIC so that the multicast doesn't
	// filter them out and leave a client stuck with looping door sounds!
	if ( !FBitSet( pev->spawnflags, SF_DOOR_SILENT ) )
		EMIT_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noiseMoving), 1, ATTN_NORM);

//	ALERT(at_debug, "%s go up (was %d)\n", STRING(pev->targetname), m_toggle_state);
	m_toggle_state = TS_GOING_UP;
	
	SetMoveDone(&CBaseDoor:: DoorHitTop );

	// LRC- if synched, we fire as soon as we start to go up
	if (m_iImmediateMode)
	{
		if (m_iOnOffMode)
			SUB_UseTargets( m_hActivator, USE_ON, 0 );
		else
			SUB_UseTargets( m_hActivator, USE_TOGGLE, 0 );
	}

	if ( FClassnameIs(pev, "func_door_rotating"))		// !!! BUGBUG Triggered doors don't work with this yet
	{
		float	sign = 1.0;

		if ( m_hActivator != NULL )
		{
			pevActivator = m_hActivator->pev;
			
			if ( !FBitSet( pev->spawnflags, SF_DOOR_ONEWAY ) && pev->movedir.y ) 		// Y axis rotation, move away from the player
			{
				Vector vec = pevActivator->origin - pev->origin;
				Vector angles = pevActivator->angles;
				angles.x = 0;
				angles.z = 0;
				UTIL_MakeVectors (angles);
	//			Vector vnext = (pevToucher->origin + (pevToucher->velocity * 10)) - pev->origin;
				UTIL_MakeVectors ( pevActivator->angles );
				Vector vnext = (pevActivator->origin + (gpGlobals->v_forward * 10)) - pev->origin;
				if ( (vec.x*vnext.y - vec.y*vnext.x) < 0 )
					sign = -1.0;
			}
		}
		AngularMove(m_vecAngle2*sign, pev->speed);
	}
	else
		LinearMove(m_vecPosition2, pev->speed);
}


//
// The door has reached the "up" position.  Either go back down, or wait for another activation.
//
void CBaseDoor::DoorHitTop( void )
{
	if ( !FBitSet( pev->spawnflags, SF_DOOR_SILENT ) )
	{
		STOP_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noiseMoving) );
		EMIT_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noiseArrived), 1, ATTN_NORM);
	}

//	ALERT(at_debug, "%s hit top\n", STRING(pev->targetname));
	ASSERT(m_toggle_state == TS_GOING_UP);
	m_toggle_state = TS_AT_TOP;
	
	// toggle-doors don't come down automatically, they wait for refire.
	if (FBitSet(pev->spawnflags, SF_DOOR_NO_AUTO_RETURN))
	{
		// Re-instate touch method, movement is complete
		if ( !FBitSet ( pev->spawnflags, SF_DOOR_USE_ONLY ) ||
				FBitSet ( pev->spawnflags, SF_DOOR_FORCETOUCHABLE ) )
			SetTouch(&CBaseDoor:: DoorTouch );
	}
	else
	{
		// In flWait seconds, DoorGoDown will fire, unless wait is -1, then door stays open
		SetNextThink( m_flWait );
		SetThink(&CBaseDoor:: DoorGoDown );

		if ( m_flWait == -1 )
		{
			DontThink();
		}
	}

	// Fire the close target (if startopen is set, then "top" is closed) - netname is the close target
	if (pev->spawnflags & SF_DOOR_START_OPEN)
	{
		if (pev->netname)
			FireTargets( STRING(pev->netname), m_hActivator, this, USE_TOGGLE, 0 );
	}
	else
	{
		if (pev->message)
			FireTargets( STRING(pev->message), m_hActivator, this, USE_TOGGLE, 0 );
	}

	// LRC
	if (!m_iImmediateMode)
	{
		if (m_iOnOffMode)
			SUB_UseTargets( m_hActivator, USE_OFF, 0 );
		else
			SUB_UseTargets( m_hActivator, USE_TOGGLE, 0 );
	}
}


//
// Starts the door going to its "down" position (simply ToggleData->vecPosition1).
//
void CBaseDoor::DoorGoDown( void )
{
	if ( !FBitSet( pev->spawnflags, SF_DOOR_SILENT ) )
		EMIT_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noiseMoving), 1, ATTN_NORM);
	
//	ALERT(at_debug, "%s go down (was %d)\n", STRING(pev->targetname), m_toggle_state);

//FYI: not defined, so this doesn't happen. --LRC
#ifdef DOOR_ASSERT
	ASSERT(m_toggle_state == TS_AT_TOP);
#endif // DOOR_ASSERT
	m_toggle_state = TS_GOING_DOWN;

	SetMoveDone(&CBaseDoor:: DoorHitBottom );
	if ( FClassnameIs(pev, "func_door_rotating"))//rotating door
	{
		// LRC- if synched, we fire as soon as we start to go down
		if (m_iImmediateMode)
		{
			if (m_iOnOffMode)
				SUB_UseTargets( m_hActivator, USE_OFF, 0 );
			else
				SUB_UseTargets( m_hActivator, USE_TOGGLE, 0 );
		}
		AngularMove( m_vecAngle1, pev->speed);
	}
	else
	{
		// LRC- if synched, we fire as soon as we start to go down
		if (m_iImmediateMode)
		{
			SUB_UseTargets( m_hActivator, USE_OFF, 0 );
		}
		LinearMove( m_vecPosition1, pev->speed);
	}
}

//
// The door has reached the "down" position.  Back to quiescence.
//
void CBaseDoor::DoorHitBottom( void )
{
	if ( !FBitSet( pev->spawnflags, SF_DOOR_SILENT ) )
	{
		STOP_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noiseMoving) );
		EMIT_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noiseArrived), 1, ATTN_NORM);
	}

//	ALERT(at_debug, "%s hit bottom\n", STRING(pev->targetname));
	ASSERT(m_toggle_state == TS_GOING_DOWN);
	m_toggle_state = TS_AT_BOTTOM;

	// Re-instate touch method, cycle is complete
	if ( FBitSet ( pev->spawnflags, SF_DOOR_USE_ONLY ) &&
			!FBitSet ( pev->spawnflags, SF_DOOR_FORCETOUCHABLE ) )
	{// use only door
		SetTouch ( NULL );
	}
	else // touchable door
		SetTouch(&CBaseDoor:: DoorTouch );

	// Fire the close target (if startopen is set, then "top" is closed) - netname is the close target
	// LRC- 'message' is the open target
	if (pev->spawnflags & SF_DOOR_START_OPEN)
	{
		if (pev->message)
			FireTargets( STRING(pev->message), m_hActivator, this, USE_TOGGLE, 0 );
	}
	else
	{
		if (pev->netname)
			FireTargets( STRING(pev->netname), m_hActivator, this, USE_TOGGLE, 0 );
	}
//	else
//	{
//		ALERT(at_console,"didn't fire closetarget because ");
//		if (!(pev->netname))
//			ALERT(at_console,"no netname\n");
//		else if (pev->spawnflags & SF_DOOR_START_OPEN)
//			ALERT(at_console,"startopen\n");
//		else
//			ALERT(at_console,"!!?!\n");
//	}

	// LRC- if synched, don't fire now
	if (!m_iImmediateMode)
	{
		if (m_iOnOffMode)
			SUB_UseTargets( m_hActivator, USE_ON, 0 );
		else
			SUB_UseTargets( m_hActivator, USE_TOGGLE, 0 );
	}
}

void CBaseDoor::Blocked( CBaseEntity *pOther )
{
	CBaseEntity	*pTarget	= NULL;
	CBaseDoor	*pDoor		= NULL;

//	ALERT(at_debug, "%s blocked\n", STRING(pev->targetname));

	// Hurt the blocker a little.
	if ( pev->dmg )
		pOther->TakeDamage( pev, pev, pev->dmg, DMG_CRUSH );

	if( true ) 	// ������ ��� ������ ���� ��� �� ��������� ���������� ������ � ��������� ��������, ��������� ���������� � ������� ������.
	{
		// Detonate satchels
		if( !strcmp( "monster_satchel", STRING( pOther->pev->classname ) ) )
			( (CSatchel*)pOther )->Use( this, this, USE_ON, 0 );
	}
	// if a door has a negative wait, it would never come back if blocked,
	// so let it just squash the object to death real fast

	if (m_flWait >= 0)
	{
		//LRC - thanks to [insert name here] for this
		if ( !FBitSet( pev->spawnflags, SF_DOOR_SILENT ) )
			STOP_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noiseMoving) );
		if (m_toggle_state == TS_GOING_DOWN)
		{
			DoorGoUp();
		}
		else
		{
			DoorGoDown();
		}
	}

	// Block all door pieces with the same targetname here.
	//LRC - in immediate mode don't do this, doors are expected to do it themselves.
	if ( !m_iImmediateMode && !FStringNull ( pev->targetname ) )
	{
		for (;;)
		{
			pTarget = UTIL_FindEntityByTargetname(pTarget, STRING(pev->targetname));

			if ( !pTarget )
				break;

			if ( VARS( pTarget->pev ) != pev && FClassnameIs ( pTarget->pev, "func_door" ) ||
						FClassnameIs ( pTarget->pev, "func_door_rotating" ) )
			{
				pDoor = GetClassPtr( (CBaseDoor *) VARS(pTarget->pev) );
				if ( pDoor->m_flWait >= 0)
				{
					// avelocity == velocity!? LRC
					if (pDoor->pev->velocity == pev->velocity && pDoor->pev->avelocity == pev->velocity)
					{
						// this is the most hacked, evil, bastardized thing I've ever seen. kjb
						if ( FClassnameIs ( pTarget->pev, "func_door" ) )
						{// set origin to realign normal doors
							pDoor->pev->origin = pev->origin;
							UTIL_SetVelocity(pDoor, g_vecZero);// stop!
						}
						else
						{// set angles to realign rotating doors
							pDoor->pev->angles = pev->angles;
							UTIL_SetAvelocity(pDoor, g_vecZero);
						}
					}
					if ( pDoor->m_toggle_state == TS_GOING_DOWN)
						pDoor->DoorGoUp();
					else
						pDoor->DoorGoDown();
				}
			}
		}
	}
}


/*QUAKED FuncRotDoorSpawn (0 .5 .8) ? START_OPEN REVERSE  
DOOR_DONT_LINK TOGGLE X_AXIS Y_AXIS
if two doors touch, they are assumed to be connected and operate as  
a unit.

TOGGLE causes the door to wait in both the start and end states for  
a trigger event.

START_OPEN causes the door to move to its destination when spawned,  
and operate in reverse.  It is used to temporarily or permanently  
close off an area when triggered (not usefull for touch or  
takedamage doors).

You need to have an origin brush as part of this entity.  The  
center of that brush will be
the point around which it is rotated. It will rotate around the Z  
axis by default.  You can
check either the X_AXIS or Y_AXIS box to change that.

"distance" is how many degrees the door will be rotated.
"speed" determines how fast the door moves; default value is 100.

REVERSE will cause the door to rotate in the opposite direction.

"angle"		determines the opening direction
"targetname" if set, no touch field will be spawned and a remote  
button or trigger field activates the door.
"health"	if set, door must be shot open
"speed"		movement speed (100 default)
"wait"		wait before returning (3 default, -1 = never return)
"dmg"		damage to inflict when blocked (2 default)
"sounds"
0)	no sound
1)	stone
2)	base
3)	stone chain
4)	screechy metal
*/
class CRotDoor : public CBaseDoor
{
public:
	void Spawn( void );
	void KeyValue( KeyValueData *pkvd );
	virtual void PostSpawn( void ) {} // don't use the moveWith fix from CBaseDoor
	virtual void SetToggleState( int state );
};

LINK_ENTITY_TO_CLASS( func_door_rotating, CRotDoor );


void CRotDoor::Spawn( void )
{
	Precache();
	// set the axis of rotation
	CBaseToggle::AxisDir( pev );

	// check for clockwise rotation
	if ( FBitSet (pev->spawnflags, SF_DOOR_ROTATE_BACKWARDS) )
		pev->movedir = pev->movedir * -1;
	
	//m_flWait			= 2; who the hell did this? (sjb)
	m_vecAngle1	= pev->angles;
	m_vecAngle2	= pev->angles + pev->movedir * m_flMoveDistance;

	ASSERTSZ(m_vecAngle1 != m_vecAngle2, "rotating door start/end positions are equal");
	
	if ( FBitSet (pev->spawnflags, SF_DOOR_PASSABLE) )
		pev->solid		= SOLID_NOT;
	else
		pev->solid		= SOLID_BSP;

	pev->movetype	= MOVETYPE_PUSH;
	UTIL_SetOrigin(this, pev->origin);
	SET_MODEL(ENT(pev), STRING(pev->model) );

	if (pev->speed == 0)
		pev->speed = 100;
	
// DOOR_START_OPEN is to allow an entity to be lighted in the closed position
// but spawn in the open position
	if ( FBitSet (pev->spawnflags, SF_DOOR_START_OPEN) )
	{	// swap pos1 and pos2, put door at pos2, invert movement direction
		pev->angles = m_vecAngle2;
		Vector vecSav = m_vecAngle1;
		m_vecAngle2 = m_vecAngle1;
		m_vecAngle1 = vecSav;
		pev->movedir = pev->movedir * -1;
	}

	m_toggle_state = TS_AT_BOTTOM;

	if ( FBitSet ( pev->spawnflags, SF_DOOR_USE_ONLY ) && !FBitSet(pev->spawnflags, SF_DOOR_FORCETOUCHABLE) )
	{
		SetTouch ( NULL );
	}
	else // touchable button
		SetTouch(&CRotDoor:: DoorTouch );
}

void CRotDoor::KeyValue( KeyValueData *pkvd )
{
	if (FStrEq(pkvd->szKeyName, "axes"))
	{
		UTIL_StringToVector( (float*)(pev->movedir), pkvd->szValue );
		pkvd->fHandled = TRUE;
	}
	else
		CBaseDoor::KeyValue( pkvd );
}

void CRotDoor :: SetToggleState( int state )
{
	if ( state == TS_AT_TOP )
		pev->angles = m_vecAngle2;
	else
		pev->angles = m_vecAngle1;

	UTIL_SetOrigin( this, pev->origin );
}


#define SF_MOMDOOR_MOVESTART = 0x80000000

class CMomentaryDoor : public CBaseToggle
{
public:
	void	Spawn( void );
	void Precache( void );
	void EXPORT MomentaryMoveDone( void );

	void	KeyValue( KeyValueData *pkvd );
	void	Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int	ObjectCaps( void ) { return CBaseToggle :: ObjectCaps() & ~FCAP_ACROSS_TRANSITION; }

	virtual int	Save( CSave &save );
	virtual int	Restore( CRestore &restore );
	static	TYPEDESCRIPTION m_SaveData[];

	BYTE	m_bMoveSnd;			// sound a door makes while moving	
	STATE	m_iState;
	float m_fLastPos;

	STATE	GetState( void ) { return m_iState; }
	float CalcRatio( CBaseEntity *pLocus ) { return m_fLastPos; }
};

LINK_ENTITY_TO_CLASS( momentary_door, CMomentaryDoor );

TYPEDESCRIPTION	CMomentaryDoor::m_SaveData[] = 
{
	DEFINE_FIELD( CMomentaryDoor, m_bMoveSnd, FIELD_CHARACTER ),
	DEFINE_FIELD( CMomentaryDoor, m_iState, FIELD_INTEGER ),
	DEFINE_FIELD( CMomentaryDoor, m_fLastPos, FIELD_FLOAT ),
};

IMPLEMENT_SAVERESTORE( CMomentaryDoor, CBaseToggle );

void CMomentaryDoor::Spawn( void )
{
	SetMovedir (pev);

	pev->solid		= SOLID_BSP;
	pev->movetype	= MOVETYPE_PUSH;

	UTIL_SetOrigin(this, pev->origin);
	SET_MODEL( ENT(pev), STRING(pev->model) );
	
//	if (pev->speed == 0)
//		pev->speed = 100;
	if (pev->dmg == 0)
		pev->dmg = 2;

	m_iState = STATE_OFF;
	
	m_vecPosition1	= pev->origin;
	// Subtract 2 from size because the engine expands bboxes by 1 in all directions making the size too big
	m_vecPosition2	= m_vecPosition1 + (pev->movedir * (fabs( pev->movedir.x * (pev->size.x-2) ) + fabs( pev->movedir.y * (pev->size.y-2) ) + fabs( pev->movedir.z * (pev->size.z-2) ) - m_flLip));
	ASSERTSZ(m_vecPosition1 != m_vecPosition2, "door start/end positions are equal");

	//LRC: FIXME, move to PostSpawn
	if ( FBitSet (pev->spawnflags, SF_DOOR_START_OPEN) )
	{	// swap pos1 and pos2, put door at pos2
		UTIL_AssignOrigin(this, m_vecPosition2);
		Vector vecTemp = m_vecPosition2;
		m_vecPosition2 = m_vecPosition1;
		m_vecPosition1 = vecTemp;
	}

	if (m_pMoveWith)
	{
		m_vecPosition1 = m_vecPosition1 - m_pMoveWith->pev->origin;
		m_vecPosition2 = m_vecPosition2 - m_pMoveWith->pev->origin;
	}

	SetTouch( NULL );
	
	Precache();
}
	
void CMomentaryDoor::Precache( void )
{

// set the door's "in-motion" sound
	switch (m_bMoveSnd)
	{
	case	0:
		pev->noiseMoving = MAKE_STRING("common/null.wav");
		break;
	case	1:
		PRECACHE_SOUND ("doors/doormove1.wav");
		pev->noiseMoving = MAKE_STRING("doors/doormove1.wav");
		break;
	case	2:
		PRECACHE_SOUND ("doors/doormove2.wav");
		pev->noiseMoving = MAKE_STRING("doors/doormove2.wav");
		break;
	case	3:
		PRECACHE_SOUND ("doors/doormove3.wav");
		pev->noiseMoving = MAKE_STRING("doors/doormove3.wav");
		break;
	case	4:
		PRECACHE_SOUND ("doors/doormove4.wav");
		pev->noiseMoving = MAKE_STRING("doors/doormove4.wav");
		break;
	case	5:
		PRECACHE_SOUND ("doors/doormove5.wav");
		pev->noiseMoving = MAKE_STRING("doors/doormove5.wav");
		break;
	case	6:
		PRECACHE_SOUND ("doors/doormove6.wav");
		pev->noiseMoving = MAKE_STRING("doors/doormove6.wav");
		break;
	case	7:
		PRECACHE_SOUND ("doors/doormove7.wav");
		pev->noiseMoving = MAKE_STRING("doors/doormove7.wav");
		break;
	case	8:
		PRECACHE_SOUND ("doors/doormove8.wav");
		pev->noiseMoving = MAKE_STRING("doors/doormove8.wav");
		break;
	default:
		pev->noiseMoving = MAKE_STRING("common/null.wav");
		break;
	}
}

void CMomentaryDoor::KeyValue( KeyValueData *pkvd )
{

	if (FStrEq(pkvd->szKeyName, "movesnd"))
	{
		m_bMoveSnd = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "stopsnd"))
	{
//		m_bStopSnd = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else if (FStrEq(pkvd->szKeyName, "healthvalue"))
	{
//		m_bHealthValue = atof(pkvd->szValue);
		pkvd->fHandled = TRUE;
	}
	else
		CBaseToggle::KeyValue( pkvd );
}

void CMomentaryDoor::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( useType != USE_SET )		// Momentary buttons will pass down a float in here
		return;

	if ( value > 1.0 )
		value = 1.0;

	if (IsLockedByMaster()) return;

	Vector move = m_vecPosition1 + (value * (m_vecPosition2 - m_vecPosition1));
	
	float speed = 0;
	if (pev->speed)
	{
		//LRC- move at the given speed, if any.
		speed = pev->speed;
	}
	else
	{
		// default: get there in 0.1 secs
		Vector delta;
		delta = move - pev->origin;

		speed = delta.Length() * 10;
	}


	//FIXME: allow for it being told to move at the same speed in the _opposite_ direction!
	if ( speed != 0 )
	{
		// This entity only thinks when it moves
		//LRC- nope, in a MoveWith world you can't rely on that. Check the state instead.
		if ( m_iState == STATE_OFF )
		{
			//ALERT(at_console,"USE: start moving to %f %f %f.\n", move.x, move.y, move.z);
			m_iState = STATE_ON;
			EMIT_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noiseMoving), 1, ATTN_NORM);
		}

		m_fLastPos = value;
		LinearMove( move, speed );
		SetMoveDone(&CMomentaryDoor:: MomentaryMoveDone );
	}
}

void CMomentaryDoor::MomentaryMoveDone( void )
{
	m_iState = STATE_OFF;
	STOP_SOUND(ENT(pev), CHAN_STATIC, (char*)STRING(pev->noiseMoving));
}
