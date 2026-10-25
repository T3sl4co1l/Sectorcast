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
#define GAME_PLAYERSZ 	0.2f
/** Player collision height */
#define GAME_PLAYERHT 	0.4f
/** Player camera height (from bottom / z coordinate) */
#define GAME_PLAYERCAM	0.3f
/** Smallest move increment for collision checking */
#define GAME_MOVE_EPSILON	1e-8

/* * * Type Declarations * * */

/** Flags returned from doGameTic() */
enum GAMETIC_e {
	GAMETIC_MOVED     = 0x0001,	/**< Movement occurred; redraw display  */
	GAMETIC_COLLISION = 0x0002,	/**< Collision with single-sided wall or ledge */
	GAMETIC_PORTAL    = 0x0004,	/**< Crossed two-sided wall */
};

/** Flag parameter for collisionDetect() */
enum GAME_COLLISION_e {
	GAME_COLLISION_NONE,   	/**< No collision checks */
	GAME_COLLISION_PORTALS,	/**< No wall collision checks; portals are taken (entity sector updated) (returns true when crossing 2-sided lines) */
	GAME_COLLISION_NORMAL, 	/**< Normal movement, blocked by 1-sided walls and obstacles; free movement if out of bounds */
	GAME_COLLISION_SNAP,   	/**< Snapped to nearest location within sector before applying movement (then NORMAL collision) */
	GAME_COLLISION_CHEAP,  	/**< If collision would occur, don't move and return true */
};

//	For more definitions, see sectors.h

/* * * Shared Globals * * */

int doGameTic(GameContext* game);
bool getControlMovement(Controls* ctrl, EntityPos* ent);
int collisionDetect(GameContext* game, EntityPos* ent, Vec3* mv, int type);

#endif // GAME_H_INCLUDED
