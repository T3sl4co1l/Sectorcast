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
/** Square bounding box for the player's collision */
#define GAME_PLAYERSZ 	0.4f


/* * * Type Declarations * * */

/** Flags returned from doGameTic() */
enum GAMETIC_e {
	GAMETIC_MOVED     = 0x0001,	/**< Movement occurred; redraw display  */
	GAMETIC_COLLISION = 0x0002,	/**< Collision occurred during movement */
};

//	For more definitions, see sectors.h

/* * * Shared Globals * * */

int doGameTic(GameContext* game);
bool getControlMovement(GameContext* game, EntityPos* ent);
bool collisionDetect(GameContext* game, EntityPos* ent, Vec3* mv);

#endif // GAME_H_INCLUDED
