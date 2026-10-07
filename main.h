#ifndef MAIN_H_INCLUDED
#define MAIN_H_INCLUDED

#include <windows.h>
#include <stdbool.h>

#define numelem(x)			( sizeof(x) / sizeof(x[0]) )

BOOL OnPaint(void);
LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
int WINAPI WinMain(HINSTANCE hThisInstance, HINSTANCE hPrevInstance, LPSTR lpszArgument, int nCmdShow);
void setKeyState(int key, bool pressed);
int loadMap(const char* f);

#endif // MAIN_H_INCLUDED
