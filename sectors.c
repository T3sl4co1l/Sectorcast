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
 *	- Sector: a starting vertex and list of Sides.  A side is a
 *	vertex to which a line is drawn implicitly, from the previous
 *	side (or sector.start), to the side's vertex, and so on, forming
 *	a closed convex polygon on the map, and in 3D, a prism with
 *	floor and ceiling.  (The final side has a vertex, only as an
 *	optimization / for consistency.)
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
#include <string.h>
#include <math.h>
#include "sectors.h"

Sector_t sectors[] = {
	{
		id: 0,
		sides: 4,
		pal: 0,
		ceilHeight: 0.3f,
		floorHeight: -0.6f,
		ceilId: 12,
		floorId: 5,
		start: { -2.0f, -1.3f },
		side: {
			{
				sectorId: -1,
				wallId: 3,
				to: { -2.0f, 1.5f }
			}, {
				sectorId: -1,
				wallId: 1,
				to: { 1.0f, 1.5f }
			}, {
				sectorId: 1,
				wallId: 0,
				to: { 1.0f, -1.5f }
			}, {
				sectorId: -1,
				wallId: 2,
				to: { -2.0f, -1.3f }
			}
		}
	}, {
		id: 1,
		sides: 4,
		pal: 0,
		ceilHeight: 0.6f,
		floorHeight: -0.6f,
		ceilId: 14,
		floorId: 5,
		start: { 1.0f, 1.5f },
		side: {
			{
				sectorId: -1,
				wallId: 4,
				to: { 2.0f, 0.75f }
			}, {
				sectorId: -1,
				wallId: 5,
				to: { 2.0f, -0.75f }
			}, {
				sectorId: -1,
				wallId: 6,
				to: { 1.0f, -1.5f }
			}, {
				sectorId: 0,
				wallId: 0,
				to: { 1.0f, 1.5f }
			}
		}
	}
};

/**
 *	Processes inputs and updates game state.  Call from
 *	main loop, synchronized to system tic / frame timer.
 *	@return GRPHTIC_e bitfield:<br>
 *	GRPHTIC_MOVED     movement occurred (screen contents likely changed, needing an update)<br>
 *	GRPHTIC_COLLISION movement truncated by wall (maybe play a sound)
 */
int doGameTic(GrphContext_t* ctx) {
	float moveX = 0.0f, moveY = 0.0f, moveZ = 0.0f;
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
		moveFlag = true;
	}
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
	if (ctx->controls[GRPHCTRL_FLYUP]) {
		moveZ += GRPH_MOVESTEP;
		moveFlag = true;
	}
	if (ctx->controls[GRPHCTRL_FLYDOWN]) {
		moveZ -= GRPH_MOVESTEP;
		moveFlag = true;
	}
	if (ctx->camA < 0) ctx->camA += 2 * M_PI;
	if (ctx->camA >= 2 * M_PI) ctx->camA -= 2 * M_PI;
	if (moveFlag) {
		//collisionFlag = collisionDetect(ctx, moveX, moveY);
		ctx->camX += moveX; ctx->camY += moveY; ctx->camZ += moveZ;
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
//	float checkX[4], checkY[4];
//	int pointNum[4];
//	float changedX, changedY;
//	int blockX, blockY;
//	int corners = 0;
//	int index;

	if (ctx == NULL || ctx->sectors == NULL)
		return false;

/*
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
*/

	return true;
}

typedef struct RenderStack_s {
	Sector_t* sector;
	int colStart;
	int colEnd;
} RenderStack_t;

/**
 * Transforms current information (world, camX, camY, camA..)
 * into column colors and heights (columns, colColors).
 */
void render(GrphContext_t* ctx) {
	float angsin, angcos;
	Vec2 col0, delta;
	int i, j, sides, stackTop = 0;
	/**	Indices in ctx->cols[] of insertion point in this column's linked list */
	int insert[GRPH_COLUMNS];
	/**	Previous value of ceilTo for this column; since we can't (easily) index back-one in the linked list, sadly */
	int ceilTos[GRPH_COLUMNS];
	Intersect_t intr;
	RenderStack_t stack[GRPH_MAX_RECURSION], *pop;
	bool isFront, lastPlane, thisPlane;
	float lastCol, thisCol;
	float lastDist, thisDist;
	float startDist, endDist;

	if (ctx == NULL || ctx->sectors == NULL)
		return;

	//	Clear framebuffer
	for (i = 0; i < GRPH_COLUMNS; i++) {
		//	insert[] indexes the next-last ceiling/upper column element;
		//	create a loop in-place, which will be overwritten by the
		//	first real ceil/upper
		insert[i] = i;
		ceilTos[i] = 0;
		ctx->cols[i].id = ctx->clearId;
		ctx->cols[i].next = 0;
		ctx->cols[i].to = GRPH_ROWS;
	}
	ctx->colsLen = GRPH_COLUMNS;

	//	View plane is defined by vector; rotate to view angle (note angle is clockwise from +x axis)
	angsin = sinf(ctx->camA); angcos = cosf(ctx->camA);
	intr.orig.x = ctx->camX; intr.orig.y = ctx->camY;
	intr.dist = 1; intr.frac = 0;
	//	Each column's ray vector is related to the previous by a linear change:
	//	col0 is the left edge of screen; its conjugate is the right; and delta
	//	is the vector change per column.
	col0.x = angcos - angsin * GRPH_FOVMULT; col0.y = angsin + angcos * GRPH_FOVMULT;
	delta.x = angsin *  GRPH_COLSTEP; delta.y = angcos * -GRPH_COLSTEP;
	printf("col0.x: %e, col0.y: %e\ndelta.x: %e, delta.y: %e\n", col0.x, col0.y, delta.x, delta.y);

	//	Start with current sector...
	//	TODO: refactor stack to queue
	stack[stackTop].sector = &ctx->sectors[ctx->currentSectorId];
	stack[stackTop].colStart = 0;
	stack[stackTop++].colEnd = GRPH_COLUMNS;
	//	- Dequeue a sector.  For each side in the sector:
	//	- If start or end vertex is in front of the view plane, and the
	//	  side's front is facing the camera,
	//		- Calculate display column of vertices that passed the view
	//		  plane check (memoize for next side test).
	//		- Skip if both columns < colStarts or >= colEnds.
	//		- If only one vertex passes this check, calculate line
	//		  intersection with left display column vector (left vertex
	//		  failed view plane check or column < 0) or right (right
	//		  vertex failed view plane check or column >= GRPH_COLUMNS).
	//		  Use intersection.dist to calculate that column's floor/
	//		  ceiling .to's.  This solves the left/right-most column.
	//		- Vertices that pass: calculate dist, then floor/ceiling
	//		  .to's. This solves the remaining end columns.
	//		- Interpolate .to's between endpoints.
	//		- If line is 1-sided, add mid wall after insert[].
	//		- If 2-sided, get backside sector heights, calculate .to's,
	//		  and (as applicable) add upper wall before insert[] and
	//		  lower after insert[].  Enqueue sector and start/end
	//		  column span.
	//	- Repeat until queue is empty.
	do {
		pop = &stack[--stackTop];
		sides = pop->sector->sides;
		printf("sector %i:\n", pop->sector->id);
		//	prime the loop's sliding window...
		intr.end.x = pop->sector->start.x; intr.end.y = pop->sector->start.y;
		Vec2 d1 = { intr.orig.x - intr.end.x, intr.orig.y - intr.end.y };
		thisPlane = (d1.x * delta.y - delta.x * d1.y) > 0;
		thisCol = 0; thisDist = 0;
		if (thisPlane) {
			thisCol = (d1.x * col0.y - col0.x * d1.y) / (delta.x * d1.y - d1.x * delta.y);
			thisDist = (d1.x * delta.y - delta.x * d1.y) / (delta.x * col0.y - col0.x * delta.y);
		}
		for (i = 0; i < sides; i++) {
			//	advance to next side
			intr.start.x = intr.end.x; intr.start.y = intr.end.y;
			intr.end.x = pop->sector->side[i].to.x; intr.end.y = pop->sector->side[i].to.y;
			lastPlane = thisPlane; lastCol = thisCol; lastDist = thisDist;

			d1.x = intr.orig.x - intr.end.x; d1.y = intr.orig.y - intr.end.y;
			thisPlane = (d1.x * delta.y - delta.x * d1.y) > 0;
			thisCol = 0; thisDist = 0;
			startDist = 0; endDist = 0;
			printf("    side %i: d1.x: %e, d1.y: %e", i, d1.x, d1.y);

			//	At least one vertex is in front of the view plane; is the side visible?
			if (thisPlane || lastPlane) {
				//	is the side's front facing the camera? (backface culling)
				isFront = (d1.x * (intr.end.y - intr.start.y) - (intr.end.x - intr.start.x) * d1.y) > 0;
				if (isFront) {
					int spanStart, spanEnd;

					//	Basic checks pass; transform to screen plane
					if (thisPlane) {
						thisCol = (d1.x * col0.y - col0.x * d1.y) / (delta.x * d1.y - d1.x * delta.y);
						printf(", thisCol: %e", thisCol);
					}

					bool f;
					intr.frac = INFINITY;
					printf(", startPlane: %i, endPlane: %i", lastPlane, thisPlane);
					if (thisPlane && !lastPlane && (int)ceilf(thisCol) >= pop->colStart) {
						//	start is behind view plane, end is in front, and end
						//	is right of start column: test left edge of portal
						printf(", left");
						intr.dir.x = col0.x + delta.x * (float)pop->colStart;
						intr.dir.y = col0.y + delta.y * (float)pop->colStart;
						f = intersect(&intr);
						spanStart = pop->colStart;
						startDist = intr.dist;
						if ((int)ceilf(thisCol) > pop->colEnd) {
							//	end is right of end column; intersect right
							printf(", cut");
							intr.dir.x = col0.x + delta.x * (float)pop->colEnd;
							intr.dir.y = col0.y + delta.y * (float)pop->colEnd;
							f = intersect(&intr);
							spanEnd = pop->colEnd;
							endDist = intr.dist;
						} else {
							spanEnd = (int)ceilf(thisCol);
							thisDist = (d1.x * delta.y - delta.x * d1.y) / (delta.x * col0.y - col0.x * delta.y);
							endDist = thisDist;
						}
					} else if (!thisPlane && lastPlane && (int)ceilf(lastCol) < pop->colEnd) {
						//	Start is in front of view plane, end is behind, and
						//	end is left of end column: test right edge of portal
						printf(", right");
						intr.dir.x = col0.x + delta.x * (float)pop->colEnd;
						intr.dir.y = col0.y + delta.y * (float)pop->colEnd;
						f = intersect(&intr);
						spanEnd = pop->colEnd;
						endDist = intr.dist;
						if ((int)ceilf(lastCol) <= pop->colStart) {
							//	start is left of start column; intersect left
							printf(", cut");
							intr.dir.x = col0.x + delta.x * (float)pop->colStart;
							intr.dir.y = col0.y + delta.y * (float)pop->colStart;
							f = intersect(&intr);
							spanStart = pop->colStart;
							startDist = intr.dist;
						} else {
							spanStart = (int)ceilf(lastCol);
							startDist = lastDist;
						}
					} else if (thisPlane && lastPlane && (int)ceilf(thisCol) >= pop->colStart
							&& (int)ceilf(lastCol) < pop->colEnd) {
						//	both in front, spanning some or all of the portal
						printf(", mid");
						if ((int)ceilf(lastCol) <= pop->colStart) {
							//	start is left of start column; intersect left
							printf(", cut left");
							intr.dir.x = col0.x + delta.x * (float)pop->colStart;
							intr.dir.y = col0.y + delta.y * (float)pop->colStart;
							f = intersect(&intr);
							spanStart = pop->colStart;
							startDist = intr.dist;
						} else {
							spanStart = (int)ceilf(lastCol);
							startDist = lastDist;
						}
						if ((int)ceilf(thisCol) > pop->colEnd) {
							//	end is right of end column; intersect right
							printf(", cut right");
							intr.dir.x = col0.x + delta.x * (float)pop->colEnd;
							intr.dir.y = col0.y + delta.y * (float)pop->colEnd;
							f = intersect(&intr);
							spanEnd = pop->colEnd;
							endDist = intr.dist;
						} else {
							spanEnd = (int)ceilf(thisCol);
							thisDist = (d1.x * delta.y - delta.x * d1.y) / (delta.x * col0.y - col0.x * delta.y);
							endDist = thisDist;
						}
					}
					(void)f;

					if (intr.frac != INFINITY)
						printf(", frac: %e", intr.frac);
					if (startDist > 0) {
						printf(", colStart: %i, colEnd: %i, startDist: %e, endDist: %e", spanStart, spanEnd, startDist, endDist);
						//	Side is in view; convert dist's to row positions,
						//	then interpolate across columns
						float ceilStart  = (float)GRPH_ROWS * (0.5f - (pop->sector->ceilHeight  - ctx->camZ) / startDist);
						float ceilEnd    = (float)GRPH_ROWS * (0.5f - (pop->sector->ceilHeight  - ctx->camZ) / endDist  );
						float floorStart = (float)GRPH_ROWS * (0.5f - (pop->sector->floorHeight - ctx->camZ) / startDist);
						float floorEnd   = (float)GRPH_ROWS * (0.5f - (pop->sector->floorHeight - ctx->camZ) / endDist  );
						//	should be redundant by logic, might improve compiler inference?
						if (spanStart < 0) spanStart = 0;
						if (spanStart > GRPH_COLUMNS - 1) spanStart = GRPH_COLUMNS - 1;
						if (spanEnd < 0) spanEnd = 0;
						if (spanEnd > GRPH_COLUMNS) spanEnd = GRPH_COLUMNS;
						int span = spanEnd - spanStart;
						float ceilSlope  = (ceilEnd  - ceilStart ) / (float)span;
						float floorSlope = (floorEnd - floorStart) / (float)span;
						DrawCol_t* pdcIns, *pdcNew;
						for (j = 0; j < span; j++) {
							int ceilTo = (int)roundf(ceilStart + ceilSlope * (float)j);
							pdcIns = &ctx->cols[insert[j + spanStart]];
							if (ctx->colsLen < numelem(ctx->cols) && ceilTo > ceilTos[j + spanStart]) {
								//	Ceilings and upper walls get stacked behind insert[]
								pdcNew = &ctx->cols[ctx->colsLen];	//	make new element, copy from current
								pdcNew->to = pdcIns->to;
								pdcNew->id = pdcIns->id;
								pdcNew->next = pdcIns->next;
								//	overwrite current, then advance insert[] past it
								pdcIns->to = ceilTo;
								pdcIns->id = pop->sector->ceilId;
								pdcIns->next = ctx->colsLen;
								insert[j + spanStart] = ctx->colsLen;
								ctx->colsLen++;
							}
							int floorTo = (int)roundf(floorStart + floorSlope * (float)j);
							pdcIns = &ctx->cols[insert[j + spanStart]];
							if (ctx->colsLen < numelem(ctx->cols) && floorTo < pdcIns->to) {
								//	Floors and lowers are pushed ahead of insert[],
								//	so they draw in opposite order to ceils/uppers
								pdcNew = &ctx->cols[ctx->colsLen];	//	make new element, copy from current
								pdcNew->to = pdcIns->to;
								pdcNew->id = pop->sector->floorId;
								pdcNew->next = pdcIns->next;
								//	overwrite current, except keeping its id (which will remain clearId until mid wall overwrites it)
								pdcIns->to = floorTo;
								pdcIns->next = ctx->colsLen;
								ctx->colsLen++;
							}
						}

					} else {
						printf(", not visible, startDist: %e", startDist);
					}

				}
			}
			printf("\n");

		}
	} while (stackTop > 0);

	if (ctx->colsLen > ctx->maxColsLen) ctx->maxColsLen = ctx->colsLen;

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

	Vec2 d1 = { intr->end.x - intr->start.x, intr->end.y - intr->start.y };
	Vec2 d2 = { intr->dir.x, intr->dir.y };
	Vec2 d3 = { intr->orig.x - intr->start.x, intr->orig.y - intr->start.y };
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
