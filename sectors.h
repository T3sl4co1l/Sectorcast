#ifndef SECTORS_H_INCLUDED
#define SECTORS_H_INCLUDED

#include <stdbool.h>
#include <stdint.h>


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

typedef struct Vertex_s {
	float x;
	float y;
} Vertex;

typedef struct Side_s {
	int sectorId;	/**< Backside sector, or -1 if single-sided    */
//	int sectorSide; /**< If backside sector specified, portal to this Side of that Sector; otherwise ignored */
	int wallId;  	/**< Front-side wall descriptor (as ID)        */
	Vertex to;   	/**< Side is drawn from previous point to here */
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
	float ceilHeight; 	/**< Ceiling and...  */
	float floorHeight;	/**< ...floor height */
	TextureId ceilId; 	/**< Ceiling and...  */
	TextureId floorId;	/**< ...floor texture (as ID) */
	Vertex start;     	/**< Starting vertex (for quick access; same as last element of sides[]) */
	Side side[8];     	/**< List of sides (fixed length for now) */
} Sector_t;

/** Data container for intersect() */
typedef struct Intersect_s {
	Vertex lineStart;	/**< (in) Start point of line to test */
	Vertex lineEnd;  	/**< (in) End point of line to test */
	Vertex orig;     	/**< (in) Ray origin */
	Vertex dir;      	/**< (in) Ray direction (or end point of line being tested against) */
	float frac;      	/**< (out) Fraction along the line at which the ray intersects (< 0, >= 1: missed line segment) */
	float side;      	/**< (out) Closest distance from line to origin (> 0: on front side of line; == 0: exactly on line) */
	float dist;      	/**< (out) Distance from ray origin to intersection (multiples of dir) */
} Intersect_t;

/** Graphical data in a single display column */
typedef struct DrawCol_s {
	int top;	/**< Upper wall (ceiling edge) screen coordinate */
	int bot;	/**< Lower wall (floor edge) screen coordinate   */
	TextureId ceilId; 	/**< Ceiling and...  */
	TextureId floorId;	/**< ...floor texture (as ID) */
	TextureId wallId;	/**< Wall texture (as ID); mid if next == 0, else upper and lower */
	int frac;	/**< line intersection (u coordinate) */
	int next;	/**< Index to next DrawCol (linked list) (draws upper and lower), or 0 (was a single-sided wall) */
} DrawCol_t;

/** Fields of Sectors (pseudo)class */
typedef struct GraphicsContext_s {
	float camA; 	/**< (in) Camera angle (radians) */
	float camX; 	/**< (in) Camera x coordinate    */
	float camY; 	/**< (in) Camera y coordinate    */
	float camZ; 	/**< (in) Camera z coordinate    */
	Sector_t* sectors;  	/**< (in) Pointer to sector list */
	int sectorMax;      	/**< (in) Sector list length (indices 0 to sectorMax-1) */
	int currentSectorId;	/**< (in/out) Sector the camera is in */
	bool controls[GRPHCTRL_MAXKEY + 1];	/**< (in) Control inputs  */
	DrawCol_t cols[GRPH_COLUMNS + GRPH_RECBUF];	/**< (out) Display columns */
} GrphContext_t;


/* * * Shared Globals * * */

extern Sector_t sectors[];

int doGameTic(GrphContext_t* ctx);
bool collisionDetect(GrphContext_t* ctx, float moveX, float moveY);
bool intersect(Intersect_t* intr);
void render(GrphContext_t* ctx);

#endif // SECTORS_H_INCLUDED
