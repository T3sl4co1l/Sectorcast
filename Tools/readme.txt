Various tools supporting the project.

WAD tools for reading and writing Doom maps:

- `square.wad`: reference example, a simple 192-square room with player start.
- `map2wad.html` and `.js`: converter from Raycast `*.map` to Doom WAD.  Note output needs node building and textures (`SGFX.WAD`).
- `level.map`: example Raycast map.
- `level.map_doom.jpg`: it works!  Compare with: https://imgur.com/raycaster-graphics-engine-on-microcontroller-tdApTG1
- `*.hexpat`: ImHex Pattern Files including certain lumps.  `WAD_MAP.hexpat` is an expanded version of the preinstalled/library pattern, describing relevant map lumps.
- `SECTOR?.png`: patches for `SGFX.WAD`.
