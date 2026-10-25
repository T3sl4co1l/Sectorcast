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
#include "common.h"
#include "sectors.h"

//#define ARITHMETIC_DEBUG


Sector sectors[] = {
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
				secId: -1,
				wallId: 3,
				to: { -2.0f, 1.5f }
			}, {
				secId: -1,
				wallId: 1,
				to: { 1.0f, 1.5f }
			}, {
				secId: 1,
				wallId: 3,
				to: { 1.0f, -1.5f }
			}, {
				secId: -1,
				wallId: 2,
				to: { -2.0f, -1.3f }
			}
		}
	}, {
		id: 1,
		sides: 4,
		pal: 0,
		ceilHeight: 0.2f,
		floorHeight: -0.8f,
		ceilId: 14,
		floorId: 7,
		start: { 1.0f, 1.5f },
		side: {
			{
				secId: -1,
				wallId: 10,
				to: { 4.0f, 3.0f }
			}, {
				secId: -1,
				wallId: 11,
				to: { 4.0f, -0.75f }
			}, {
				secId: -1,
				wallId: 12,
				to: { 1.0f, -1.5f }
			}, {
				secId: 0,
				wallId: 13,
				to: { 1.0f, 1.5f }
			}
		}
	}
};

/**
 *	Transforms current graphics context (wrl, grph.cam...)
 *	into column colors and heights (cols[]).
 */
void render(GameContext* ctx) {
	bool isFront, lastPlane, thisPlane;
	int i, j, sides, queueStart = 1, queueEnd = 0;
	float angsin, angcos;
	float lastCol, thisCol;
	float lastDist, thisDist;
	Vec2 d1, col0, delta;
	RenderQueue queue[GRPH_MAX_RECURSION], *dequeue;
	Intersect intr;
	DrawDescriptor dd;

	if (ctx == NULL)
		return;

	//	Clear framebuffer
	for (i = 0; i < GRPH_COLUMNS; i++) {
		//	insert[] indexes the next-last ceiling/upper column element;
		//	create a loop in-place, which will be overwritten by the
		//	first real ceil/upper
		dd.insert[i] = i;
		dd.ceilTos[i] = 0;
		ctx->grph.cols[i].id = ctx->grph.clearId;
		ctx->grph.cols[i].next = 0;
		ctx->grph.cols[i].to = GRPH_ROWS;
	}
	ctx->grph.colsLen = GRPH_COLUMNS;

	//	we can't draw anything more if we don't have a world to draw... but we can clear screen I guess?
	if (ctx->wrl.sectors == NULL)
		return;

	//	View plane is defined by vector; rotate to view angle (angle
	//	is clockwise from +x axis; and +y axis is 90° left of +x)
	angsin = sinf(ctx->grph.cam.a); angcos = cosf(ctx->grph.cam.a);
	intr.orig.x = ctx->grph.cam.x; intr.orig.y = ctx->grph.cam.y;
	intr.dist = 1; intr.frac = 0;
	//	Each column's ray vector is related to the previous by a linear change:
	//	col0 is the left edge of screen; its conjugate is the right; and delta
	//	is the vector change per column.
	col0.x = angcos - angsin * GRPH_FOVMULT; col0.y = angsin + angcos * GRPH_FOVMULT;
	delta.x = angsin *  GRPH_COLSTEP; delta.y = angcos * -GRPH_COLSTEP;
#ifdef ARITHMETIC_DEBUG
	printf("col0.x: %e, col0.y: %e\ndelta.x: %e, delta.y: %e\n", col0.x, col0.y, delta.x, delta.y);
#endif // ARITHMETIC_DEBUG

	//	Start with current sector...
	queue[0].sector = &ctx->wrl.sectors[ctx->grph.cam.secId];
	queue[0].colStart = 0;
	queue[0].colEnd = GRPH_COLUMNS;
	/*
	 *	- Dequeue a sector.  For each side in the sector:
	 *	- If start or end vertex is in front of the view plane, and the
	 *	  side's front is facing the camera,
	 *		- Calculate display column of, and map distance to, vertices
	 *		  that passed the view plane check (memoize for next side
	 *		  test).
	 *		- Skip if both columns < colStarts or >= colEnds.
	 *		- If only one vertex passes this check, the side must span
	 *		  over one side of the screen.  Calculate line intersection
	 *		  with left display column vector (left vertex failed view
	 *		  plane check or column < 0) or right (right vertex failed
	 *		  view plane check or column >= GRPH_COLUMNS).  Use
	 *		  intersection.dist to calculate that column's floor/ceiling
	 *		  .to's.
	 *		- Vertices that pass and are on screen: calculate dist, then
	 *		  floor/ceiling .to's.
	 *		- Sides where both vertices pass, but colStart/End spans the
	 *		  display, need to be cut once or twice; apply the same
	 *		  processes as above.
	 *		- Interpolate .to's between endpoints.  This solves flat
	 *		  perspective without spending a raycast per column.
	 *		Output formatting:
	 *		- The data structure is a linked list that grows from the
	 *		  middle.  dd.insert[] tracks the insertion point, and
	 *		  dd.ceilTos[] the previous ceiling/upper .to's.
	 *		  (Previous lower is available on cols[insert[]].to, but the
	 *		  singly-linked list prevents easy access to the upper, so
	 *		  we need an array to record that.)
	 *		  Upper/lower .to's are used to eliminate invisible spans
	 *		  and solve occlusion front-to-back.  (This would also act
	 *		  as a crude depth buffer, if a wall/flat flag were stored.)
	 *		  Top of screen starts at ctx->grph.cols[x], going downward;
	 *		  the remainder of the array is allocated sequentially for
	 *		  linked list elements.
	 *		- Add ceiling before, and floor after, insert[], to the draw
	 *		  columns list.
	 *		- If line is 1-sided, set insert[] to the mid wall,
	 *		  completing that column.
	 *		- If 2-sided, get backside sector heights, calculate .to's,
	 *		  and (as applicable) add upper wall before insert[] and
	 *		  lower after insert[].  Enqueue sector with start/end
	 *		  column span.
	 *	- Repeat until queue is empty.
	 */
	do {
		dequeue = &queue[queueEnd++];
		if (queueEnd >= numelem(queue)) queueEnd = 0;
#ifdef ARITHMETIC_DEBUG
		printf("sector %i:\n", dequeue->sector->id);
#endif // ARITHMETIC_DEBUG
		//	prime the loop's sliding window...
		intr.end.x = dequeue->sector->start.x; intr.end.y = dequeue->sector->start.y;
		d1 = (Vec2){ intr.orig.x - intr.end.x, intr.orig.y - intr.end.y };
		thisPlane = (d1.x * delta.y - delta.x * d1.y) >= GRPH_PLANECOMPARE;
		thisCol = 0; thisDist = 0;
		if (thisPlane) {
			thisCol = (d1.x * col0.y - col0.x * d1.y) / (delta.x * d1.y - d1.x * delta.y);
			thisDist = (d1.x * delta.y - delta.x * d1.y) / (delta.x * col0.y - col0.x * delta.y);
		}
		sides = dequeue->sector->sides;
		for (i = 0; i < sides; i++) {
			//	advance to next side
			intr.start.x = intr.end.x; intr.start.y = intr.end.y;
			intr.end.x = dequeue->sector->side[i].to.x; intr.end.y = dequeue->sector->side[i].to.y;
			lastPlane = thisPlane; lastCol = thisCol; lastDist = thisDist;

			d1.x = intr.orig.x - intr.end.x; d1.y = intr.orig.y - intr.end.y;
			thisPlane = (d1.x * delta.y - delta.x * d1.y) >= GRPH_PLANECOMPARE;
			thisCol = 0; thisDist = 0;
			dd.startDist = 0; dd.endDist = 0;
#ifdef ARITHMETIC_DEBUG
			printf("    side %i: d1.x: %e, d1.y: %e", i, d1.x, d1.y);
#endif // ARITHMETIC_DEBUG

			//	At least one vertex is in front of the view plane; is the side visible?
			if (thisPlane || lastPlane) {
				//	is the side's front facing the camera? (backface culling)
				isFront = (d1.x * (intr.end.y - intr.start.y) - (intr.end.x - intr.start.x) * d1.y) >= GRPH_PLANECOMPARE;
				if (isFront) {

					//	Basic checks pass; transform to screen plane
					if (thisPlane) {
						thisCol = (d1.x * col0.y - col0.x * d1.y) / (delta.x * d1.y - d1.x * delta.y);
#ifdef ARITHMETIC_DEBUG
						printf(", thisCol: %e", thisCol);
#endif // ARITHMETIC_DEBUG
					}

					bool f;
					intr.frac = INFINITY;
#ifdef ARITHMETIC_DEBUG
					printf(", startPlane: %i, endPlane: %i", lastPlane, thisPlane);
#endif // ARITHMETIC_DEBUG
					if (thisPlane && !lastPlane && (int)ceilf(thisCol) >= dequeue->colStart) {
						//	start is behind view plane, end is in front, and end
						//	is right of start column: test left edge of portal
#ifdef ARITHMETIC_DEBUG
						printf(", left");
#endif // ARITHMETIC_DEBUG
						intr.dir.x = col0.x + delta.x * (float)dequeue->colStart;
						intr.dir.y = col0.y + delta.y * (float)dequeue->colStart;
						f = intersect(&intr);
						dd.spanStart = dequeue->colStart;
						dd.startDist = intr.dist;
						if ((int)ceilf(thisCol) > dequeue->colEnd) {
							//	end is right of end column; intersect right
#ifdef ARITHMETIC_DEBUG
							printf(", cut");
#endif // ARITHMETIC_DEBUG
							intr.dir.x = col0.x + delta.x * (float)dequeue->colEnd;
							intr.dir.y = col0.y + delta.y * (float)dequeue->colEnd;
							f = intersect(&intr);
							dd.spanEnd = dequeue->colEnd;
							dd.endDist = intr.dist;
						} else {
							dd.spanEnd = (int)ceilf(thisCol);
							thisDist = (d1.x * delta.y - delta.x * d1.y) / (delta.x * col0.y - col0.x * delta.y);
							dd.endDist = thisDist;
						}
					} else if (!thisPlane && lastPlane && (int)ceilf(lastCol) < dequeue->colEnd) {
						//	Start is in front of view plane, end is behind, and
						//	end is left of end column: test right edge of portal
#ifdef ARITHMETIC_DEBUG
						printf(", right");
#endif // ARITHMETIC_DEBUG
						intr.dir.x = col0.x + delta.x * (float)dequeue->colEnd;
						intr.dir.y = col0.y + delta.y * (float)dequeue->colEnd;
						f = intersect(&intr);
						dd.spanEnd = dequeue->colEnd;
						dd.endDist = intr.dist;
						if ((int)ceilf(lastCol) <= dequeue->colStart) {
							//	start is left of start column; intersect left
#ifdef ARITHMETIC_DEBUG
							printf(", cut");
#endif // ARITHMETIC_DEBUG
							intr.dir.x = col0.x + delta.x * (float)dequeue->colStart;
							intr.dir.y = col0.y + delta.y * (float)dequeue->colStart;
							f = intersect(&intr);
							dd.spanStart = dequeue->colStart;
							dd.startDist = intr.dist;
						} else {
							dd.spanStart = (int)ceilf(lastCol);
							dd.startDist = lastDist;
						}
					} else if (thisPlane && lastPlane && (int)ceilf(thisCol) >= dequeue->colStart
							&& (int)ceilf(lastCol) < dequeue->colEnd) {
						//	both in front, spanning some or all of the portal
#ifdef ARITHMETIC_DEBUG
						printf(", mid");
#endif // ARITHMETIC_DEBUG
						if ((int)ceilf(lastCol) <= dequeue->colStart) {
							//	start is left of start column; intersect left
#ifdef ARITHMETIC_DEBUG
							printf(", cut left");
#endif // ARITHMETIC_DEBUG
							intr.dir.x = col0.x + delta.x * (float)dequeue->colStart;
							intr.dir.y = col0.y + delta.y * (float)dequeue->colStart;
							f = intersect(&intr);
							dd.spanStart = dequeue->colStart;
							dd.startDist = intr.dist;
						} else {
							dd.spanStart = (int)ceilf(lastCol);
							dd.startDist = lastDist;
						}
						if ((int)ceilf(thisCol) > dequeue->colEnd) {
							//	end is right of end column; intersect right
#ifdef ARITHMETIC_DEBUG
							printf(", cut right");
#endif // ARITHMETIC_DEBUG
							intr.dir.x = col0.x + delta.x * (float)dequeue->colEnd;
							intr.dir.y = col0.y + delta.y * (float)dequeue->colEnd;
							f = intersect(&intr);
							dd.spanEnd = dequeue->colEnd;
							dd.endDist = intr.dist;
						} else {
							dd.spanEnd = (int)ceilf(thisCol);
							thisDist = (d1.x * delta.y - delta.x * d1.y) / (delta.x * col0.y - col0.x * delta.y);
							dd.endDist = thisDist;
						}
					}
					(void)f;

					if (intr.frac != INFINITY) {
#ifdef ARITHMETIC_DEBUG
						printf(", frac: %e", intr.frac);
#endif // ARITHMETIC_DEBUG
					}

					//	Side is in view?
					if (dd.startDist > 0) {

#ifdef ARITHMETIC_DEBUG
						printf(", colStart: %i, colEnd: %i, startDist: %e, endDist: %e", dd.spanStart, dd.spanEnd, dd.startDist, dd.endDist);
#endif // ARITHMETIC_DEBUG
						//	draw its ceil/floors...
						dd.upper = dequeue->sector->ceilId;
						dd.lower = dequeue->sector->floorId;
						dd.sec = dequeue->sector;
						drawQuads(ctx, &dd);

						//	...and walls
						if (dequeue->sector->side[i].secId == -1) {
							//	Single-sided: draw mid wall
							for (j = dd.spanStart; j < dd.spanEnd; j++) {
								ctx->grph.cols[dd.insert[j]].id = dequeue->sector->side[i].wallId;
							}
						} else {
							//	Two-sided: draw upper/lower wall as needed, then enqueue the sector
							int s = dequeue->sector->side[i].secId;
							if (s >= 0 && s < ctx->wrl.sectorsLen) {
								Sector* farSec = &ctx->wrl.sectors[s];
								dd.upper = dequeue->sector->side[i].wallId;
								dd.lower = dequeue->sector->side[i].wallId;
								dd.sec = farSec;
								drawQuads(ctx, &dd);
								queue[queueStart].sector = farSec;
								queue[queueStart].colStart = dd.spanStart;
								queue[queueStart++].colEnd = dd.spanEnd;
								if (queueStart >= numelem(queue)) queueStart = 0;
							}
						}

					} else {
#ifdef ARITHMETIC_DEBUG
						printf(", not visible, startDist: %e", dd.startDist);
#endif // ARITHMETIC_DEBUG
					}

				}
			}
#ifdef ARITHMETIC_DEBUG
			printf("\n");
#endif // ARITHMETIC_DEBUG

		}
	} while (queueStart != queueEnd);

	if (ctx->grph.colsLen > ctx->grph.maxColsLen) ctx->grph.maxColsLen = ctx->grph.colsLen;

}

/**
 *	Calculates useful parameters of the intersection of two
 *	lines in Cartesian coordinates.
 *	@param intr input geometry and return values.
 *	@return true on success; false if divide-by-zero (slopes
 *	are equal; intr->side sign is still valid) or intr == NULL.
 */
bool intersect(Intersect* intr) {

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

/**
 *	Draws trapezoids to the draw buffer (ctx->cols[]), with
 *	perspective transformation, and accounting for occlusion
 *	by graphics "in front of" the described objects.
 *	@param ctx input geometry and return values.
 *	dd  (in) geometry
 *	@return true on success; false if divide-by-zero (slopes
 *	are equal; intr->side sign is still valid) or intr == NULL.
 */
bool drawQuads(GameContext* ctx, DrawDescriptor* dd) {
	int j;

	if (ctx == NULL || dd == NULL || dd->sec == NULL)
		return false;

	//	convert dist's to row positions, then interpolate across columns
	float ceilStart  = (float)GRPH_ROWS * (float)GRPH_HEIGHT / GRPH_FOVMULT * (0.5f - (dd->sec->ceilHeight  - ctx->grph.cam.z) / dd->startDist);
	float ceilEnd    = (float)GRPH_ROWS * (float)GRPH_HEIGHT / GRPH_FOVMULT * (0.5f - (dd->sec->ceilHeight  - ctx->grph.cam.z) / dd->endDist  );
	float floorStart = (float)GRPH_ROWS * (float)GRPH_HEIGHT / GRPH_FOVMULT * (0.5f - (dd->sec->floorHeight - ctx->grph.cam.z) / dd->startDist);
	float floorEnd   = (float)GRPH_ROWS * (float)GRPH_HEIGHT / GRPH_FOVMULT * (0.5f - (dd->sec->floorHeight - ctx->grph.cam.z) / dd->endDist  );
	//	should be redundant by logic, might improve compiler inference?
	if (dd->spanStart < 0) dd->spanStart = 0;
	if (dd->spanStart > GRPH_COLUMNS - 1) dd->spanStart = GRPH_COLUMNS - 1;
	if (dd->spanEnd <= dd->spanStart) return true;
	if (dd->spanEnd > GRPH_COLUMNS) dd->spanEnd = GRPH_COLUMNS;
	int span = dd->spanEnd - dd->spanStart;
	float ceilSlope  = (ceilEnd  - ceilStart ) / (float)span;
	float floorSlope = (floorEnd - floorStart) / (float)span;
	DrawCol* pdcIns, *pdcNew;
	//	draw ceiling and floor
	for (j = 0; j < span; j++) {
		int ceilTo = (int)roundf(ceilStart + ceilSlope * ((float)j + 0.5f));
		pdcIns = &ctx->grph.cols[dd->insert[j + dd->spanStart]];
		if (ctx->grph.colsLen < numelem(ctx->grph.cols) && ceilTo > dd->ceilTos[j + dd->spanStart]) {
			//	Ceilings and upper walls get stacked behind insert[]
			pdcNew = &ctx->grph.cols[ctx->grph.colsLen];	//	make new element, copy from current
			pdcNew->to = pdcIns->to;
			pdcNew->id = pdcIns->id;
			pdcNew->next = pdcIns->next;
			//	overwrite current, then advance insert[] past it
			pdcIns->to = ceilTo;
			pdcIns->id = dd->upper;
			pdcIns->next = ctx->grph.colsLen;
			dd->insert[j + dd->spanStart] = ctx->grph.colsLen;
			dd->ceilTos[j + dd->spanStart] = ceilTo;
			ctx->grph.colsLen++;
		}
		int floorTo = (int)roundf(floorStart + floorSlope * ((float)j + 0.5f));
		pdcIns = &ctx->grph.cols[dd->insert[j + dd->spanStart]];
		if (ctx->grph.colsLen < numelem(ctx->grph.cols) && floorTo < pdcIns->to) {
			//	Floors and lowers are pushed ahead of insert[],
			//	so they draw in opposite order to ceils/uppers
			pdcNew = &ctx->grph.cols[ctx->grph.colsLen];	//	make new element, copy from current
			pdcNew->to = pdcIns->to;
			pdcNew->id = dd->lower;
			pdcNew->next = pdcIns->next;
			//	overwrite current, except keeping its id (which will remain clearId until mid wall overwrites it)
			pdcIns->to = floorTo;
			pdcIns->next = ctx->grph.colsLen;
			ctx->grph.colsLen++;
		}
	}

	return true;
}
