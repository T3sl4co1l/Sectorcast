#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <tchar.h>
#include <gdiplus/gdiplus.h>
#include "gdi_colors.h"
#include "sectors.h"


/* * * Defines * * */

/** Window and drawing dimensions */
#define STRIP_WIDTH		3
#define WIDTH			(GRPH_COLUMNS * STRIP_WIDTH)


/* * * Global Variables * * */

/** Class name */
TCHAR szClassName[] = _T("SectorcastWndCls");
/**	GDI+ token for unloading */
ULONG_PTR gdiplusToken;
/** Handle for main window */
HWND hMainWindow;
/**	Timer handle / ID */
ULONG_PTR hTimer;

/** Color palette to draw everything in */
ARGB drawPalette[] = {
	Color_Black,
	Color_DarkBlue,
	Color_DarkGreen,
	Color_DarkCyan,
	Color_DarkRed,
	Color_Purple,
	Color_Brown,
	Color_LightGray,
	Color_DarkGray,
	Color_LightBlue,
	Color_LightGreen,
	Color_Cyan,
	Color_Salmon,
	Color_Magenta,
	Color_Yellow,
	Color_White
};

GrphContext_t gc = {
	camX: -0.5f,
	camY: -0.25f,
	camZ: 0.0f,
	camA: 0.0f * M_PI,
	clearId: 0,
	sectors: sectors,
	sectorMax: 2,
	currentSectorId: 0
};


/* * * Functions * * */

/**
 *	Main Entry Point
 */
int WINAPI WinMain(HINSTANCE hThisInstance, HINSTANCE hPrevInstance, LPSTR lpszArgument, int nCmdShow) {
	MSG msgs;
	WNDCLASSEX wincl;
	GdiplusStartupInput gdiSI;
	int err;
	ATOM aClass;

	/* * *  Initialization and loading  * * */

	printf("Sectorcast " VERSION_STRING "\n"
	       "Flat shaded, lines, sectors and portals engine\n"
	       "By Tim Williams, " __DATE__ "\n");
	if (!strlen(lpszArgument)) {
		printf("\nUsage: SECTORS <mapname>\n");
		return 0;
	}

	//	trim space around filename
	while (isspace(*lpszArgument)) lpszArgument++;
	for (int i = strlen(lpszArgument) - 1; i > 0 && isspace(lpszArgument[i]); i--)
		lpszArgument[i] = '\0';

//	printf("Loading map \"%s\"...\n", lpszArgument);
//	if ((err = loadMap(lpszArgument))) {
//		if (err == -1) {
//			printf("File not found.\n");
//		} else if (err == -2) {
//			printf("Windows error: 0x%lx\n", GetLastError());
//		} else if (err == -3) {
//			printf("Player start not found.\n");
//		} else {
//			printf("Failed at character: %i\n", err);
//		}
//		return 1;
//	}

	/* * *  Deal with all the Windows crap  * * */

	gdiSI = (GdiplusStartupInput){ GdiplusVersion: 1 };
	if ((err = GdiplusStartup(&gdiplusToken, &gdiSI, NULL))) {
		printf("Error opening GDI+, status: 0x%x\n", err);
		return 2;
	}

	//	set up window class...
	wincl = (WNDCLASSEX){
		cbSize: sizeof(WNDCLASSEX),
		style: CS_DBLCLKS, 	//	Catch double-clicks
		lpfnWndProc: WindowProc,
		cbClsExtra: 0,     	//	No extra bytes after the window class structure,
		cbWndExtra: 0,     	//	or the window instance
		hbrBackground: (HBRUSH)COLOR_BACKGROUND,
		lpszMenuName: NULL,	//	No menu
		lpszClassName: szClassName
	};
	wincl.hInstance = hThisInstance;
	//	Use default icon and mouse cursor
	wincl.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wincl.hCursor = LoadCursor(NULL, IDC_ARROW);
	wincl.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

	//	Register window class, and if it fails, quit out
	if (!(aClass = RegisterClassEx(&wincl))) {
		printf("RegisterClassEx failed with error 0x%lx.\n", GetLastError());
		return 3;
	}
	//	Class is registered, create window...
	hMainWindow = CreateWindowEx(
			0,            	//	Extended possibilities for variation
			MAKEINTATOM(aClass),	//	Class atom
			_T("Sectorcast"),	//	Title text
			(WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX),	//	window style
			CW_USEDEFAULT,	//	Windows decides position where..
			CW_USEDEFAULT,	//	..the window ends up on the screen
			WIDTH,        	//	Window's width..
			GRPH_ROWS,    	//	..and height in px
			HWND_DESKTOP, 	//	Is child to desktop
			NULL,         	//	No menu
			hThisInstance,	//	Application instance
			NULL          	//	No window creation data
			);
	if (!hMainWindow) {
		printf("CreateWindowEx failed with error 0x%lx.\n", GetLastError());
		return 3;
	}

	hTimer = SetTimer(hMainWindow, 1, 1000 / GRPH_FRAMERATE, NULL);
	if (!hTimer) {
		printf("SetTimer failed with error 0x%lx.\n", GetLastError());
		return 4;
	}

	render(&gc);

	//	Make the window visible on the screen
	ShowWindow(hMainWindow, nCmdShow);

	/* Run the message loop. It will run until GetMessage() returns 0 */
	while (GetMessage(&msgs, NULL, 0, 0)) {
		//	Translate virtual-key messages into character messages
		TranslateMessage(&msgs);
		//	Send message to WindowProc
		DispatchMessage(&msgs);
	}

	return 0;
}

/**
 *	Message dispatch handler
 */
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	static _Atomic bool inGameTic = false;

	switch(msg) {
	case WM_TIMER:
		if ((ULONG_PTR)wParam == hTimer) {
			//	Not that Windows should be calling us from multiple
			//	threads, but a guard is simple to add
			if (!__atomic_exchange_n(&inGameTic, true, __ATOMIC_ACQUIRE)) {

				//	Handle timer tic
				int ticFlags = doGameTic(&gc);
				if (ticFlags & GRPHTIC_MOVED)
					RedrawWindow(hMainWindow, NULL, NULL, RDW_INVALIDATE);

				if (ticFlags & GRPHTIC_COLLISION)
					MessageBeep(MB_ICONEXCLAMATION);	//	Hit something, play a sound

				__atomic_store_n(&inGameTic, false, __ATOMIC_RELEASE);
			}
		}
		break;
	case WM_KEYUP:
		setKeyState((int)wParam, false);
		break;
	case WM_KEYDOWN:
		if (~lParam & (1 << 30)) {
			setKeyState((int)wParam, true);
		}
		break;
	case WM_ACTIVATE:
	case WM_PAINT:
		OnPaint();
		break;
	case WM_DESTROY:
		KillTimer(hwnd, hTimer);
		GdiplusShutdown(gdiplusToken);
		printf("Max cols used: %i\n", gc.maxColsLen);
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}

	return 0;
}

/**
 *	Draws graphical output to the window.
 */
BOOL OnPaint(void) {
	int id, len, next, prevTo, st, x;
	HDC hdc;
	PAINTSTRUCT ps;
	GpGraphics *g;
	GpSolidFill *f;
	DrawCol_t* col;

	if (!(hdc = BeginPaint(hMainWindow, &ps))) {
		EndPaint(hMainWindow, &ps);
		return false;
	}
	st = GdipCreateFromHDC(hdc, &g);
	st |= GdipCreateSolidFill(drawPalette[0], &f);

	for (x = 0; x < GRPH_COLUMNS; x++) {
		//	each column starts from its respective index in .cols;
		//	these are ceilings/upper walls
		next = x; prevTo = 0;
		do {
			col = &gc.cols[next];
			len = col->to - prevTo;
			if (len > 0) {
				if (len > GRPH_ROWS - prevTo) len = GRPH_ROWS - prevTo;
				id = col->id;
				if (id < 0 || id > numelem(drawPalette)) id = 0;
				st |= GdipSetSolidFillColor(f, drawPalette[id]);
				st |= GdipFillRectangleI(g, f, x * STRIP_WIDTH, prevTo, STRIP_WIDTH, len);
			}
			next = col->next; prevTo += len;
			if (next < 0 || next >= numelem(gc.cols)) next = 0;
		} while (next != 0 && prevTo < GRPH_ROWS);
	}

	st |= GdipDeleteGraphics(g);
	(void)st;
	if (st) printf("some GDI+ errors; st = %i\n", st);
	EndPaint(hMainWindow, &ps);

	return true;
}

/**
 *	Convert virtual key messages into control inputs.
 */
void setKeyState(int key, bool pressed) {
	int k;

	switch (key) {
	case VK_UP:
	case 'W':
		k = GRPHCTRL_FWD;
		break;
	case VK_DOWN:
	case 'S':
		k = GRPHCTRL_REV;
		break;
	case VK_LEFT:
		k = GRPHCTRL_TURNLEFT;
		break;
	case VK_RIGHT:
		k = GRPHCTRL_TURNRIGHT;
		break;	case 'A':
		k = GRPHCTRL_STRAFELEFT;
		break;
	case 'D':
		k = GRPHCTRL_STRAFERIGHT;
		break;
	case 'F':
		k = GRPHCTRL_FLYUP;
		break;
	case 'C':
		k = GRPHCTRL_FLYDOWN;
		break;
	case VK_ESCAPE:
		SendMessage(hMainWindow, WM_DESTROY, 0, 0);
		return;
	default:
		return;
	}
	gc.controls[k] = pressed;

}

/**
 *	Loads map data from the specified filename.
 *	worldMap will be allocated on the heap, and freed first
 *	if non-NULL.
 *	@param f  String; filename, zero terminated
 *	@return 0: success<br>
 *	 positive: file offset at which an error occurred<br>
 *	 negative: error:<br>
 *	               -1: Failed to open file<br>
 *	               -2: Windows error; check GetLastError()<br>
 *	               -3: No player start location
 */
int loadMap(const char* f) {
	int err = 0;
//	int err = 0, scanlen, tokens, x, y, col;
	HANDLE hFile = INVALID_HANDLE_VALUE;
	DWORD bytesRead;
	LARGE_INTEGER fLen;
	char* mapbuf = NULL;

	//	Memory-map file (could also use mmap as such)
	hFile = CreateFileA(
			f, GENERIC_READ, FILE_SHARE_READ, NULL,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		err = -1;
		goto loadFileError;
	}
	if (!GetFileSizeEx(hFile, &fLen)) {
		err = -1;
		goto loadFileError;
	}
	if (fLen.QuadPart < 5) {	//	negative would be weird, but also exclude non-minimum-size files
		err = -1;
		goto loadFileError;
	}	fLen.QuadPart++;
	if (fLen.HighPart > 0) {
		err = -1;
		goto loadFileError;
	}	mapbuf = HeapAlloc(GetProcessHeap(), 0, fLen.QuadPart);
	if (mapbuf == NULL) {
		err = -2;
		goto loadFileError;
	}	fLen.QuadPart--;
	ReadFile(hFile, mapbuf, fLen.LowPart, &bytesRead, NULL);
	if (bytesRead != fLen.LowPart) {
		err = -2;
		goto loadFileError;
	}	mapbuf[fLen.QuadPart] = 0;	//	ensure very last entry is zero-terminated
	CloseHandle(hFile);

	/*	parse mapbuf:
	 *	- Line breaks are any '\n'
	 *	- Tokens are any numeric digits, separated by white space
	 *	- From file start, 0 or more lines starting with "//" are comments; skip over them
	 *	- First (non-comment) line: two tokens (width, height)
	 *	- Subsequent lines: (width) tokens per line, for (row) lines (expected; but lines
	 *	  aren't special, any whitespace suffices for delimiting and tokens are positional)
	 *	- Additional lines/tokens after array: ignored
	 */
	size_t cursor = 0;
	while (cursor < fLen.QuadPart - 1  && mapbuf[cursor] == '/' && mapbuf[cursor + 1] == '/') {
		//	advance whole lines until heading comments clear
		cursor = (size_t)memchr(&mapbuf[cursor], '\n', fLen.QuadPart - cursor);
		if (cursor == (size_t)NULL)
			break;
		cursor = cursor - (size_t)mapbuf + 1;
		if (cursor >= (size_t)fLen.QuadPart) {
			err = cursor;
			goto loadFileError;
		}	}
	if (cursor >= (size_t)fLen.QuadPart) {
		err = cursor;
		goto loadFileError;
	}
/*
	//	capture tokens
	tokens = sscanf(&mapbuf[cursor], "%d %d%n",
				&gc.mapW,
				&gc.mapH,
				&scanlen);
	cursor += scanlen;
	if (tokens < 2 || cursor >= (size_t)fLen.QuadPart) {
		err = cursor;
		goto loadFileError;
	}
	printf("Map size: %i x %i\n", gc.mapW, gc.mapH);

	//	Allocate the map, and read in data
	if (gc.world != NULL) {
		//	we've probably been here before; realloc
		HeapFree(GetProcessHeap(), 0, gc.world);
	}
	gc.world = HeapAlloc(GetProcessHeap(), 0,
				gc.mapW * gc.mapH * sizeof(*gc.world));
	if (gc.world == NULL) {
		err = -2;
		goto loadFileError;
	}

	gc.camX = -1.0f; gc.camY = -1.0f; gc.camA = 0.0f;
	for (y = 0; y < gc.mapH; y++) {
		for (x = 0; x < gc.mapW; x++) {
			tokens = sscanf(&mapbuf[cursor], " %d%n", &col, &scanlen); cursor += scanlen;
			if (tokens < 1 || cursor >= (size_t)fLen.QuadPart) {
				err = cursor;
				goto loadFileError;
			}
			if (col < 0 || col > 9) {
				printf("Ignoring invalid map value at %I64d\n", cursor);
				col = 0;
			}
			if (col == 9) {
				if (gc.camX >= 0.0f) {
					printf("Ignoring extra player start at %I64d\n", cursor);
					col = 0;
				} else {
					gc.camX = x + 0.5f; gc.camY = y + 0.5f;
					printf("Player at location: %i, %i\n", x, y);
					col = 0;
				}
			}
			gc.world[x + y * gc.mapW] = col;
		}
	}
	if (gc.camX < 0.0f || gc.camY < 0.0f) {
		err = -3;
		goto loadFileError;
	}
*/
	err = 0;

loadFileError:
	if (hFile != INVALID_HANDLE_VALUE)
		CloseHandle(hFile);
	if (mapbuf != NULL)
		HeapFree(GetProcessHeap(), 0, mapbuf);
	if (err != 0 && gc.sectors != NULL) {
		HeapFree(GetProcessHeap(), 0, gc.sectors);
		gc.sectors = NULL;
	}
	return err;
}
