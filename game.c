#include <math.h>
#include <stdio.h>
#include <string.h>
#include "common.h"
#include "game.h"
#include "sectors.h"


/**
 *	Conditions an input axis, applying offset, cutting
 *	out a deadband near zero, and scaling and clamping
 *	to a signed 16-bit range.
 *	Because sensitivity is multiplicative, an input
 *	range of (+/-) a few hundred to a few thousand
 *	will deliver the most comfortable input resolution
 *	and sensitivity adjustment.
 *	@param x  (in/out) value to condition
 *	cal  (in) axis calibration parameters
 */
void trimControlAxis(int* x, AxisCal* cal) {
	int xx = *x;

	xx += cal->offset;
	if (xx > 0) {
		xx = xx - cal->deadband;
		if (xx < 0) xx = 0;
	} else if (xx < 0) {
		xx = xx + cal->deadband;
		if (xx > 0) xx = 0;
	}
	xx = xx * cal->sensitivity;
	if (xx < -32768) xx = -32768;
	if (xx >  32767) xx =  32767;
	*x = xx;

}

/**
 *	Processes inputs and updates game state.  Call from
 *	main loop, synchronized to system tic / frame timer.
 *	@return GRPHTIC_e bitfield:<br>
 *	GRPHTIC_MOVED     movement occurred (screen contents
 *	likely changed, needing an update)<br>
 *	GRPHTIC_COLLISION movement truncated by wall (maybe play a sound?)
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

	memcpy((void*)&pl.ep, (void*)&game->player, sizeof(pl.ep));
	trimControlAxis(&game->ctrl.xAxis, &game->calX);
	trimControlAxis(&game->ctrl.yAxis, &game->calY);
	moveFlag = getControlMovement(&game->ctrl, &pl.ep);
	game->player.a = pl.ep.a;

	if (moveFlag) {
		collisionFlag = collisionDetect(game, &game->player, &pl.mv, GAME_COLLISION_PORTALS);

		//	Place camera as desired
		memcpy((void*)&game->grph.cam, (void*)&game->player, sizeof(game->grph.cam));
		//	assuming player entity is snapped to normal floor/ceiling range, add camera height offset
		game->grph.cam.z += GAME_PLAYERCAM;
	}

	if (moveFlag) r |= GAMETIC_MOVED;
	if (collisionFlag) r |= GAMETIC_COLLISION;

	return r;
}

/**
 *	Turns a Controls struct into an entity movement vector.
 *	@param ctrl (in) Movement action flags and axes<br>
 *	ent  (in/out) Movement vector:<br>
 *		x, y, z (out) position change<br>
 *		a  (in) required to orient movement;
 *		   (out) resulting (absolute) angle<br>
 *		secId  (not used)
 *
 */
bool getControlMovement(Controls* ctrl, EntityPos* ent) {
	bool moveFlag = false;
	float angsin, angcos, step = GAME_MOVESTEP;

	if (ctrl == NULL || ent == NULL)
		return false;

	angsin = sinf(ent->a); angcos = cosf(ent->a);
	ent->x = 0.0f; ent->y = 0.0f; ent->z = 0.0f;

	ent->x += (float)ctrl->yAxis * (1.0f / 32768.0f) * angcos;
	ent->y += (float)ctrl->yAxis * (1.0f / 32768.0f) * angsin;
	if (ctrl->buttons & GAMECTRL_SLIDE) {
		ent->x += (float)ctrl->xAxis * (1.0f / 32768.0f) * angsin;
		ent->y -= (float)ctrl->xAxis * (1.0f / 32768.0f) * angcos;
	} else {
		ent->a -= (float)ctrl->xAxis * (1.0f /  2048.0f) * GAME_ANGLESTEP;
	}
	if (ctrl->xAxis != 0 || ctrl->yAxis != 0) moveFlag = true;

	if (ctrl->buttons & GAMECTRL_SPEED) {
		step *= GAME_ACCELERATOR;
	}
	if (ctrl->buttons & GAMECTRL_FWD) {
		ent->x += step * angcos;
		ent->y += step * angsin;
		moveFlag = true;
	}
	if (ctrl->buttons & GAMECTRL_REV) {
		ent->x -= step * angcos;
		ent->y -= step * angsin;
		moveFlag = true;
	}
	if (ctrl->buttons & GAMECTRL_TURNLEFT) {
		ent->a += ((ctrl->buttons & GAMECTRL_SPEED) ? GAME_ANGLESTEP * 2 : GAME_ANGLESTEP);
		moveFlag = true;
	}
	if (ctrl->buttons & GAMECTRL_TURNRIGHT) {
		ent->a -= ((ctrl->buttons & GAMECTRL_SPEED) ? GAME_ANGLESTEP * 2 : GAME_ANGLESTEP);
		moveFlag = true;
	}
	if (ctrl->buttons & GAMECTRL_STRAFELEFT) {
		ent->x -= step * angsin;
		ent->y += step * angcos;
		moveFlag = true;
	}
	if (ctrl->buttons & GAMECTRL_STRAFERIGHT) {
		ent->x += step * angsin;
		ent->y -= step * angcos;
		moveFlag = true;
	}
	if (ctrl->buttons & GAMECTRL_FLYUP) {
		ent->z += step;
		moveFlag = true;
	}
	if (ctrl->buttons & GAMECTRL_FLYDOWN) {
		ent->z -= step;
		moveFlag = true;
	}
	if (ent->a < 0) ent->a += 2 * M_PI;
	if (ent->a >= 2 * M_PI) ent->a -= 2 * M_PI;

	return moveFlag;
}

/**
 *	Checks if the specified move is allowable, and updates the entity's
 *	world location accordingly.
 *	(Return value reports collision in x-y plane; z collision isn't used
 *	in its calculation.)
 *	@param game  (in) Gamestate context object (map data needed)<br>
 *	ent   (in/out) Entity to move<br>
 *	mv    (in) Desired movement in X, Y, Z direction;
 *	      (out) actual movement<br>
 *	type  (in) one of GAME_COLLISION_e, which type of collision check
 *	           to perform
 *	@return bit flags, GAMETIC_COLLISION or GAMETIC_PORTAL
 */
int collisionDetect(GameContext* game, EntityPos* ent, Vec3* mv, int type) {
	int r = 0;
	int i, sides, prevSec = -1;
	Intersect intr;
	Sector* curSec;

	if (game == NULL || ent == NULL || mv == NULL || game->wrl.sectors == NULL
			|| game->wrl.sectorsLen < 1 || ent->secId < 0 || ent->secId >= game->wrl.sectorsLen)
		return false;

	if (type <= GAME_COLLISION_NONE || type > GAME_COLLISION_SNAP) {

		ent->x += mv->x; ent->y += mv->y; ent->z += mv->z;
		return false;

	} else if (type == GAME_COLLISION_PORTALS) {

		/*
		 *	Starting from ent's sector,
		 *	- Check (ent, ent + mv) intersection with sides of sector
		 *	- If a 2-sided line intersected is from front, move to
		 *	  the intersection point (subtract difference from mv),
		 *	  and move to the new sector
		 *	- repeat until mv is consumed
		 */
		intr.orig.x = ent->x; intr.orig.y = ent->y;
		intr.dir.x = mv->x; intr.dir.y = mv->y;
		do {

			curSec = &game->wrl.sectors[ent->secId];
			intr.end.x = curSec->start.x; intr.end.y = curSec->start.y;
			sides = curSec->sides;

			for (i = 0; i < sides; i++) {
				intr.start.x = intr.end.x; intr.start.y = intr.end.y;
				intr.end.x = curSec->side[i].to.x; intr.end.y = curSec->side[i].to.y;
				if (curSec->side[i].secId >= 0) {
					bool f = intersect(&intr);
					//bool intersectsFront = ((intr.end.x - intr.start.x) * intr.dir.y - intr.dir.x * (intr.end.y - intr.start.y)) > 0;//GRPH_PLANECOMPARE;
					if (f && intr.side > 0 && /* intersectsFront && */ intr.dist > 0 && intr.dist <= 1) {
						//	move intersects facing side
#ifdef COLLISION_DEBUG
						printf("2-sided collision, dist %e", intr.dist);
#endif // COLLISION_DEBUG
						r |= GAMETIC_PORTAL;
						intr.orig.x += intr.dist * intr.dir.x; intr.orig.y += intr.dist * intr.dir.y;
						intr.dir.x -= intr.dist * intr.dir.x; intr.dir.y -= intr.dist * intr.dir.y;
						int s = curSec->side[i].secId;
						if (s != prevSec && s >= 0 && s < game->wrl.sectorsLen) {
#ifdef COLLISION_DEBUG
							printf(", from sec %i to %i", ent->secId, s);
#endif // COLLISION_DEBUG
							prevSec = ent->secId;
							ent->secId = s;
						}
						break;
					}
				}
			}
			if (i >= sides) {
				//	for loop terminated without break -->
				//	movement isn't crossing a line, complete it
				intr.orig.x += intr.dir.x; intr.orig.y += intr.dir.y;
				break;
			}
		} while (fabsf(intr.dir.x) + fabsf(intr.dir.y) > GAME_MOVE_EPSILON);

		ent->x = intr.orig.x; ent->y = intr.orig.y;
#ifdef COLLISION_DEBUG
		if (r) printf("\nnew pos: %e, %e\n", ent->x, ent->y);
#endif // COLLISION_DEBUG

		float tmpZ = ent->z + mv->z;
		if (tmpZ < curSec->floorHeight) tmpZ = curSec->floorHeight;
		if (tmpZ > curSec->ceilHeight - ent->height) tmpZ = curSec->ceilHeight - ent->height;
		mv->z = tmpZ - ent->z;
		ent->z = tmpZ;

		return r;
	}
	/*
	 *	Perform vector collision where possible. That is, from the
	 *	initial position to the final position (ent.x/y/z + mv.x/y/z),
	 *	check all points along that path.
	 *	The final position will not be ent + mv, if collision occurred;
	 *	but in particular, it need not be colinear between (ent,
	 *	ent + mv): to make wall sliding more satisfying, motion along
	 *	the projection of mv onto the colliding wall is used.
	 *	Movement is performed in segments, until the full movement
	 *	vector is consumed: clear space until a wall or portal
	 *	collision, then the projection along that line, or the
	 *	continuation of the remaining movement vector into the next
	 *	sector, and so on.
	   In general, this will be an
	 *	  intersection of lines problem, with special cases handing
	 *	  start/end of lines, and corners (particularly because convex
	 *	  corners are a product of one side within a sector, and one
	 *	  side in the next (adjacent) sector, straddling a 2-sided wall
	 *	  -- or more if sectors are thin!).
	 *	- Generally, geometry is virtually shrunk by GAME_PLAYERSZ so
	 *	  that a point moves within that reduced interior space, as
	 *	  opposed to testing a circular entity collider.
	 *	- Corners are handled by simple intersection (convex sides),
	 *	  or closing what would otherwise be an opening, with a single
	 *	  diagonal line segment.  This permits movement quite close to
	 *	  the apex of a very tight corner; avoid using corners over
	 *	  90° or so for this reason.
	 *	- First, if applicable, snap initial location within the initial
	 *	  sector:
	 *		- Check sidedness.  If not all sides show location as front-
	 *		  facing, that location is outside the sector.
	 *		- Find the nearest side(s) and intersect them:
	 *		- If 0 <= .frac < 1, project to that side's position;
	 *		- If one side has .frac >= 1, and the next < 0, find the
	 *		  reduced position of the vertex between those sides (i.e.,
	 *		  where the sides intersect after a lateral displacement
	 *		  of GAME_PLAYERSZ).
	 *	- Where a move intersects only one (1-sided) wall, use the
	 *	  projection along that line.
	 *	- Where a move intersects a wall beyond its end (Intersect.frac
	 *	  >= 1),
	 *	Depending on collision flags, check sides of the current sector:
	 *	- Tentatively add
	 *	- Quick elimination with rectangular bounding box (+GAME_PLAYERSZ);
	 *	  this particularly optimizes rectilinear lines
	 *	- Of passing sides, compute exact distance (or comparison via
	 *	  norm-squared weighted distance) (option: pre-compute side length
	 *	  to avoid sqrt() in loop)
	 *	- Since a sector is closed and convex, all
	 *
	 *	Collision flags: force into sector; all/normal collision; only 2-
	 *	sided (take portals, ignore 1-sideds); none at all (basically
	 *	freecam in current sector).
	 */

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

	return false;
}
