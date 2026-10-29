#pragma author WerWolv
#pragma description DOOM WAD Archive
#pragma magic [ 50 57 41 44 ] @ 0x00

import type.magic;
import type.size;

enum WADType : char {
    Internal = 'I',
    Patch = 'P'
};

struct mapvertex_t {
    u16 x;
    u16 y;
};

struct mapsidedef_t {
    u16 textureoffset;
    u16 rowoffset;
    char toptexture[8];
    char bottomtexture[8];
    char midtexture[8];
    u16 sector;
};

struct maplinedef_t {
  u16 v1;
  u16 v2;
  u16 flags;
  u16 special;
  u16 tag;
  u16 sidefront;
  u16 sideback;
};

struct mapsector_t {
    u16 floorheight;
    u16 ceilingheight;
    char floorpic[8];
    char ceilingpic[8];
    u16 lightlevel;
    u16 special;
    u16 tag;
};

struct mapthing_t {
    u16 x;
    u16 y;
    u16 angle;
    u16 type;
    u16 options;
};

struct FileLump {
    u32 filePos;
    type::Size<u32> size;
    char name[8];
    u8 data[size] @ filePos [[sealed]];
    if (name == "SECTORS\x00") {
        mapsector_t sectors[size / sizeof(mapsector_t)] @ filePos;
    }
    if (name == "SIDEDEFS") {
        mapsidedef_t sidedefs[size / sizeof(mapsidedef_t)] @ filePos;
    }
    if (name == "LINEDEFS") {
        maplinedef_t linedefs[size / sizeof(maplinedef_t)] @ filePos;
    }
    if (name == "VERTEXES") {
        mapvertex_t vertices[size / sizeof(mapvertex_t)] @ filePos;
    }
    if (name == "THINGS\x00\x00") {
        mapthing_t things[size / sizeof(mapthing_t)] @ filePos;
    }
};

struct WAD {
    WADType type;
    type::Magic<"WAD"> identification;
    u32 numLumps;
    FileLump *infoTable[numLumps] : u32;
};

WAD wad @ 0x00;