#include <math.h>
#include <string.h>
#include "common.h"
#include "game.h"
#include "sectors.h"


/**
 *	Processes inputs and updates game state.  Call from
 *	main loop, synchronized to system tic / frame timer.
 *	@return GRPHTIC_e bitfield:<br>
 *	GRPHTIC_MOVED     movement occurred (screen contents likely changed, needing an update)<br>
 *	GRPHTIC_COLLISION movement truncated by wall (maybe play a sound)
 */
int doGameTic(GameContext* game) {
	bool moveFlag = false, collisionFlag = false;
	int r = 0;
	union Player_u {
		Vec3 mv;
		EntityPos ep;
	} pl;

	if (game == NULL)
		return 0;

	pl.ep.a = game->player.a;
	pl.ep.secId = game->player.secId;
	moveFlag = getControlMovement(game, &pl.ep);
	game->player.a = pl.ep.a;

	if (moveFlag) {
		collisionFlag = collisionDetect(game, &game->player, &pl.mv);

		//	Place camera as desired
		memcpy((void*)&game->grph.cam, (void*)&game->player, sizeof(game->grph.cam));
	}

	if (moveFlag) r |= GAMETIC_MOVED;
	if (collisionFlag) r |= GAMETIC_COLLISION;

	return r;
}

/**
 *	Turns controls[] into a movement vector.
 *	@param game  (in) Movement action flags (controls[])<br>
 *	ent  (in/out) Movement vector:<br>
 *		a  (in) required to orient movement; (out) resulting (absolute) angle<br>
 *		x, y, z (out) position change<br>
 *		secId  (not used)
 *
 */
bool getControlMovement(GameContext* game, EntityPos* ent) {
	bool moveFlag = false;
	float angsin, angcos;

	if (game == NULL || ent == NULL)
		return false;

	angsin = sinf(ent->a); angcos = cosf(ent->a);
	ent->x = 0.0f; ent->y = 0.0f; ent->z = 0.0f;

	if (game->controls[GAMECTRL_FWD]) {
		ent->x += GAME_MOVESTEP * angcos;
		ent->y += GAME_MOVESTEP * angsin;
		moveFlag = true;
	}
	if (game->controls[GAMECTRL_REV]) {
		ent->x -= GAME_MOVESTEP * angcos;
		ent->y -= GAME_MOVESTEP * angsin;
		moveFlag = true;
	}
	if (game->controls[GAMECTRL_TURNLEFT]) {
		ent->a += GAME_ANGLESTEP;
		moveFlag = true;
	}
	if (game->controls[GAMECTRL_TURNRIGHT]) {
		ent->a -= GAME_ANGLESTEP;
		moveFlag = true;
	}
	if (game->controls[GAMECTRL_STRAFELEFT]) {
		ent->x -= GAME_MOVESTEP * angsin;
		ent->y += GAME_MOVESTEP * angcos;
		moveFlag = true;
	}
	if (game->controls[GAMECTRL_STRAFERIGHT]) {
		ent->x += GAME_MOVESTEP * angsin;
		ent->y -= GAME_MOVESTEP * angcos;
		moveFlag = true;
	}
	if (game->controls[GAMECTRL_FLYUP]) {
		ent->z += GAME_MOVESTEP;
		moveFlag = true;
	}
	if (game->controls[GAMECTRL_FLYDOWN]) {
		ent->z -= GAME_MOVESTEP;
		moveFlag = true;
	}
	if (ent->a < 0) ent->a += 2 * M_PI;
	if (ent->a >= 2 * M_PI) ent->a -= 2 * M_PI;

	return moveFlag;
}

/**
 *	Checks if the specified move is allowable, and updates the entity's
 *	world location accordingly.
 *	@param game  (in) Gamestate context object<br>
 *	ent  (in/out) Entity to move<br>
 *	mv   (in) Desired movement in X, Y, Z direction; (out) actual movement
 *	@return true if there was a collision
 */
bool collisionDetect(GameContext* game, EntityPos* ent, Vec3* mv) {
//	float checkX[4], checkY[4];
//	int pointNum[4];
//	float changedX, changedY;
//	int blockX, blockY;
//	int corners = 0;
//	int index;

	if (game == NULL || ent == NULL || mv == NULL)
		return false;
/*
	changedX = game->camX + mv.x; changedY = game->camY + mv.y;
	//	Check if the move is even on the map
	if (changedX < 1 || changedY < 1 || changedX >= (game->mapW - 1)
			|| changedY >= (game->mapH - 1))
		//	Out of bounds?  Hit the edge, don't move
		return true;
	//	Check for map collisions
	//	Lower right corner of player
	checkX[0] = changedX + GAME_PLAYERSZ; checkY[0] = changedY + GAME_PLAYERSZ;
	//	lower left
	checkX[1] = changedX - GAME_PLAYERSZ; checkY[1] = changedY + GAME_PLAYERSZ;
	//	upper left
	checkX[2] = changedX - GAME_PLAYERSZ; checkY[2] = changedY - GAME_PLAYERSZ;
	//	upper right
	checkX[3] = changedX + GAME_PLAYERSZ; checkY[3] = changedY - GAME_PLAYERSZ;
	for (int i = 0; i < 4; i++) {
		//	corners counts the number of points that are faulty:
		//	0 = nothing, 1 = an outside corner, 2 = one side (or a
		//	diagonal pinch), 3 = inside corner, 4 = trapped.
		if (game->world[((int)checkX[i]) + ((int)checkY[i]) * game->mapW] > 0) {
			pointNum[corners] = i;
			corners++;
		}
	}
	switch (corners) {
	case 0:
		//	No collisions, make the move
		game->camX += mv.x; game->camY += mv.y;
		return false;
	case 1:
		//	1 collision: an outside wall corner at pointNum[0].
		blockX = (int)checkX[pointNum[0]];
		blockY = (int)checkY[pointNum[0]];
		switch (pointNum[0]) {
		case 0:
			//	Lower right corner of player is stuck
			if (changedY > (blockY + changedX - blockX)) {
				if (mv.x > 0) mv.x = 0;
			} else {
				if (mv.y > 0) mv.y = 0;
			}
			break;
		case 1:
			//	Lower left
			blockX++;
			if (changedY > (blockY - changedX + blockX)) {
				if (mv.x < 0) mv.x = 0;
			} else {
				if (mv.y > 0) mv.y = 0;
			}
			break;
		case 2:
			//	Upper left
			blockY++; blockX++;
			if (changedY > (blockY + changedX - blockX)) {
				if (mv.y < 0) mv.y = 0;
			} else {
				if (mv.x < 0) mv.x = 0;
			}
			break;
		case 3:
			//	Upper right
			blockY++;
			if (changedY > (blockY - changedX + blockX)) {
				if (mv.y < 0) mv.y = 0;
			} else {
				if (mv.x > 0) mv.x = 0;
			}
			break;
		default:
			break;
		}
		break;
	case 2:
		//	Wall hit.
		index = pointNum[0] + pointNum[1] * 4;
		switch (index) {
		case 4:
			//	Bottom of player (0, 1)
			if (mv.y > 0) mv.y = 0;
			break;
		case 9:
			//	Left (1, 2)
			if (mv.x < 0) mv.x = 0;
			break;
		case 14:
			//	Top (2, 3)
			if (mv.y < 0) mv.y = 0;
			break;
		case 12:
			//	Right (3, 0)
			if (mv.x > 0) mv.x = 0;
			break;
		case 8:
		case 13:
			//	Diagonal corners (partial inside corners (0, 2), (1, 3))
			mv.x = 0; mv.y = 0;
			break;
		default:
			break;
		}
		break;
	case 3:
		//	Inside corner: one free.  Allow motion in that direction.
		index = pointNum[0] + pointNum[1] * 4 + pointNum[2] * 16;
		switch (index) {
		case 57:
			//	Bottom right open (stuck points 1, 2, 3)
			if (mv.x < 0) mv.x = 0;
			if (mv.y < 0) mv.y = 0;
			break;
		case 56:
			//	Bottom left (0, 2, 3)
			if (mv.x > 0) mv.x = 0;
			if (mv.y < 0) mv.y = 0;
			break;
		case 52:
			//	Top left (0, 1, 3)
			if (mv.x > 0) mv.x = 0;
			if (mv.y > 0) mv.y = 0;
			break;
		case 36:
			//	Top right (0, 1, 2)
			if (mv.x < 0) mv.x = 0;
			if (mv.y > 0) mv.y = 0;
			break;
		default:
			break;
		}
		break;
	default:
		//	Stuck in a block?!
		break;
	}
*/
	ent->x += mv->x; ent->y += mv->y; ent->z += mv->z;

	return false;
}
