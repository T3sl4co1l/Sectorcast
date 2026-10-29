#include <math.h>
#include <stdio.h>
#include <string.h>
#include "common.h"
#include "game.h"
#include "sectors.h"


//#define COLLISION_DEBUG

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
	bool moveFlag = false;
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
		r = collisionDetect(game, &game->player, &pl.mv,
				(game->ctrl.buttons & GAMECTRL_NOCLIP) ? GAME_COLLISION_PORTALS : GAME_COLLISION_CHEAP
				) | GAMETIC_MOVED;
		//	Place camera as desired
		memcpy((void*)&game->grph.cam, (void*)&game->player, sizeof(game->grph.cam));
		//	assuming player entity is snapped to normal floor/ceiling range, add camera height offset
		game->grph.cam.z += GAME_PLAYERCAM;
	}

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
		ent->x += step * angcos; ent->y += step * angsin;
		moveFlag = true;
	}
	if (ctrl->buttons & GAMECTRL_REV) {
		ent->x -= step * angcos; ent->y -= step * angsin;
		moveFlag = true;
	}
	if (ctrl->buttons & GAMECTRL_TURNLEFT) {
		if (ctrl->buttons & GAMECTRL_SLIDE) {
			ent->x -= step * angsin; ent->y += step * angcos;
		} else {
			ent->a += ((ctrl->buttons & GAMECTRL_SPEED) ? GAME_ANGLESTEP * 2 : GAME_ANGLESTEP);
		}
		moveFlag = true;
	}
	if (ctrl->buttons & GAMECTRL_TURNRIGHT) {
		if (ctrl->buttons & GAMECTRL_SLIDE) {
			ent->x += step * angsin; ent->y -= step * angcos;
		} else {
			ent->a -= ((ctrl->buttons & GAMECTRL_SPEED) ? GAME_ANGLESTEP * 2 : GAME_ANGLESTEP);
		}
		moveFlag = true;
	}
	if (ctrl->buttons & GAMECTRL_STRAFELEFT) {
		ent->x -= step * angsin; ent->y += step * angcos;
		moveFlag = true;
	}
	if (ctrl->buttons & GAMECTRL_STRAFERIGHT) {
		ent->x += step * angsin; ent->y -= step * angcos;
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

	} else if (type == GAME_COLLISION_PORTALS || type == GAME_COLLISION_CHEAP) {

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
					if (f && intr.side > 0 && intr.frac >= 0 && intr.frac < 1 && intr.dist > 0 && intr.dist <= 1) {
						//	move intersects facing side
						if (curSec->side[i].secId >= 0) {
							//	two-sided (portal), take it
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
				} else if (type == GAME_COLLISION_CHEAP) {
					//	single-sided, check if close enough to collide
					PointNearLine pnl = {
						{ intr.start.x, intr.start.y },
						{ intr.end.x, intr.end.y },
						{ intr.orig.x + intr.dir.x, intr.orig.y + intr.dir.y },
						ent->radius,
						true
					};
					if (isNearLine(&pnl)) {
						//	movement would go into a wall hitbox or out-of-bounds, cancel the remainder
#ifdef COLLISION_DEBUG
						printf("1-sided collision, sec %i, side %i, distSq %e, crossSq %e\n", ent->secId, i, distSq, crossSq);
#endif // COLLISION_DEBUG
						intr.dir.x = 0; intr.dir.y = 0;
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

	} else { // if (type == GAME_COLLISION_NORMAL || type == GAME_COLLISION_SNAP) {

		if (type == GAME_COLLISION_SNAP) {
			//	iterate over sides of sector, translating inward by radius; find nearest
		}

	/*
	 *	Performs full vector collision.  That is, check all points
	 *	along the path; not just stepwise, or stopping at the first
	 *	obstacle.  Compute exact distances, and bend the path as
	 *	needed to slide along geometry in a reasonable manner.
	 *
	 *	If collision occurred, the final position will not be ent +
	 *	mv; indeed the move might be aside the initial vector, to
	 *	slide along the wall a bit (namely, the projection of mv on
	 *	the wall gives the end point; assuming the end point didn't
	 *	run off the end of the wall).
	 *
	 *	Movement is performed in segments, until the full movement
	 *	vector is consumed: move through clear space until a wall or
	 *	portal collision, then move by the projection of that line,
	 *	or the remaining movement vector into the next sector, and
	 *	so on.
	 *
	 *	Line ends are handled as semicircles.  This is complicated
	 *	to solve, but besides providing the smoothest experience, it
	 *	solves the problem of trying to follow line ends around
	 *	concave corners (potentially through multiple sectors) and
	 *	somehow capping them off.  Instead of having to resolve
	 *	structure (sector connections), the map is flattened: all
	 *	lines in the potential collision area are collected into an
	 *	array.
	 *
	 *	We can also displace colliding walls by ent->radius at the
	 *	same time, so the solution uses simple line intersections
	 *	(..and circles).
	 *
	 *	Since collision can slide along a wall, the potential
	 *	collision area (region around ent) is described by a circle
	 *	not just around ent, but somewhat ahead (namely,
	 *	ent + mv / 2), of radius ent->radius + |mv| / 2.
	 *
	 *	The remaining problem is overlapping geometry.  We have five
	 *	cases in total:
	 *	- None: free movement
	 *	- Line or circle: slide along it (mv --> Proj(mv, line),
	 *	  etc.)
	 *	- Two lines: when sliding along the one line, another line
	 *	  is met.  Slide to the intersection, then slide along the
	 *	  second line; if this second move would cross the first
	 *	  line, instead stop at the intersection.
	 *	- Line and circle: similarly, slide along one until reaching
	 *	  the intersection.  A touching line and circle have two
	 *	  intersections (or one degenerate); resolve which one in
	 *	  the correct direction(!).
	 *	- Two circles: similarly, slide along one until reaching the
	 *	  intersection.  Two overlapping circles, of the same size,
	 *	  have two intersections (or one degenerate, or the full
	 *	  locus of the circle if in exactly the same place -- but
	 *	  this is prevented by removing duplicate vertices from the
	 *	  list).
	 *
	 *	There is something of a state machine here, and transitions
	 *	between all states can occur.  A free move can bump into
	 *	a line or circle; a projected move can slide off the end of
	 *	a line, into the adjoining circle; another circle could be
	 *	nearby (overlapping) and movement ends in an inside corner.
	 *	When a collision occurs, the colliding object is recorded,
	 *	so that it can be: 1. ignored for the move along it, since
	 *	that will give unstable or degenerate numerical results; and
	 *	2. to keep it in context in case an intersection occurs.
	 *	Thus an intersection is computed with the new collision and
	 *	the previous (in-context) object.
	 */

		return r;
	}

}

/**
 *	Checks if a point is within given distance of a line.
 *	@param pnl
 */
bool isNearLine(PointNearLine* pnl) {

	float distSq = pnl->dist * pnl->dist;
	float sideNormSq = (pnl->end.x - pnl->start.x) * (pnl->end.x - pnl->start.x)
			+ (pnl->end.y - pnl->start.y) * (pnl->end.y - pnl->start.y);
	distSq *= sideNormSq;
	float crossSq = (pnl->end.x - pnl->start.x) * (pnl->point.y - pnl->start.y)
			- (pnl->point.x - pnl->start.x) * (pnl->end.y - pnl->start.y);

	return (crossSq * crossSq < distSq || (pnl->excludeBack && crossSq > 0));
}
