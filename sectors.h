#ifndef SECTORS_H_INCLUDED
#define SECTORS_H_INCLUDED

#include <stdbool.h>
#include <stdint.h>

/* * * Configuration Defines * * */

/** Vertical/column multiplier; controls aspect ratio with respect to GRPH_FOVMULT */
#define GRPH_HEIGHT   	1.0f
/** Number of rays/columns to draw */
#define GRPH_COLUMNS  	240
/** Number of vertical pixels to scale to */
#define GRPH_ROWS     	640
#define GRPH_HALFROWS 	(GRPH_ROWS / 2)
/** Field-of-view multiplier (= tan(FOV/2)) */
#define GRPH_FOVMULT  	1.0f
/** Display-column vector increment */
#define GRPH_COLSTEP  	(2.0f * GRPH_FOVMULT / GRPH_COLUMNS)
/** Maximum number of sectors to portal through */
#define GRPH_MAX_RECURSION	20
/** Extra columns in GrphContext_t.cols for recursive chaining */
#define GRPH_LISTBUF   	(GRPH_COLUMNS * 7)
/** Threshold to decide whether a point is in front of the view plane (isPlane and isFront checks) */
#define GRPH_PLANECOMPARE	0

/* * * Type Declarations * * */

/** Controls.buttons bit fields */
enum GAMECTRL_e {
	GAMECTRL_FWD         = 0x0001,
	GAMECTRL_REV         = 0x0002,
	GAMECTRL_TURNLEFT    = 0x0004,
	GAMECTRL_TURNRIGHT   = 0x0008,
	GAMECTRL_STRAFELEFT  = 0x0010,
	GAMECTRL_STRAFERIGHT = 0x0020,
	GAMECTRL_FLYUP       = 0x0040,
	GAMECTRL_FLYDOWN     = 0x0080,
	GAMECTRL_SPEED       = 0x0100,
	GAMECTRL_SLIDE       = 0x0200
};

typedef int TextureId;

typedef int PaletteId;

typedef struct AxisConditioning_s {
	int offset;
	int deadband;
	int sensitivity;
} AxisCal;

typedef struct Vec2_s {
	float x;
	float y;
} Vec2;

typedef struct Vec3_s {
	float x;
	float y;
	float z;
} Vec3;

/** Position of an entity in 3D space */
typedef struct EntityPos_s {
	float x;  	/**< x coordinate */
	float y;  	/**< y coordinate */
	float z;  	/**< z coordinate */
	float a;  	/**< Facing angle (0 ... 2 * PI radians) */
	int secId;	/**< Current sector ID */
	float radius;	/**< Entity radius (x-y plane) */
	float height;	/**< Entity height (from z to z + height) */
} EntityPos;

typedef struct Side_s {
	int secId; 	/**< Backside sector, or -1 if single-sided    */
	int wallId;	/**< Front-side wall descriptor (as ID)        */
	Vec2 to;   	/**< Side is drawn from previous point to here */
} Side;

typedef struct __attribute__((aligned(4))) SectorFileHeader_s {
	uint32_t magic;	/**< Magic number "SCTR" */
	union ver {
		struct {
			char build;
			char minor;
			char major;
			char padding;
		};
		uint32_t u32;	/**< Version number (packed) */
	};
	uint32_t ids;	/**< Number of IDs in following table */
	uint32_t free;	/**< File offset of first freed block (or EOF or NULL) (in memory: pointer, or offset from root allocation, to same, or NULL if no free locations) */
	uint32_t idList[];	/**< Table of file offsets to Sectors (in memory: pointers, or offset from root allocation); zero/NULL values are empty/deleted/unused */
} SectorFileHeader;

/** Sector definition */
typedef struct Sector_s {
	int id;           	/**< Sector ID; index into world file's object allocation table, or -1 if invalid/unallocated */
	int sides;        	/**< Size of flexible member (number of elements) */
	PaletteId pal;    	/**< Room palette (as ID) */
	float ceilHeight; 	/**< Ceiling height */
	float floorHeight;	/**< Floor height   */
	TextureId ceilId; 	/**< Ceiling texture (as ID) */
	TextureId floorId;	/**< Floor texture (as ID)   */
	Vec2 start;     	/**< Starting vertex (for quick access; same as last element of sides[]) */
	Side side[8];     	/**< List of sides (static length for now) */
} Sector;

/** Container for render() queue entries */
typedef struct RenderQueue_s {
	Sector* sector;
	int colStart;
	int colEnd;
} RenderQueue;

/** Data container for intersect() */
typedef struct Intersect_s {
	Vec2 start; 	/**< (in) Start point of line to test */
	Vec2 end;   	/**< (in) End point of line to test   */
	Vec2 orig;  	/**< (in) Ray origin */
	Vec2 dir;   	/**< (in) Ray direction/slope (or displacement to end point of line being tested with) */
	float frac; 	/**< (out) Fraction along the line at which the ray intersects (&lt; 0, &gt;= 1: missed line segment)  */
	float side; 	/**< (out) Closest distance from line to origin (&gt; 0: on front side of line; == 0: exactly on line) */
	float dist; 	/**< (out) Distance from ray origin to intersection (as multiples of dir) */
} Intersect;

/** Data container for drawQuads() */
typedef struct DrawDescriptor_s {
	float startDist;	/**< Perspective-corrected map distance to start (left) vertex */
	float endDist;  	/**< Perspective-corrected map distance to end (right) vertex  */
	int spanStart;  	/**< Column to start drawing on (spanStart &lt;= x &lt; spanEnd) */
	int spanEnd;    	/**< Column to stop drawing at (spanStart &lt;= x &lt; spanEnd) */
	TextureId upper;	/**< Ceiling / upper wall texture (as ID) */
	TextureId lower;	/**< Floor / lower wall texture (as ID) */
	Sector* sec;    	/**< Sector to read ceil/floor/wall parameters from */
	int insert[GRPH_COLUMNS]; 	/**< Indices in ctx-&gt;cols[] of insertion point in this column's linked list */
	int ceilTos[GRPH_COLUMNS];	/**< Previous value of ceilTo for this column (since we can't (easily) go back one in the ctx-&gt;cols[] linked list) */
} DrawDescriptor;

/** Graphical data in a single display column */
typedef struct DrawCol_s {
	int to;      	/**< Screen coordinate to draw to (starting from upper screen edge or end of proceeding element) */
	TextureId id;	/**< Solid color or ceiling/wall texture */
//	int u;       	/**< Texture x-coordinate (if id references a texture, else unused) */
//	int v;       	/**< Texture initial y-coordinate (if id references a texture, else unused) */
//	int vSlope;  	/**< Texture y-scale (if id references a texture, else unused) */
	int next;    	/**< Index to next DrawCol (linked list) */
} DrawCol;

/** Fields of Sectors (pseudo)class */
typedef struct GraphicsContext_s {
	EntityPos cam;	/**< (in) Camera location */
	int clearId;	/**< (in) Background color (as ID) to clear screen with */
	int maxColsLen; /**< (out) Optional debug stat, records runtime maximum colsLen */
	int colsLen;	/**< (out) Used length of, or next free slot index in, cols[] */
	DrawCol cols[GRPH_COLUMNS + GRPH_LISTBUF];	/**< (out) Display column linked lists; indices 0 to GRPH_COLUMNS - 1 are head of list for these columns */
} GrphContext;

/** Fields of World (pseudo)class */
typedef struct GameWorld_s {
	int sectorsLen;   	/**< (in) Sector list length (indices 0 to sectorsLen - 1) */
	Sector* sectors;	/**< (in) Pointer to sector list */
} GameWorld;

/** Control inputs */
typedef struct Controls_s {
	uint32_t buttons;	/**< Bit vector, current button state (from GAMECTRL_e) */
	int xAxis;	/**< Horizontal (turn/slide) axis */
	int yAxis;	/**< Vertical (move) axis */
} Controls;

/** Fields of Gamestate (pseudo)class */
typedef struct GameContext_s {
	AxisCal calX; 	/**< Control X axis calibration */
	AxisCal calY; 	/**< Control Y axis calibration */
	Controls ctrl;	/**< Control inputs */
	EntityPos player;	/**< Player position  */
	GrphContext grph;	/**< Graphics context */
	GameWorld wrl;   	/**< World data */
} GameContext;


/* * * Shared Globals * * */

extern Sector sectors[];

void render(GameContext* ctx);
bool intersect(Intersect* intr);
bool drawQuads(GameContext* ctx, DrawDescriptor* dd);

#endif // SECTORS_H_INCLUDED
