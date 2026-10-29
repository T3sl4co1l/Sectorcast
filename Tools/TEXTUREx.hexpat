import type.size;

struct MapPatch {
    u16 originx;
    u16 originy;
    u16 patch;
    u16 stepdir;
    u16 colormap;
};

struct MapTexture {
    u8       name[8];
    u32      masked;   
    u16      width;
    u16      height;
    u32      obsolete;
    u16      patchCount;
    MapPatch patches[patchCount];
};

struct Texture {
    u32 numTextures;
    MapTexture *filePos[numTextures]: u32;
};
 
Texture tex @ 0x0;