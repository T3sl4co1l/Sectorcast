#ifndef GAME_H_INCLUDED
#define GAME_H_INCLUDED

#include <stdbool.h>
#include <stdint.h>
#include "sectors.h"

/* * * Configuration Defines * * */

/** Frame rate (1/gametic timer) */
#define GAME_FRAMERATE	60
/** Angle (in radians) turned per gametic */
#define GAME_ANGLESTEP	(M_PI / 52.0f)
/** Fraction of a grid square moved in one gametic */
#define GAME_MOVESTEP 	0.08f
/** Speedup factor when holding GAMECTRL_SPEED */
#define GAME_ACCELERATOR	1.6f
/** Player collision radius */
#define GAME_PLAYERSZ 	0.3f
/** Player collision height */
#define GAME_PLAYERHT 	0.5f
/** Player camera height (from bottom / z coordinate) */
#define GAME_PLAYERCAM	0.36f
/** Smallest move increment for collision checking */
#define GAME_MOVE_EPSILON	1e-8


/* * * Type Declarations * * */

/** Flags returned from doGameTic() */
enum GAMETIC_e {
	GAMETIC_MOVED     = 0x0001,	/**< Movement occurred; redraw display  */
	GAMETIC_COLLISION = 0x0002,	/**< Collision with single-sided wall or ledge */
	GAMETIC_PORTAL    = 0x0004,	/**< Crossed two-sided wall */
};

/** Controls.buttons bit fields */
enum GAMECTRL_e {
	GAMECTRL_FWD         = 0x0001,
	GAMECTRL_REV         = 0x0002,
	GAMECTRL_TURNLEFT    = 0x0004,
	GAMECTRL_TURNRIGHT   = 0x0008,
	GAMECTRL_STRAFELEFT  = 0x0010,
	GAMECTRL_STRAFERIGHT = 0x0020,
	GAMECTRL_FLYUP       = 0x0040,
	GAMECTRL_FLYDOWN     = 0x0080,
	GAMECTRL_SPEED       = 0x0100,
	GAMECTRL_SLIDE       = 0x0200,
	GAMECTRL_NOCLIP      = 0x0400
};

/** Flag parameter for collisionDetect() */
enum GAME_COLLISION_e {
	GAME_COLLISION_NONE,   	/**< No collision checks (free X/Y/Z movement, no sector update) */
	GAME_COLLISION_PORTALS,	/**< No obstacle collision checks, but portals are taken; snap Z within destination sector ceil/floor bounds */
	GAME_COLLISION_CHEAP,  	/**< PORTALS, but if obstacle collision would occur, don't move */
	GAME_COLLISION_NORMAL, 	/**< Normal movement, collision with 1-sided walls and low ceil/floor differences; free movement out-of-bounds */
	GAME_COLLISION_SNAP,   	/**< OoB snapped to nearest in-bounds location within sector, before applying NORMAL movement */
};

//	For more definitions, see sectors.h


/* * * Shared Globals * * */

int doGameTic(GameContext* game);
bool getControlMovement(Controls* ctrl, EntityPos* ent);
int collisionDetect(GameContext* game, EntityPos* ent, Vec3* mv, int type);

#endif // GAME_H_INCLUDED
