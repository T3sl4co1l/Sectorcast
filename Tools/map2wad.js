var fileData, fileName;

document.addEventListener("DOMContentLoaded", function() {
	document.getElementById("load").addEventListener("click", loadFile);
	document.getElementById("save").addEventListener("click", saveFile);
});

/**
 *	Load button clicked
 */
function loadFile(e) {
	var inp = document.getElementById("in");
	if (inp.files.length) {
		fileName = inp.files[0].name;
		inp.files[0].text().then(s => fileData = s);
	}
}

/**
 *	Save button clicked
 */
function saveFile(e) {
	var i, x, y;
	const BLOCK_WIDTH = 96;
	const BLOCK_HEIGHT = 82;

	//	Parse map data
	if (fileData === undefined || typeof fileData !== "string" || fileData.length < 3)
		return;

	var lines = fileData.trimRight().split("\n");
	if (lines.length < 4) {
		console.log("Insufficient lines.")
		return;
	}
	//	Just in case, trim white space
	lines = lines.map(s => s.trim());
	for (y = 0; y < lines.length; y++)
		lines[y] = lines[y].split(" ");
	if (lines[0].length != 2) {
		console.log(((lines[0].length > 2) ? "Too many" : "Insufficient") + " tokens on line 1.")
		return;
	}
	var width = parseInt(lines[0][0]), height = parseInt(lines[0][1]);
	lines.shift();
	if (isNaN(width) || isNaN(height) || width < 3 || height < 3 || width > 256 || height > 256) {
		console.log("Map size out of range on line 1.")
		return;
	}
	if (lines.length != height) {
		console.log("Map size out of range on line 1.")
		return;
	}
	var mapData = new Array(width * height);
	var player = { x: 1, y: 1 };
	for (y = 0; y < height; y++) {
		if (lines[y].length != width) {
			console.log(((lines[y].length > width) ? "Too many" : "Insufficient")
					+ " tokens on line " + String(y + 2) + ".")
			return;
		}
		for (x = 0; x < width; x++) {
			var tile = parseInt(lines[y][x], 16);
			if (isNaN(tile) || tile < 0 || tile > 16) {
				console.log("Line " + String(y + 2) + ": unexpected token " + String(x + 1) + " or out of range.")
				return;
			}
			mapData[x + y * width] = tile;
			if (tile == 9) {
				player = { x: x, y: y };
				mapData[x + y * width] = 0;
			}
		}
	}

	/*
	 *	Turn map data into level geometry -- populate
	 *	lines[] and secs[] from visible / occupy-able
	 *	regions
	 */

	var lines = [], secs = [];
	//	Proceed recursively: starting at player spawn, find all connected,
	//	open tiles; then examine the map for remaining open tiles, until
	//	the whole map has been covered exhaustively.
	var queue = [], scanAddr;
	if (mapData[player.x + width * player.y] == 0) {
		queue.push(player.x + width * player.y);
		secs.push( {} );
	}
	for (scanAddr = 0; scanAddr < width * height; scanAddr++) {
		while (queue.length) {
			var seed = queue.shift();
			//	If we're already been here, or a tile was added erroneously, skip
			if (mapData[seed] == 0) {
				mapData[seed] = -1;	//	mark off with sentinel value

				//	Check neighbors: push open ones, or add lines for solid ones
				if (seed >= width && mapData[seed - width] == 0) {	//	up
					queue.push(seed - width);
				} else if (seed >= width && mapData[seed - width] > 0) {
					var x = (seed - width) % width, y = Math.floor((seed - width) / width);
					lines.push( {
						v1: { x:       x * BLOCK_WIDTH, y: (y + 1) * -BLOCK_WIDTH },
						v2: { x: (x + 1) * BLOCK_WIDTH, y: (y + 1) * -BLOCK_WIDTH },
						side1: { idx: mapData[seed - width], sector: secs.length - 1 }
					} );
				}

				if (seed % width < width && mapData[seed + 1] == 0) {	//	right
					queue.push(seed + 1);
				} else if (seed % width < width && mapData[seed + 1] > 0) {
					var x = (seed + 1) % width, y = Math.floor((seed + 1) / width);
					lines.push( {
						v1: { x: x * BLOCK_WIDTH, y:       y * -BLOCK_WIDTH },
						v2: { x: x * BLOCK_WIDTH, y: (y + 1) * -BLOCK_WIDTH },
						side1: { idx: mapData[seed + 1] + 8, sector: secs.length - 1 }
					} );
				}

				if (seed < (height - 1) * width && mapData[seed + width] == 0) {	//	down
					queue.push(seed + width);
				} else if (seed < (height - 1) * width && mapData[seed + width] > 0) {
					var x = (seed + width) % width, y = Math.floor((seed + width) / width);
					lines.push( {
						v1: { x: (x + 1) * BLOCK_WIDTH, y: y * -BLOCK_WIDTH },
						v2: { x:       x * BLOCK_WIDTH, y: y * -BLOCK_WIDTH },
						side1: { idx: mapData[seed + width], sector: secs.length - 1 }
					} );
				}

				if (seed % width > 0 && mapData[seed - 1] == 0) {	//	left
					queue.push(seed - 1);
				} else if (seed % width > 0 && mapData[seed - 1] > 0) {
					var x = (seed - 1) % width, y = Math.floor((seed - 1) / width);
					lines.push( {
						v1: { x: (x + 1) * BLOCK_WIDTH, y: (y + 1) * -BLOCK_WIDTH },
						v2: { x: (x + 1) * BLOCK_WIDTH, y:       y * -BLOCK_WIDTH },
						side1: { idx: mapData[seed - 1] + 8, sector: secs.length - 1 }
					} );
				}

			}
		}
		if (mapData[scanAddr] == 0) {
			queue.push(scanAddr);
			secs.push( {} );
		}
		//	Degenerate case: for loop ends before adding geometry for last
		//	tile, on the border, that happens to be open for some reason,
		//	and surrounded by walls (otherwise it would've been found from
		//	its neighbors).  I'm not going to mind if that happens...
		//	since border tiles already produce open sectors.
	}

	/*
	 *	Turn level geometry into Doom data structures
	 */

	//	compact vertex list using implicit hash map
	var vertMap = {};
	for (i = 0; i < lines.length; i++) {
		var key = "x" + ("0000" + lines[i].v1.x.toString(16)).slice(-4) + "y" + ("0000" + lines[i].v1.y.toString(16)).slice(-4);
		vertMap[key] = lines[i].v1; lines[i].v1 = key;
		key = "x" + ("0000" + lines[i].v2.x.toString(16)).slice(-4) + "y" + ("0000" + lines[i].v2.y.toString(16)).slice(-4);
		vertMap[key] = lines[i].v2; lines[i].v2 = key;
	}
	var vertIdx = [];
	var kl = Object.keys(vertMap);
	for (i = 0; i < kl.length; i++) {
		vertIdx.push(vertMap[kl[i]]);
		vertMap[kl[i]].idx = vertIdx.length - 1;
	}
	for (i = 0; i < lines.length; i++) {
		lines[i].v1 = vertMap[lines[i].v1].idx;
		lines[i].v2 = vertMap[lines[i].v2].idx;
	}

	var Vertices = new Uint8Array(vertIdx.length * 4);
	for (i = 0; i < vertIdx.length; i++) {
		applyVertex(Vertices, i * 4, vertIdx[i]);
	}

	var numSidedefs = lines.length, j = 0;
	for (i = 0; i < lines.length; i++)
		if (lines.side2 !== undefined) numSidedefs++;
	var Sidedefs = new Uint8Array(numSidedefs * 30);
	for (i = 0; i < lines.length; i++) {
		applySidedef(Sidedefs, j * 30, lines[i].side1);
		lines[i].side1 = j++;
		if (lines[i].side2 !== undefined) {
			applySidedef(Sidedefs, j * 30, lines[i].side2);
			lines[i].side2 = j++;
		}
	}

	var Linedefs = new Uint8Array(lines.length * 14);
	for (i = 0; i < lines.length; i++) {
		applyLinedef(Linedefs, i * 14, lines[i]);
	}

	var Sectors = new Uint8Array(secs.length * 26);
	for (i = 0; i < secs.length; i++)
		applySector(Sectors, i * 26, secs[i]);

	//	Construct WAD
	var e = new TextEncoder();
	var Lumps = [
		{ name: "MAP01",    data: new Uint8Array(0) },	//	empty marker
		{ name: "THINGS",   data: new Uint8Array( [0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x07, 0x00] ) },	//	Default type 1 (Player 1) object at (0, 0), right
		{ name: "LINEDEFS", data: Linedefs },
		{ name: "SIDEDEFS", data: Sidedefs },
		{ name: "VERTEXES", data: Vertices },
		//	We can leave these empty, and re-save in Doom Builder, or run BSP
		//{ name: "SEGS",     data: new Uint8Array(0) },
		//{ name: "SSECTORS", data: new Uint8Array(0) },
		//{ name: "NODES",    data: new Uint8Array(0) },
		{ name: "SECTORS",  data: Sectors },
		//{ name: "REJECT",   data: new Uint8Array(0) },
		//{ name: "BLOCKMAP", data: new Uint8Array(0) }
	];
	//	Set player position
	applyWord(Lumps[1].data, 0,  player.x * BLOCK_WIDTH + BLOCK_WIDTH / 2);
	applyWord(Lumps[1].data, 2, -player.y * BLOCK_WIDTH - BLOCK_WIDTH / 2);

	var WAD = new Uint8Array(8);
	WAD = u8concat(e.encode("PWAD"), WAD);
	applyDword(WAD, 4, Lumps.length);
	for (i = 0; i < Lumps.length; i++) {
		Lumps[i].name = new Uint8Array(
				(Lumps[i].name + "\u0000\u0000\u0000\u0000\u0000\u0000\u0000\u0000")
				.substring(0, 8).split("").map(s => s.charCodeAt(0))
				);
	}
	var infoTable = new Uint8Array(Lumps.length * 16);
	for (i = 0; i < Lumps.length; i++) {
		applyDword(infoTable, i * 16, WAD.length);
		applyDword(infoTable, i * 16 + 4, Lumps[i].data.length);
		infoTable.set(Lumps[i].name, i * 16 + 8);
		WAD = u8concat(WAD, Lumps[i].data);
	}
	applyDword(WAD, 8, WAD.length);
	WAD = u8concat(WAD, infoTable);

	//	Download file
	var a = document.createElement("a");
	a.target = "_blank";
	a.href = "data:application/octet-stream;base64,"
			+ btoa(Array.from(WAD, b => String.fromCodePoint(b)).join(""));
	a.download = fileName + ".wad";
	document.body.appendChild(a);
	a.click();
	document.body.removeChild(a);

	function u8concat(a, b) {
		var r = new Uint8Array(a.length + b.length);
		r.set(a, 0); r.set(b, a.length);
		return r;
	}
	function applyWord(a, offs, n) {
		a[offs] = (n >>> 0) & 0xff; a[offs + 1] = (n >>> 8) & 0xff;
	}

	function applyDword(a, offs, n) {
		a[offs + 0] = (n >>>  0) & 0xff; a[offs + 1] = (n >>>  8) & 0xff;
		a[offs + 2] = (n >>> 16) & 0xff; a[offs + 3] = (n >>> 24) & 0xff;
	}

	/** struct mapvertex_t, length 4 */
	function applyVertex(a, offs, v) {
		applyWord(a, offs + 0, v.x);
		applyWord(a, offs + 2, v.y);
	}

	/** struct maplinedef_t, length 14 */
	function applyLinedef(a, offs, ld) {
		applyWord(a, offs +  0, ld.v1);
		applyWord(a, offs +  2, ld.v2);
		applyWord(a, offs +  4, (ld.side2 !== undefined) ? 4 : 1);	//	flags: 1 = impassable, 4 = 2-sided
		applyWord(a, offs +  6, 0);	//	special
		applyWord(a, offs +  8, 0);	//	tag
		applyWord(a, offs + 10, ld.side1);
		applyWord(a, offs + 12, (ld.side2 !== undefined) ? ld.side2 : -1);
	}

	/** struct mapsidedef_t, length 30 */
	function applySidedef(a, offs, sd) {
		applyWord(a, offs + 0, 0);	//	x offset
		applyWord(a, offs + 2, 0);	//	y offset
		const UNUSED = new Uint8Array( [0x2D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00] );
		const MID = new Uint8Array( [0x53, 0x45, 0x43, 0x54, 0x4F, 0x52, 0x00, 0x00] );
		a.set(UNUSED, offs +  4);
		a.set(UNUSED, offs + 12);
		a.set(MID, offs + 20);
		a[offs + 26] = sd.idx.toString(16).toUpperCase().charCodeAt(0);	//	mid texture "SECTORn" (n = 0..9, A..F)
		applyWord(a, offs + 28, sd.sector);	//	sector
	}

	/** struct mapsector_t, length 26 */
	function applySector(a, offs, s) {
		applyWord(a, offs +  0,   0);	//	floor height
		applyWord(a, offs +  2,  BLOCK_HEIGHT);	//	ceiling height
		const FLAT = [0x53, 0x45, 0x43, 0x54, 0x4F, 0x52, 0x38, 0x00];	//	"SECTOR8"
		a.set(FLAT, offs +  4);	//	floor flat
		a.set(FLAT, offs + 12);	//	ceiling flat
		a[offs + 18] = 0x37;	//	"SECTOR7"
		applyWord(a, offs + 20, 255);	//	light level
		applyWord(a, offs + 22,   0);	//	special
		applyWord(a, offs + 24,   0);	//	tag
	}

}
