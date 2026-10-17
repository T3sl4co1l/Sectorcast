#ifndef SECTORS_H_INCLUDED
#define SECTORS_H_INCLUDED

#include <stdint.h>
#include "common.h"


/* * * Configuration Defines * * */

/** Frame rate (1/gametic timer) */
#define GRPH_FRAMERATE	60
/** Angle (in radians) turned per gametic */
#define GRPH_ANGLESTEP	(M_PI / 52.0f)
/** Fraction of a grid square moved in one gametic */
#define GRPH_MOVESTEP 	0.08f
/** Square bounding box for the player's collision */
#define GRPH_PLAYERSZ 	0.4f
/** Draw column height scaling */
#define GRPH_HEIGHT   	0.6f
/** Number of rays/columns to draw */
#define GRPH_COLUMNS  	240
/** Number of vertical pixels to scale to */
#define GRPH_ROWS     	640
#define GRPH_HALFROWS 	(GRPH_ROWS / 2)
/** Field-of-view multiplier (= tan(FOV/2)) */
#define GRPH_FOVMULT  	1.0f
/** Display-column vector increment */
#define GRPH_COLSTEP  	(2.0f * GRPH_FOVMULT / GRPH_COLUMNS)
/** Maximum number of sectors to raycast through */
#define GRPH_MAX_RECURSION	20
/** Extra columns in GrphContext_t.cols for recursive chaining */
#define GRPH_RECBUF   	GRPH_COLUMNS


/* * * Type Declarations * * */

/** Named indices of GrphContext_t.controls[] */
enum GRPHCTRL_e {
	GRPHCTRL_FWD,
	GRPHCTRL_REV,
	GRPHCTRL_TURNLEFT,
	GRPHCTRL_TURNRIGHT,
	GRPHCTRL_STRAFELEFT,
	GRPHCTRL_STRAFERIGHT,
	GRPHCTRL_FLYUP,
	GRPHCTRL_FLYDOWN,
	GRPHCTRL_MAXKEY = GRPHCTRL_FLYDOWN
};

/** Flags returned from doGameTic() */
enum GRPHTIC_e {
	GRPHTIC_MOVED     = 0x0001,	/**< Movement occurred; redraw display  */
	GRPHTIC_COLLISION = 0x0002,	/**< Collision occurred during movement */
};

typedef int TextureId;

typedef int PaletteId;

typedef struct Vec2_s {
	float x;
	float y;
} Vec2;

typedef struct Side_s {
	int sectorId;	/**< Backside sector, or -1 if single-sided    */
	int wallId;  	/**< Front-side wall descriptor (as ID)        */
	Vec2 to;   	/**< Side is drawn from previous point to here */
} Side;

typedef struct __attribute__((aligned(4))) SectorFileHeader_s {
	uint32_t magic;	/**< Magic number "SCTR" */
	union version {
		struct {
			char build;
			char minor;
			char major;
			char padding;
		};
		uint32_t ver;	/**< Version number */
	};
	uint32_t ids;	/**< Number of IDs in following table */
	uint32_t free;	/**< File offset of first freed block (or EOF or NULL) (in memory: pointer, or offset from root allocation, to same, or NULL if no free locations) */
	uint32_t idList[];	/**< Table of file offsets to Sectors (in memory: pointers, or offset from root allocation); zero/NULL values are empty/deleted/unused */
} SectorFileHeader_t;

/** Sector definition */
typedef struct Sector_s {
	int id;           	/**< Sector ID; index into map file's object allocation table, or -1 if invalid/unallocated */
	int sides;        	/**< Size of flexible member (number of elements) */
	PaletteId pal;    	/**< Room palette (as ID) */
	float ceilHeight; 	/**< Ceiling height */
	float floorHeight;	/**< Floor height   */
	TextureId ceilId; 	/**< Ceiling texture (as ID) */
	TextureId floorId;	/**< Floor texture (as ID)   */
	Vec2 start;     	/**< Starting vertex (for quick access; same as last element of sides[]) */
	Side side[8];     	/**< List of sides (static length for now) */
} Sector_t;

/** Data container for intersect() */
typedef struct Intersect_s {
	Vec2 start; 	/**< (in) Start point of line to test */
	Vec2 end;   	/**< (in) End point of line to test   */
	Vec2 orig;  	/**< (in) Ray origin */
	Vec2 dir;   	/**< (in) Ray direction/slope (or displacement to end point of line being tested with) */
	float frac; 	/**< (out) Fraction along the line at which the ray intersects (< 0, >= 1: missed line segment) */
	float side; 	/**< (out) Closest distance from line to origin (> 0: on front side of line; == 0: exactly on line) */
	float dist; 	/**< (out) Distance from ray origin to intersection (as multiples of dir) */
} Intersect_t;

/** Graphical data in a single display column */
typedef struct DrawCol_s {
	int to;      	/**< Screen coordinate to draw to (starting from upper screen edge or end of proceeding element) */
	TextureId id;	/**< Solid color or ceiling/wall texture */
//	int u;       	/**< Texture x-coordinate (if id references a texture, else unused) */
//	int v;       	/**< Texture initial y-coordinate (if id references a texture, else unused) */
//	int vSlope;  	/**< Texture y-scale (if id references a texture, else unused) */
	int next;    	/**< Index to next DrawCol (linked list) */
} DrawCol_t;

/** Fields of Sectors (pseudo)class */
typedef struct GraphicsContext_s {
	bool controls[GRPHCTRL_MAXKEY + 1];	/**< (in) Control inputs  */
	float camA; 	/**< (in) Camera angle (radians) */
	float camX; 	/**< (in) Camera x coordinate    */
	float camY; 	/**< (in) Camera y coordinate    */
	float camZ; 	/**< (in) Camera z coordinate    */
	int clearId;	/**< (in) Background color (as ID) to clear screen with */
	Sector_t* sectors;  	/**< (in) Pointer to sector list */
	int sectorMax;      	/**< (in) Sector list length (indices 0 to sectorMax-1) */
	int currentSectorId;	/**< (in/out) Sector the camera is in */
	int colsLen;	/**< (out) Used length of, or next free slot index in, cols[] */
	int maxColsLen; /**< (out) Optional debug stat, records runtime maximum colsLen */
	DrawCol_t cols[2 * (GRPH_COLUMNS + GRPH_RECBUF)]; 	/**< (out) Display columns */
} GrphContext_t;


/* * * Shared Globals * * */

extern Sector_t sectors[];

int doGameTic(GrphContext_t* ctx);
bool collisionDetect(GrphContext_t* ctx, float moveX, float moveY);
bool intersect(Intersect_t* intr);
void render(GrphContext_t* ctx);

#endif // SECTORS_H_INCLUDED
