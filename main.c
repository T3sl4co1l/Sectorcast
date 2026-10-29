#include "main.h"
#include <windowsx.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <tchar.h>
#include <gdiplus/gdiplus.h>
#include "gdi_colors.h"
#include "common.h"
#include "game.h"
#include "sectors.h"


/* * * Defines * * */

/** Window and drawing dimensions */
#define STRIP_WIDTH		3
#define WIDTH			(GRPH_COLUMNS * STRIP_WIDTH)

//#define POSITION_DEBUG


/* * * Global Variables * * */

/** Class name */
TCHAR szClassName[] = _T("SectorcastWndCls");
/** GDI+ token for unloading */
ULONG_PTR gdiplusToken;
/** Handle for main window */
HWND hMainWindow;
/** Timer handle / ID */
ULONG_PTR hTimer;
/** Mouse capture flag */
BOOL mouseCapture;

/** Color palette to draw everything in */
ARGB drawPalette[] = {
	Color_Black,
	Color_DarkBlue,
	Color_DarkGreen,
	Color_DarkCyan,
	Color_DarkRed,
	Color_Purple,
	Color_SaddleBrown,
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

GameContext game = {
	calX: {
		offset: 0,
		deadband: 0,
		sensitivity: 96
	},
	calY: {
		offset: 0,
		deadband: 0,
		sensitivity: 96
	},
	player: {
		a: 0.0f * M_PI,
		x: -0.5f,
		y: -0.25f,
		z: -0.6f,
		secId: 0,
		radius: GAME_PLAYERSZ,
		height: GAME_PLAYERHT
	},
	grph: {
		cam: {
			a: 0.0f * M_PI,
			x: -0.5f,
			y: -0.25f,
			z: -0.6f + GAME_PLAYERCAM,
			secId: 0
		},
		clearId: 0
	},
	wrl: {
		sectorsLen: 2,
		sectors: sectors
	}
};


/* * * Functions * * */

void setPosAroundConsole(HWND hwnd);
void toggleMouseCapture(void);


/**
 *	Main Entry Point
 */
int WINAPI WinMain(HINSTANCE hThisInstance, HINSTANCE hPrevInstance, LPSTR lpszArgument, int nCmdShow) {
	MSG msg;
	WNDCLASSEX wincl;
	GdiplusStartupInput gdiSI;
	int err;
	ATOM aClass;

	/* * *  Initialization and loading  * * */

	printf("Sectorcast " VERSION_STRING "\n"
	       "Flat shaded, lines, sectors and portals engine\n"
	       "By Tim Williams, " __DATE__ "\n");

//	if (!strlen(lpszArgument)) {
//		printf("\nUsage: SECTORS <mapname>\n");
//		return 0;
//	} else {
		printf("\n"
		       "Keys:\n"
		       "    W, UARROW  Forward\n"
		       "    S, DARROW  Backpedal\n"
		       "    A          Strafe left\n"
		       "    D          Strafe right\n"
		       "    F          Move up\n"
		       "    C          Move down\n"
		       "    LARROW     Turn left\n"
		       "    RARROW     Turn right\n"
		       "    SHIFT      Speed on\n"
		       "    SPACE      Strafe on\n"
		       "    ESC        Quit\n"
		       "    TAB        Capture/release mouse\n"
		       "    F5         Toggle noclip\n"
		       "\n");
//	}
//
//	//	trim space around filename
//	while (isspace(*lpszArgument)) lpszArgument++;
//	for (int i = strlen(lpszArgument) - 1; i > 0 && isspace(lpszArgument[i]); i--)
//		lpszArgument[i] = '\0';
//
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

	setPosAroundConsole(hMainWindow);

	hTimer = SetTimer(hMainWindow, 1, 1000 / GAME_FRAMERATE, NULL);
	if (!hTimer) {
		printf("SetTimer failed with error 0x%lx.\n", GetLastError());
		return 4;
	}

	RAWINPUTDEVICE rid[] = {
		{
			usUsagePage: 0x01,	//	HID_USAGE_PAGE_GENERIC
			usUsage: 0x02,	//	HID_USAGE_GENERIC_MOUSE
			dwFlags: 0
		}
	};
	if (!RegisterRawInputDevices(rid, numelem(rid), sizeof(RAWINPUTDEVICE)))
		printf("RegisterRawInputDevices() error 0x%lx\n", GetLastError());

	doGameTic(&game);	//	prepare graphics context
	render(&game);	//	and render it for the first time

	//	Make the window visible on the screen
	ShowWindow(hMainWindow, nCmdShow);

	//	Windows
	while (GetMessage(&msg, NULL, 0, 0)) {
		DispatchMessage(&msg);
	}

	return 0;
}

/**
 *	Message dispatch handler
 */
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	static _Atomic bool inGameTic = false;
	static RECT rWnd;
	static BOOL mouseWasCapture;

	switch(msg) {
	case WM_SETFOCUS:
		if (mouseWasCapture) {
			mouseWasCapture = FALSE;
			toggleMouseCapture();
		}
		break;
	case WM_KILLFOCUS:
		if (mouseCapture) {
			mouseWasCapture = TRUE;
			toggleMouseCapture();
		}
		break;
	case WM_WINDOWPOSCHANGED:
		//	lParam pointer to WINDOWPOS describing window; convert to RECT
		rWnd.left   = ((WINDOWPOS*)lParam)->x;
		rWnd.top    = ((WINDOWPOS*)lParam)->y;
		rWnd.right  = rWnd.left + ((WINDOWPOS*)lParam)->cx;
		rWnd.bottom = rWnd.top  + ((WINDOWPOS*)lParam)->cy;
		break;
	case WM_SIZING:
		//	window not resizable, unused
	case WM_MOVING:
		//	lParam pointer to RECT of current window size; make a copy
		memcpy((void*)&rWnd, (void*)lParam, sizeof(rWnd));
		break;
	case WM_TIMER:
		if ((ULONG_PTR)wParam == hTimer) {
			//	Not that Windows should be calling us from multiple
			//	threads, but a guard is simple to add
			if (!__atomic_exchange_n(&inGameTic, true, __ATOMIC_ACQUIRE)) {

				//	Handle timer tic
				int ticFlags = doGameTic(&game);
				//	consume mouse axis movement
				game.ctrl.xAxis = 0; game.ctrl.yAxis = 0;
				if (ticFlags & GAMETIC_MOVED) {
					render(&game);
					RedrawWindow(hMainWindow, NULL, NULL, RDW_INVALIDATE);
				}
				//if (ticFlags & GAMETIC_COLLISION)
				//	MessageBeep(MB_ICONEXCLAMATION);	//	Hit something, play a sound

				__atomic_store_n(&inGameTic, false, __ATOMIC_RELEASE);
			}
		}
		break;
	case WM_KEYUP:
		setKeyState((int)wParam, false);
		break;
	case WM_KEYDOWN:
		if (~lParam & (1 << 30))
			setKeyState((int)wParam, true);
		break;
	case WM_INPUT:
		//	pointer registered, get its data
		if (!mouseCapture)
			return DefWindowProc(hwnd, msg, wParam, lParam);
		RAWINPUT ri;
		UINT sz = sizeof(ri);
		GetRawInputData((HRAWINPUT)lParam, RID_INPUT, (LPVOID)&ri, &sz, sizeof(RAWINPUTHEADER));
		if (ri.header.dwType == RIM_TYPEMOUSE && ri.data.mouse.usFlags == MOUSE_MOVE_RELATIVE) {
			game.ctrl.xAxis += ri.data.mouse.lLastX;
			game.ctrl.yAxis -= ri.data.mouse.lLastY;
		}
		break;
	case WM_ACTIVATE:
	case WM_PAINT:
		OnPaint();
		break;
	case WM_DESTROY:
		KillTimer(hwnd, hTimer);
		GdiplusShutdown(gdiplusToken);
		printf("Max cols used: %i\n", game.grph.maxColsLen);
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
	DrawCol* col;

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
			col = &game.grph.cols[next];
			len = col->to - prevTo;
			if (len > 0) {
				if (len > GRPH_ROWS - prevTo) len = GRPH_ROWS - prevTo;
				id = col->id;
				if (id < 0 || id > numelem(drawPalette)) id = 0;
				st |= GdipSetSolidFillColor(f, drawPalette[id]);
				st |= GdipFillRectangleI(g, f, x * STRIP_WIDTH, prevTo, STRIP_WIDTH, len);
			}
			next = col->next; prevTo += len;
			if (next < 0 || next >= numelem(game.grph.cols)) next = 0;
		} while (next != 0 && prevTo < GRPH_ROWS);
	}

	st |= GdipDeleteGraphics(g);
	(void)st;
	if (st) printf("some GDI+ errors; st = %i\n", st);
	EndPaint(hMainWindow, &ps);

	return true;
}

/**
 *	Translate virtual key messages into control inputs.
 */
void setKeyState(int key, BOOL pressed) {
	uint32_t k = 0;

	switch (key) {
	case VK_UP:
	case 'W':
		k = GAMECTRL_FWD;
		break;
	case VK_DOWN:
	case 'S':
		k = GAMECTRL_REV;
		break;
	case VK_LEFT:
		k = GAMECTRL_TURNLEFT;
		break;
	case VK_RIGHT:
		k = GAMECTRL_TURNRIGHT;
		break;	case 'A':
		k = GAMECTRL_STRAFELEFT;
		break;
	case 'D':
		k = GAMECTRL_STRAFERIGHT;
		break;
	case 'F':
		k = GAMECTRL_FLYUP;
		break;
	case 'C':
		k = GAMECTRL_FLYDOWN;
		break;
	case VK_SHIFT:
		k = GAMECTRL_SPEED;
		break;
	case ' ':
		k = GAMECTRL_SLIDE;
		break;
	case VK_TAB:
		if (pressed)
			toggleMouseCapture();
		return;
	case VK_F5:
		if (pressed) {
			game.ctrl.buttons ^= GAMECTRL_NOCLIP;
			printf("Noclip %s\n", (game.ctrl.buttons & GAMECTRL_NOCLIP) ? "on" : "off");
		}
		return;
	case VK_ESCAPE:
		SendMessage(hMainWindow, WM_DESTROY, 0, 0);
		return;
	default:
		return;
	}
	if (pressed)
		game.ctrl.buttons |= k;
	else
		game.ctrl.buttons &= ~k;

}

/**
 *	Toggle mouse capture of the main window.
 */
void toggleMouseCapture(void) {
	RECT rect;
	POINT pt;

	mouseCapture = !mouseCapture;
	if (mouseCapture) {
		SetCapture(hMainWindow);
		GetClientRect(hMainWindow, &rect);
		pt.x = rect.left; pt.y = rect.top;
		ClientToScreen(hMainWindow, &pt);
		rect.left = pt.x; rect.top = pt.y;
		pt.x = rect.right + 1; pt.y = rect.bottom + 1;
		ClientToScreen(hMainWindow, &pt);
		rect.right = pt.x; rect.bottom = pt.y;
		ClipCursor(&rect);
		ShowCursor(FALSE);
		//SetCursorPos((rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2);
	} else {
		ClipCursor(NULL);
		ReleaseCapture();
		ShowCursor(TRUE);
	}

}

/**
 *	Positions hwnd relative to the console attached to
 *	this process, if any.
 */
void setPosAroundConsole(HWND hwnd) {
	HWND hCons;
	HMONITOR hMon;
	WINDOWINFO wi;
	MONITORINFO mi;
	RECT rCons;
	RECT rWnd;
	RECT rScreen;

	hCons = GetConsoleWindow();
	if (hCons == NULL) return;

	wi.cbSize = sizeof(WINDOWINFO);
	GetWindowInfo(hCons, &wi);
	memcpy((void*)&rCons, (void*)&wi.rcWindow, sizeof(RECT));
#ifdef POSITION_DEBUG
	printf("console: %li, %li; %li, %li\n", wi.rcWindow.left,
			wi.rcWindow.top, wi.rcWindow.right, wi.rcWindow.bottom);
#endif // POSITION_DEBUG

	GetWindowInfo(hwnd, &wi);
	memcpy((void*)&rWnd, (void*)&wi.rcWindow, sizeof(RECT));
#ifdef POSITION_DEBUG
	printf("window: %li, %li; %li, %li\n", wi.rcWindow.left,
			wi.rcWindow.top, wi.rcWindow.right, wi.rcWindow.bottom);
#endif // POSITION_DEBUG

	hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
	//	If they're not on the same screen, it's probably fine
	if (MonitorFromWindow(hCons, MONITOR_DEFAULTTONEAREST) != hMon) return;

	mi.cbSize = sizeof(MONITORINFO);
	GetMonitorInfoA(hMon, &mi);
	memcpy((void*)&rScreen, (void*)&mi.rcWork, sizeof(RECT));
#ifdef POSITION_DEBUG
	printf("screen: %li, %li; %li, %li\n", rScreen.left,
			rScreen.top, rScreen.right, rScreen.bottom);
#endif // POSITION_DEBUG

	LONG width = rWnd.right - rWnd.left;
	LONG height = rWnd.bottom - rWnd.top;
	const int COMFYSPACE = 40;
	//	Prefer placement below or right of console;
	if (rScreen.bottom - rCons.bottom > height + COMFYSPACE) {
		rWnd.left = rCons.left + COMFYSPACE / 2;
		rWnd.top = rCons.bottom + COMFYSPACE / 2;
	} else if (rScreen.right - rCons.right > width + COMFYSPACE) {
		rWnd.left = rCons.right + COMFYSPACE / 2;
		rWnd.top = rCons.top + COMFYSPACE / 2;
	//	if there's not enough space for a comfy fit, try above or left;
	} else if (rCons.top - rScreen.top > height + COMFYSPACE) {
		rWnd.left = rCons.left + COMFYSPACE / 2;
		rWnd.top = rCons.top - height - COMFYSPACE / 2;
	} else if (rCons.left - rScreen.left > width + COMFYSPACE) {
		rWnd.left = rCons.left - width - COMFYSPACE / 2;
		rWnd.top = rCons.top + COMFYSPACE / 2;
	//	otherwise, justify towards whichever screen edge leaves the
	//	most console visible
	} else if (rScreen.right - rCons.right - 2 * (rCons.right - rCons.left) / 3 > width + COMFYSPACE / 2) {
		rWnd.left = rScreen.right - width - COMFYSPACE / 4;
		rWnd.top = rCons.top + COMFYSPACE / 2;
	} else if (rScreen.bottom - rCons.bottom - (rCons.bottom - rCons.top) / 2 > height + COMFYSPACE / 2) {
		rWnd.left = rCons.left + COMFYSPACE / 2;
		rWnd.top = rScreen.bottom - height - COMFYSPACE / 4;
	}
	if (rWnd.left > rScreen.right - width) rWnd.left = rScreen.right - width - COMFYSPACE / 4;
	if (rWnd.top > rScreen.bottom - height) rWnd.top = rScreen.bottom - height - COMFYSPACE / 4;
	if (rWnd.left < rScreen.left + COMFYSPACE / 4) rWnd.left = rScreen.left + COMFYSPACE / 4;
	if (rWnd.top < rScreen.top + COMFYSPACE / 4) rWnd.top = rScreen.top + COMFYSPACE / 4;

	MoveWindow(hwnd, rWnd.left, rWnd.top, width, height, FALSE);

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
	if (err != 0 && game.wrl.sectors != NULL) {
		HeapFree(GetProcessHeap(), 0, game.wrl.sectors);
		game.wrl.sectors = NULL;
	}
	return err;
}
