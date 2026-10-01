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
/** Height scaling of a ray */
#define GRPH_HEIGHT   	0.6f
/** Number of rays/columns to draw */
#define GRPH_COLUMNS  	240
/** Field-of-view multiplier (= tan(FOV/2)) */
#define GRPH_FOVMULT  	1.0f
/** Display-column vector increment */
#define GRPH_COLSTEP  	(GRPH_FOVMULT / GRPH_COLUMNS)


/* * * Type Declarations * * */

/** Named indices of GrphContext_t.controls[] */
enum GRPHCTRL_e {
	GRPHCTRL_FWD,
	GRPHCTRL_REV,
	GRPHCTRL_TURNLEFT,
	GRPHCTRL_TURNRIGHT,
	GRPHCTRL_STRAFELEFT,
	GRPHCTRL_STRAFERIGHT,
	GRPHCTRL_MAXKEY = GRPHCTRL_STRAFERIGHT
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
	int sectorSide; /**< If backside sector specified, portal to this Side of that Sector; otherwise ignored */
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
	int id;         	/**< Sector ID; index into map file's object allocation table, or -1 if invalid/unallocated */
	int sides;       	/**< Size of flexible member (number of elements) */
	PaletteId pal;  	/**< Room palette (as ID)     */
	TextureId ceil; 	/**< Ceiling and... */
	TextureId floor;	/**< ...floor texture (as ID) */
	Vertex start;   	/**< Starting vertex (for quick access; same as last element of sides[]) */
	Side side[8];    	/**< List of sides (fixed length for now) */
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

/** Fields of rayCast (pseudo)class */
typedef struct GraphicsContext_s {
	float camX; 	/**< (in) Camera x coordinate    */
	float camY; 	/**< (in) Camera y coordinate    */
	float camA; 	/**< (in) Camera angle (radians) */
	int mapW;   	/**< (in) Column count of map array */
	int mapH;   	/**< (in) Row count of map array    */
	int* world; 	/**< (in) Pointer to map array; accessed as [x + y * mapH] */
	Sector_t* sectors;
	int currentSectorId;
	bool controls[GRPHCTRL_MAXKEY + 1];	/**< (in) Control inputs */
	float columns[GRPH_COLUMNS]; 	/**< (out) Column heights to display       */
	int colColors[GRPH_COLUMNS]; 	/**< (out) Column color indices to display */
} GrphContext_t;

/** Container for rayCast() parameters */
typedef struct GrphRay_s {
	int sector;  		/**<  (in) Index of sector to cast in */
	float posX;     	/**<  (in) Origin x */
	float posY;     	/**<  (in) Origin y */
	float slopeX;   	/**<  (in) direction vector x */
	float slopeY;   	/**<  (in) direction vector y */
	float travel;   	/**< (out) Distance traveled along ray to nearest line (multiple of slope vector) */
	float frac;     	/**< (out) Distance along struck line (0..1) */
	int colr;       	/**< (out) Index of struck linedef */
	bool wallHoriz; 	/**< (out) true: struck top/bottom of map block */
} GrphRay_t;


/* * * Shared Globals * * */

extern Sector_t sectors[];

int doGameTic(GrphContext_t* ctx);
bool collisionDetect(GrphContext_t* ctx, float moveX, float moveY);
bool intersect(Intersect_t* intr);
void render(GrphContext_t* ctx);
void rayCast(GrphContext_t* ctx, GrphRay_t* ray);

#endif // SECTORS_H_INCLUDED
