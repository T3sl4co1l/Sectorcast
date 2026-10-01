/*
 *	Sector-lines-portals based Pseudo-3D Graphics Engine
 *	By Tim Williams, 2026-09-17
 *
 *	Concept is basically a Build engine, with some baked-in
 *	logistics, and polygonal drawing of walls/floors.
 *
 *	Intent is, a core render loop that uses minimal RAM, and assumes
 *	fast loading from external/persistent storage (Flash, or a
 *	drive, hopefully with a little cache), that could run
 *	comfortably on an embedded system.
 *
 *	Data structures:
 *	- Sector: a starting Vertex and list of Sides.  A side is a
 *	vertex to which a line is drawn implicitly, from the starting
 *	point, to the side's vertex, and so on, forming a closed convex
 *	polygon on the map, and in 3D, a prism with floor and ceiling.
 *	(The final side is assigned the starting coordinate for
 *	convenience.)
 *	- Side: a face of a Sector; a line on the map with 1 or 2 sides.
 *	If single-sided, has a flat color or texture, drawn from floor
 *	to ceiling.  If two-sided, references an adjoining sector (and
 *	instead of one wall texture, has upper and lower parts, filling
 *	in the gaps between ceilings/floors).  Two-sided lines act as
 *	portals, rendering the next sector through the aperture left by
 *	the nearest floor/ceiling,
 *
 *	Textures are referenced by ID in a table; one-sided walls have a
 *	color or texture, and take the palette of the parent sector.
 *	Two-sided lines have upper and lower textures specified in the
 *	table (cf. a Doom sidedef).
 *
 *	A Side also provides triggers for interaction.  Two-sided lines
 *	act as triggers to move between sectors: when the movement
 *	vector overlaps a two-sided line, position is advanced to the
 *	intersection with that line, the current sector is switched to
 *	the adjoining one, and remaining movement is completed following
 *	the same process recursively, until the movement vector has been
 *	depleted.  Likewise, a collision check involving a two-sided
 *	line brings in all lines of the adjoining sector, and so on.
 *
 *	Portals are a recursive rendering technique.  The camera starts
 *	in an initial (given) Sector.  All (one-sided) walls are drawn,
 *	perspective transforming and culling to FOV as usual.  All two-
 *	sided walls in view, are pushed onto a stack.  Until the stack
 *	is empty: pop a sector, and render it through the respective
 *	line.  (Sectors are generally coordinate-independent, and can be
 *	centered on axis individually; the paired lines have to have the
 *	same axis-aligned span, x[n] - x[n-1], y[n] - y[n-1].)
 */

#include <stddef.h>
#include <stdio.h>
#include <math.h>
#include "sectors.h"

Sector_t sectors[] = {
	{
		id: 0,
		sides: 4,
		pal: 0,
		ceil: 0,
		floor: 1,
		start: { -2.0f, -1.3f },
		side: {
			{
				sectorId: -1,
				sectorSide: 0,
				wallId: 4,
				to: { -2.0f, 1.5f }
			}, {
				sectorId: -1,
				sectorSide: 0,
				wallId: 1,
				to: { 1.0f, 1.5f }
			}, {
				sectorId: -1,
				sectorSide: 0,
				wallId: 2,
				to: { 1.0f, -1.5f }
			}, {
				sectorId: -1,
				sectorSide: 0,
				wallId: 3,
				to: { -2.0f, -1.3f }
			}
		}
//	}, {
	}
};

/**
 *	Processes inputs and updates game state.  Call from
 *	main loop, synchronized to system tic / frame timer.
 *	@return RAYTIC_e bitfield:<br>
 *	RAYTIC_MOVED     movement occurred (screen contents likely changed, needing an update)<br>
 *	RAYTIC_COLLISION movement truncated by wall (maybe play a sound)
 */
int doGameTic(GrphContext_t* ctx) {
	float moveX = 0, moveY = 0;
	float angsin, angcos;
	bool moveFlag = false, collisionFlag = false;

	angsin = sinf(ctx->camA); angcos = cosf(ctx->camA);

	if (ctx->controls[GRPHCTRL_FWD]) {
		moveX += GRPH_MOVESTEP * angcos;
		moveY += GRPH_MOVESTEP * angsin;
		moveFlag = true;
	}
	if (ctx->controls[GRPHCTRL_REV]) {
		moveX -= GRPH_MOVESTEP * angcos;
		moveY -= GRPH_MOVESTEP * angsin;
		moveFlag = true;
	}
	if (ctx->controls[GRPHCTRL_TURNLEFT]) {
		ctx->camA += GRPH_ANGLESTEP;
		moveFlag = true;	}
	if (ctx->controls[GRPHCTRL_TURNRIGHT]) {
		ctx->camA -= GRPH_ANGLESTEP;
		moveFlag = true;
	}
	if (ctx->controls[GRPHCTRL_STRAFELEFT]) {
		moveX -= GRPH_MOVESTEP * angsin;
		moveY += GRPH_MOVESTEP * angcos;
		moveFlag = true;
	}
	if (ctx->controls[GRPHCTRL_STRAFERIGHT]) {
		moveX += GRPH_MOVESTEP * angsin;
		moveY -= GRPH_MOVESTEP * angcos;
		moveFlag = true;
	}
	if (ctx->camA < 0) ctx->camA += 2 * M_PI;
	if (ctx->camA >= 2 * M_PI) ctx->camA -= 2 * M_PI;
	if (moveFlag) {
		//collisionFlag = collisionDetect(ctx, moveX, moveY);
		ctx->camX += moveX; ctx->camY += moveY;
		render(ctx);
	}

	int r = 0;
	if (moveFlag) r |= GRPHTIC_MOVED;
	if (collisionFlag) r |= GRPHTIC_COLLISION;

	return r;
}

/**
 * Checks if the specified move is allowable.  Automatically updates
 * camX, camY.
 * @param moveX Movement in X direction
 * @param moveY Movement in Y direction
 * @return true if there was a collision
 */
bool collisionDetect(GrphContext_t* ctx, float moveX, float moveY) {
	float checkX[4], checkY[4];
	int pointNum[4];
	float changedX, changedY;
	int blockX, blockY;
	int corners = 0;
	int index;

	if (ctx == NULL || ctx->world == NULL)
		return false;

	changedX = ctx->camX + moveX; changedY = ctx->camY + moveY;
	//	Check if the move is even on the map
	if (changedX < 1 || changedY < 1 || changedX >= (ctx->mapW - 1)
			|| changedY >= (ctx->mapH - 1))
		//	Out of bounds?  Hit the edge, don't move
		return true;
	//	Check for map collisions
	//	Lower right corner of player
	checkX[0] = changedX + GRPH_PLAYERSZ; checkY[0] = changedY + GRPH_PLAYERSZ;
	//	lower left
	checkX[1] = changedX - GRPH_PLAYERSZ; checkY[1] = changedY + GRPH_PLAYERSZ;
	//	upper left
	checkX[2] = changedX - GRPH_PLAYERSZ; checkY[2] = changedY - GRPH_PLAYERSZ;
	//	upper right
	checkX[3] = changedX + GRPH_PLAYERSZ; checkY[3] = changedY - GRPH_PLAYERSZ;
	for (int i = 0; i < 4; i++) {
		//	corners counts the number of points that are faulty:
		//	0 = nothing, 1 = an outside corner, 2 = one side (or a
		//	diagonal pinch), 3 = inside corner, 4 = trapped.
		if (ctx->world[((int)checkX[i]) + ((int)checkY[i]) * ctx->mapW] > 0) {
			pointNum[corners] = i;
			corners++;
		}
	}
	switch (corners) {
	case 0:
		//	No collisions, make the move
		ctx->camX += moveX; ctx->camY += moveY;
		return false;
	case 1:
		//	1 collision: an outside wall corner at pointNum[0].
		blockX = (int)checkX[pointNum[0]];
		blockY = (int)checkY[pointNum[0]];
		switch (pointNum[0]) {
		case 0:
			//	Lower right corner of player is stuck
			if (changedY > (blockY + changedX - blockX)) {
				if (moveX > 0) moveX = 0;
			} else {
				if (moveY > 0) moveY = 0;
			}
			break;
		case 1:
			//	Lower left
			blockX++;
			if (changedY > (blockY - changedX + blockX)) {
				if (moveX < 0) moveX = 0;
			} else {
				if (moveY > 0) moveY = 0;
			}
			break;
		case 2:
			//	Upper left
			blockY++; blockX++;
			if (changedY > (blockY + changedX - blockX)) {
				if (moveY < 0) moveY = 0;
			} else {
				if (moveX < 0) moveX = 0;
			}
			break;
		case 3:
			//	Upper right
			blockY++;
			if (changedY > (blockY - changedX + blockX)) {
				if (moveY < 0) moveY = 0;
			} else {
				if (moveX > 0) moveX = 0;
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
			if (moveY > 0) moveY = 0;
			break;
		case 9:
			//	Left (1, 2)
			if (moveX < 0) moveX = 0;
			break;
		case 14:
			//	Top (2, 3)
			if (moveY < 0) moveY = 0;
			break;
		case 12:
			//	Right (3, 0)
			if (moveX > 0) moveX = 0;
			break;
		case 8:
		case 13:
			//	Diagonal corners (partial inside corners (0, 2), (1, 3))
			moveX = 0; moveY = 0;
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
			if (moveX < 0) moveX = 0;
			if (moveY < 0) moveY = 0;
			break;
		case 56:
			//	Bottom left (0, 2, 3)
			if (moveX > 0) moveX = 0;
			if (moveY < 0) moveY = 0;
			break;
		case 52:
			//	Top left (0, 1, 3)
			if (moveX > 0) moveX = 0;
			if (moveY > 0) moveY = 0;
			break;
		case 36:
			//	Top right (0, 1, 2)
			if (moveX < 0) moveX = 0;
			if (moveY > 0) moveY = 0;
			break;
		default:
			break;
		}
		break;
	default:
		//	Stuck in a block?!
		break;
	}
	ctx->camX += moveX; ctx->camY += moveY;

	return true;
}

/**
 * Transforms current information (world, camX, camY, camA..)
 * into column colors and heights (columns, colColors).
 */
void render(GrphContext_t* ctx) {
	int i, x;
	float angsin, angcos;
	Vertex delta;
	Sector_t* curSec;
	Intersect_t intr;

	if (ctx == NULL || ctx->sectors == NULL)
		return;

	//	Clear framebuffer
	for (x = 0; x < GRPH_COLUMNS; x++) {
		ctx->colColors[x] = 0;
		ctx->columns[x] = 0;
	}

	//	View plane is defined by vector; rotate to view angle (note angle is clockwise from +x axis)
	angsin = sinf(ctx->camA); angcos = cosf(ctx->camA);
	intr.orig.x = ctx->camX; intr.orig.y = ctx->camY;
	intr.dist = 1; intr.frac = 0;
	//	Each column vector is related to the previous by a linear change:
	intr.dir.x = angcos - angsin * GRPH_FOVMULT;
	intr.dir.y = angsin + angcos * GRPH_FOVMULT;
	delta.x = angsin *  2.0f * GRPH_COLSTEP;
	delta.y = angcos * -2.0f * GRPH_COLSTEP;

//	printf("orig.x: %e, orig.y: %e\n", intr.orig.x, intr.orig.y);

	for (x = 0; x < GRPH_COLUMNS; x++) {

//		if (x >= GRPH_COLUMNS / 2 - 1 && x <= GRPH_COLUMNS / 2 + 1)
//			printf("col.x: %e, col.y: %e\n", intr.dir.x, intr.dir.y);

		//	For each display column, loop over sides of the sector...
		curSec = &ctx->sectors[ctx->currentSectorId];
		intr.lineEnd.x = curSec->start.x; intr.lineEnd.y = curSec->start.y;

		for (i = 0; i < curSec->sides; i++) {
			//	Check lines in current sector for an intersection...
			intr.lineStart.x = intr.lineEnd.x; intr.lineStart.y = intr.lineEnd.y;
			intr.lineEnd.x = curSec->side[i].to.x;
			intr.lineEnd.y = curSec->side[i].to.y;
			bool f = intersect(&intr);
//			if (x >= GRPH_COLUMNS / 2 - 1 && x <= GRPH_COLUMNS / 2 + 1)
//				printf("side %i, intr side: %e, frac: %e, dist: %e\n", i, intr.side, intr.frac, intr.dist);
			if (f && intr.side > 0 && intr.dist > 0 && intr.frac >= 0 && intr.frac < 1) {
				//	Hit a side, plot it
				ctx->colColors[x] = curSec->side[i].wallId;
				if ((intr.lineEnd.y - intr.lineStart.y) * (intr.lineEnd.x - intr.lineStart.x) > 0)
					ctx->colColors[x] += 8;
				ctx->columns[x] = GRPH_HEIGHT / intr.dist;
				break;
			}
		}

		intr.dir.x += delta.x; intr.dir.y += delta.y;
	}
//	printf("\n");

}

/**
 *	Calculates useful parameters of the intersection of two
 *	lines in Cartesian coordinates.
 *	@param intr input geometry and return values.
 *	@return true on success; false if divide-by-zero (slopes
 *	are equal; intr->side sign is still valid) or intr == NULL.
 */
bool intersect(Intersect_t* intr) {

	if (intr == NULL) return false;

	Vertex d1 = { intr->lineEnd.x - intr->lineStart.x, intr->lineEnd.y - intr->lineStart.y };
	Vertex d2 = { intr->dir.x, intr->dir.y };
	Vertex d3 = { intr->orig.x - intr->lineStart.x, intr->orig.y - intr->lineStart.y };
	intr->side = d3.x * d1.y - d1.x * d3.y;
	float det = d1.x * d2.y - d2.x * d1.y;
	if (det == 0) return false;
	det = 1 / det;
	intr->frac = (d3.x * d2.y - d2.x * d3.y) * det;
	intr->dist = intr->side * det;
	//float normsq = d1.x * d1.x + d1.y * d1.y;
	//if (normsq != 0) intr->side = intr->side / normsq;

	return true;
}

/**
 *	Cast a ray across the map
 */
void rayCast(GrphContext_t* ctx, GrphRay_t* ray) {
	int blockX, blockY, magnitude;
	int dirX, dirY, i;

	if (ctx == NULL || ray == NULL || ctx->world == NULL)
		return;

	blockX = (int)ray->posX; blockY = (int)ray->posY;
	ray->colr = 0; ray->travel = 0;
	//	Normalization, if desired (defeats perspective here)
	//magnitude = sqrt(ray->slopeX * ray->slopeX + ray->slopeY * ray->slopeY);
	magnitude = 1;
	//	Amount traveled to the next axis intersection
	float travelX = magnitude / fabs(ray->slopeX);
	float travelY = magnitude / fabs(ray->slopeY);
	//	Distance to the first intersection (and each thereafter)
	float tIntV, tIntH;
	if (ray->slopeX > 0) {
		dirX = 1;
		tIntV = (1 - ray->posX + blockX) * travelX;
	} else {
		dirX = -1;
		tIntV = (ray->posX - blockX) * travelX;
	}
	if (ray->slopeY > 0) {
		dirY = 1;
		tIntH = (1 - ray->posY + blockY) * travelY;
	} else {
		dirY = -1;
		tIntH = (ray->posY - blockY) * travelY;
	}
	i = 0;
	do {
		if (tIntH < tIntV) {
			ray->travel = tIntH;
			blockY += dirY;
			tIntH += travelY;
			ray->wallHoriz = true;
		} else {
			ray->travel = tIntV;
			blockX += dirX;
			tIntV += travelX;
			ray->wallHoriz = false;
		}
		i++;
		if (blockX < 0 || blockY < 0 || blockX >= ctx->mapW || blockY >= ctx->mapH) {
			//	Out of bounds?
			break;
		}
		ray->colr = ctx->world[blockX + blockY * ctx->mapW];
	} while (i < 100 && ray->colr == 0);

}
