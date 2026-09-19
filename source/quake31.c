#include <windows.h>
#include <mmsystem.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <ctype.h>
#include <i86.h>
#include <float.h>
#include "quake31.h"
#include "quakedef.h"
#include "quakegeneric.h"

#define MAX_ARGS 32

HINSTANCE       MyInstance;
static char     GenericClass[32]="GenericClass";

static BOOL FirstInstance( HINSTANCE );
static BOOL AnyInstance( HINSTANCE, int, LPSTR );

long _EXPORT FAR PASCAL WindowProc( HWND, unsigned, UINT, LONG );

HINSTANCE hMMsystem = NULL;
FARPROC fptimeGetTime;
FARPROC fpSoundInit16;
FARPROC fpSoundInit;
FARPROC fpSoundQuit;
FARPROC fpSoundPosition;
FARPROC fpSoundWrite;

HINSTANCE hWinG = NULL;
FARPROC fpWinGCreateDC;
FARPROC fpWinGCreateBitmap;
FARPROC fpWinGGetDIBPointer;
FARPROC fpWinGStretchBlt;
FARPROC fpWinGSetDIBColorTable;

#define KEYBUFFERSIZE 32
int keybuffer[KEYBUFFERSIZE];
int keybuffer_len;
int keybuffer_start;

cvar_t m_filter ={"m_filter", "1"};

struct {
    BITMAPINFOHEADER bmiHeader;
    RGBQUAD          bmiColors[256];
} bmiBuffer;

HWND globalHWND = NULL;
HDC hdcWinG = NULL;
BITMAPINFO *bmi = (BITMAPINFO *)&bmiBuffer;
HBITMAP backBitmap = 0;
HBITMAP hOldBitmap;
char far *farbitmap;
int WINDOW_WIDTH = 320;
int WINDOW_HEIGHT = 240;
int old_mouse_x, old_mouse_y;
int enable_mouse = 0;

static int sound_flag = 0;
static int pcm_buffer_size = 0;
unsigned char *pcm_buffer=0;
HGLOBAL hSoundMem = NULL;

qboolean looping;
int ret;


int ParseWin16CmdLine(HINSTANCE hInstance, LPSTR lpCmdLine, char*** pArgv) {
	int argc = 0;
	char** argv;
	char* p;
	char buf[256];
	int bufIdx;
	int inQuote;

	argv = (char**)calloc(MAX_ARGS, sizeof(char*));
	if (!argv) return 0;

	GetModuleFileName(hInstance, buf, sizeof(buf));
	argv[0] = _strdup(buf);
	argc = 1;

	p = lpCmdLine;
	while (*p != '\0' && argc < MAX_ARGS - 1) {
		while (*p == ' ' || *p == '\t') p++;
        	if (*p == '\0') break;

		bufIdx = 0;
		inQuote = 0;

        	while (*p != '\0') {
			if (*p == '"') {
				inQuote = !inQuote;
			} else if (!inQuote && (*p == ' ' || *p == '\t')) {
				break;
			} else {
				if (bufIdx < sizeof(buf) - 1) {
					buf[bufIdx++] = *p;
				}
			}
			p++;
		}
		buf[bufIdx] = '\0';

		argv[argc++] = _strdup(buf);
	}

	argv[argc] = NULL;
	*pArgv = argv;
	return argc;
}

int Check_Screen_Colors()
{
	HDC hdc;
	int bitsPerPixel;
	int numPlanes;
	int bpp;

	hdc = GetDC(NULL);

	bitsPerPixel = GetDeviceCaps(hdc, BITSPIXEL);
	numPlanes    = GetDeviceCaps(hdc, PLANES);
	bpp = bitsPerPixel * numPlanes;

	ReleaseDC(NULL, hdc);

	if(bpp <= 8)
	{
		return 0;
	}

	return 1;
}

int Load_DLL()
{
	hMMsystem = LoadLibrary("QUAKE31.DLL");
	if(hMMsystem< (HINSTANCE)HINSTANCE_ERROR)
	{
		return 0;
	}
	fptimeGetTime = GetProcAddress(hMMsystem, "GetTime");
	fpSoundInit16 = GetProcAddress(hMMsystem, "SoundInit16");
	fpSoundInit = GetProcAddress(hMMsystem, "SoundInit");
	fpSoundQuit = GetProcAddress(hMMsystem, "SoundQuit");
	fpSoundPosition = GetProcAddress(hMMsystem, "SoundPosition");
	fpSoundWrite = GetProcAddress(hMMsystem, "SoundWrite");
	if ((!fptimeGetTime))
	{
		MessageBox(NULL, "Can't find Quake 16bit functions", "Error", MB_OK);
		return 0;
	}

	hWinG = LoadLibrary("WING.DLL");
	if(hWinG< (HINSTANCE)HINSTANCE_ERROR)
	{
		return 0;
	}
	fpWinGCreateDC = GetProcAddress(hWinG, "WinGCreateDC");
	fpWinGCreateBitmap = GetProcAddress(hWinG, "WinGCreateBitmap");
	fpWinGGetDIBPointer = GetProcAddress(hWinG, "WinGGetDIBPointer");
	fpWinGStretchBlt = GetProcAddress(hWinG, "WinGStretchBlt");
	fpWinGSetDIBColorTable = GetProcAddress(hWinG, "WinGSetDIBColorTable");
	if (	(!fpWinGSetDIBColorTable) ||
		(!fpWinGCreateBitmap) ||
		(!fpWinGGetDIBPointer) ||
		(!fpWinGStretchBlt) ||
		(!fpWinGSetDIBColorTable) )
	{
		MessageBox(NULL, "Can't find WinG functions", "Error", MB_OK);
		return 0;
	}

	return 1;
}


void init_Wing_Bitmap()
{
	DWORD ptr;

	memset(&bmiBuffer, 0, sizeof(bmiBuffer));
	bmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi->bmiHeader.biWidth = 320;
	bmi->bmiHeader.biHeight = -200;
	bmi->bmiHeader.biPlanes = 1;
	bmi->bmiHeader.biBitCount = 8;
	bmi->bmiHeader.biCompression = BI_RGB;
	bmi->bmiHeader.biClrUsed = 256;

	hdcWinG = (HDC)_Call16(fpWinGCreateDC, "");
	backBitmap = (HBITMAP)_Call16(fpWinGCreateBitmap, "wpp", hdcWinG, bmi, &farbitmap);
	hOldBitmap = SelectObject(hdcWinG, backBitmap);

	ptr = _Call16(fpWinGGetDIBPointer, "wp", backBitmap, bmi);
	farbitmap = MK_FP( ptr >> 16, ptr & 0xffff );

	_fmemset(farbitmap, 0, 64000);
}


int ConvertToQuakeKey(unsigned int key){
	int qkey;

	switch (key){
		case VK_BACK:
			qkey = K_BACKSPACE;
			break;
		case VK_TAB:
			qkey = K_TAB;
			break;
		case VK_ESCAPE:
			qkey = K_ESCAPE;
			break;
		case VK_RETURN:
			qkey = K_ENTER;
			break;
		case VK_SPACE:
			qkey = K_SPACE;
			break;
		case VK_PRIOR:
			qkey = K_PGUP;
			break;
		case VK_NEXT:
			qkey = K_PGDN;
			break;
		case VK_END:
			qkey = K_END;
			break;
		case VK_HOME:
			qkey = K_HOME;
			break;
		case VK_UP:
			qkey = K_UPARROW;
			break;
		case VK_DOWN:
			qkey = K_DOWNARROW;
			break;
		case VK_LEFT:
			qkey = K_LEFTARROW;
			break;
		case VK_RIGHT:
			qkey = K_RIGHTARROW;
			break;
		case VK_CONTROL:
			qkey = K_CTRL;
			break;
		case VK_SHIFT:
			qkey = K_SHIFT;
			break;
		case VK_PAUSE:
			qkey = K_PAUSE;
			break;
		case VK_INSERT:
			qkey = K_INS;
			break;
		case VK_DELETE:
			qkey = K_DEL;
			break;
		case VK_F1:
			qkey = K_F1;
			break;
		case VK_F2:
			qkey = K_F2;
			break;
		case VK_F3:
			qkey = K_F3;
			break;
		case VK_F4:
			qkey = K_F4;
			break;
		case VK_F5:
			qkey = K_F5;
			break;
		case VK_F6:
			qkey = K_F6;
			break;
		case VK_F7:
			qkey = K_F7;
			break;
		case VK_F8:
			qkey = K_F8;
			break;
		case VK_F9:
			qkey = K_F9;
			break;
		case VK_F10:
			qkey = K_F10;
			break;
		case VK_F11:
			qkey = K_F11;
			break;
		case VK_F12:
			qkey = K_F12;
			break;
		default:
			qkey = tolower(key);
			break;
	}

	return qkey;
}

int KeyPop(int *down, int *key)
{
	if (keybuffer_len == 0)
		return 0; // underflow

	*key = keybuffer[keybuffer_start];
	*down = *key < 0;
	if (*key < 0)
		*key = -*key;
	keybuffer_start = (keybuffer_start + 1) % KEYBUFFERSIZE;
	keybuffer_len--;

	return 1;
}

int KeyPush(int down, int key)
{
	if (keybuffer_len == KEYBUFFERSIZE)
		return 0; // overflow
	if (down) {
		key = -key;
	}
	keybuffer[(keybuffer_start + keybuffer_len) % KEYBUFFERSIZE] = key;
	keybuffer_len++;

	return 1;
}



void QG_Init(void)
{
	Cvar_RegisterVariable (&m_filter);
}

int QG_GetKey(int *down, int *key)
{
	return KeyPop(down, key);
}

void QG_GetMouseMove(int *x, int *y)
{
	int mouse_x, mouse_y;
	RECT rect;
	POINT pt;
	int centerX;
	int centerY;

	if(enable_mouse)
	{
		GetWindowRect(globalHWND, &rect);

		centerX = rect.left + (rect.right - rect.left) / 2;
		centerY = rect.top + (rect.bottom - rect.top) / 2;

		GetCursorPos(&pt);

		mouse_x = pt.x - centerX;
		mouse_y = pt.y - centerY;

		if (m_filter.value)
		{
			mouse_x = (mouse_x + old_mouse_x) >> 1;
			mouse_y = (mouse_y + old_mouse_y) >> 1;
			old_mouse_x = pt.x - centerX;
			old_mouse_y = pt.y - centerY;
		}

		SetCursorPos(centerX, centerY);
	}
	else
	{
		mouse_x = mouse_y = 0;
	}

	*x = mouse_x;
	*y = mouse_y;
}

void QG_GetJoyAxes(float *axes)
{
	*axes = 0;
}

void QG_Quit(void)
{
	int i;

	if(hdcWinG)
	{
		SelectObject(hdcWinG, hOldBitmap);
		DeleteDC(hdcWinG);
		DeleteObject(backBitmap);
	}

	if(sound_flag)
	{
		_Call16(fpSoundQuit, "");
	}

	i = ShowCursor(TRUE);
	while(1)
	{
		if(i == 0)
		{
			break;
		}
		else if(i < 0)
		{
			i = ShowCursor(TRUE);
		}
		else if(i >= 1)
		{
			i = ShowCursor(FALSE);
		}
	}

	if(hWinG >= (HINSTANCE)HINSTANCE_ERROR)
	{
		FreeLibrary(hWinG);
	}
	if(hMMsystem >= (HINSTANCE)HINSTANCE_ERROR)
	{
		FreeLibrary(hMMsystem);
	}

	if(MyInstance)
	{
		UnregisterClass(GenericClass, MyInstance);
	}

	QG_Free();
}

void QG_DrawFrame(void *pixels)
{
	HDC hdcWin;
	char *pix;
	pix = (char *)pixels;

	_fmemcpy(farbitmap, pix, 64000);

	hdcWin = GetDC(globalHWND);
	_Call16(fpWinGStretchBlt, "wwwwwwwwww", hdcWin, 0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, hdcWinG, 0, 0, 320, 200);
	ReleaseDC(globalHWND, hdcWin);
}

void QG_SetPalette(unsigned char palette[768])
{
	int i;
	RGBQUAD *willPalette;
	const unsigned char *p = palette;
	willPalette = &(bmi->bmiColors[0]);

	for(i = 0; i < 256; i++)
	{
		willPalette[i].rgbRed   = p[0];
		willPalette[i].rgbGreen = p[1];
		willPalette[i].rgbBlue  = p[2];
		willPalette[i].rgbReserved = 0;

		p += 3;
	}

	_Call16(fpWinGSetDIBColorTable, "wwwp", hdcWinG, 0, 256, willPalette);
}


qboolean SNDDMA_Init(void)
{
	int i;

	shm = 0;

	hSoundMem = GlobalAlloc(GMEM_FIXED, 4096);
	if(hSoundMem == NULL)
	{
		return false;
	}

	pcm_buffer = malloc(4096);
	if(pcm_buffer == NULL)
	{
		return false;
	}

	i = _Call16(fpSoundInit16, "w", 4096);
	if(i == 0)
	{
		i = _Call16(fpSoundInit, "w", 1024);
		if(i == 0)
		{
			return false;
		}

		sound_flag = 1;

		shm = &sn;

		shm->channels = 1;
		shm->samplebits = 8;
		shm->speed = 11025;
		shm->soundalive = true;
		shm->splitbuffer = false;
		shm->samples = (1024) / (shm->samplebits/8);
		shm->samplepos = 0;
		shm->submission_chunk = 1;
		shm->buffer = pcm_buffer;
		pcm_buffer_size = 1024;
		memset(pcm_buffer, 128, 4096);

		return true;
	}

	sound_flag = 1;

	shm = &sn;

	shm->channels = 2;
	shm->samplebits = 16;
	shm->speed = 11025;
	shm->soundalive = true;
	shm->splitbuffer = false;
	shm->samples = (4096) / (shm->samplebits/8);
	shm->samplepos = 0;
	shm->submission_chunk = 1;
	shm->buffer = pcm_buffer;
	pcm_buffer_size = 4096;
	memset(pcm_buffer, 0, 4096);

	return true;
}

int SNDDMA_GetDMAPos(void)
{
	int s;

	s = _Call16(fpSoundPosition, "");

	s >>= ((shm->samplebits/8) - 1);

	return s;
}

void SNDDMA_Shutdown(void)
{
	_Call16(fpSoundQuit, "");
}

void SNDDMA_Submit(void)
{
	if(sound_flag)
	{
		DWORD far16_ptr;
		char *flat_ptr;
		far16_ptr = (DWORD)hSoundMem;
		flat_ptr = (char *)MK_FP32((void __far16 *)far16_ptr);
		memcpy(flat_ptr, pcm_buffer, pcm_buffer_size);
		_Call16(fpSoundWrite, "p", flat_ptr);
	}
}


void Sys_SendKeyEvents (void)
{
	MSG         msg;
	while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
	{
		if (msg.message == WM_QUIT)
		{
			looping = false;
			ret = msg.wParam;
			break;
		}
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}


/*
 * WinMain - initialization, message loop
 */
int PASCAL WinMain( HINSTANCE this_inst, HINSTANCE prev_inst, LPSTR cmdline,
                    int cmdshow )
{
	int argc;
	char** argv;
	DWORD nowtime;
	double oldtime, newtime;
	MyInstance = this_inst;

	_fpreset();

	if(!Check_Screen_Colors())
	{
		MessageBox(NULL, "Quake for Windows 3.1 requires higher than 15 bit colors mode.", "Error", MB_OK);
		return( FALSE );
	}

	if(!Load_DLL())
	{
		return( FALSE );
	}
	init_Wing_Bitmap();

	atexit(QG_Quit);

#ifdef __WINDOWS_386__
	sprintf( GenericClass, "GenericClass%d", this_inst );
	prev_inst = 0;
#endif
	if( !prev_inst )
	{
		if( !FirstInstance( this_inst ) )
		{
			return( FALSE );
		}
	}
	if( !AnyInstance( this_inst, cmdshow, cmdline ) )
	{
		return( FALSE );
	}

	argc = ParseWin16CmdLine(MyInstance, cmdline, &argv);
	if(!QG_Create(argc, argv))
	{
		MessageBox(NULL, "Failed to allocate memory.", "Error", MB_OK);
		return( FALSE );
	}

	nowtime = _Call16(fptimeGetTime, "");
	oldtime = (double)nowtime / 1000 - 0.1;

	looping = true;
	while (looping)
	{
		nowtime = _Call16(fptimeGetTime, "");
		newtime = (double)nowtime / 1000;
		QG_Tick(newtime - oldtime);
		oldtime = newtime;
	}

	return( ret );

} /* WinMain */

/*
 * FirstInstance - register window class for the application,
 *                 and do any other application initialization
 */
static BOOL FirstInstance( HINSTANCE this_inst )
{
	WNDCLASS    wc;
	BOOL        rc;

	/*
	 * set up and register window class
	 */
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = (LPVOID) WindowProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = sizeof( DWORD );
	wc.hInstance = this_inst;
	wc.hIcon = LoadIcon( this_inst, "QuakeIcon" );
	wc.hCursor = LoadCursor( NULL, IDC_ARROW );
	wc.hbrBackground = GetStockObject( WHITE_BRUSH );
	wc.lpszMenuName = NULL;
	wc.lpszClassName = GenericClass;
	rc = RegisterClass( &wc );
	return( rc );

} /* FirstInstance */

/*
 * AnyInstance - do work required for every instance of the application:
 *                create the window, initialize data
 */
static BOOL AnyInstance( HINSTANCE this_inst, int cmdshow, LPSTR cmdline )
{
	HWND        hwnd;
	extra_data  *edata_ptr;

	/*
	 * create main window
	 */
	hwnd = CreateWindow(
		GenericClass,           /* class */
		"Quake for Windows 3.1",  /* caption */
		WS_OVERLAPPEDWINDOW,    /* style */
		CW_USEDEFAULT,          /* init. x pos */
		CW_USEDEFAULT,          /* init. y pos */
		320,                    /* init. x size */
		240,                    /* init. y size */
		NULL,                   /* parent window */
		NULL,                   /* menu handle */
		this_inst,              /* program handle */
		NULL                    /* create parms */
		);

	if( !hwnd ) return( FALSE );

	/*
	 * set up data associated with this window
	 */
	edata_ptr = malloc( sizeof( extra_data ) );
	if( edata_ptr == NULL ) return( FALSE );
	edata_ptr->cmdline = cmdline;
	SetWindowLong( hwnd, EXTRA_DATA_OFFSET, (DWORD) edata_ptr );

	/*
	 * display window
	 */
	ShowWindow( hwnd, cmdshow );
	UpdateWindow( hwnd );

	globalHWND = hwnd;

	return( TRUE );

} /* AnyInstance */

/*
 * AboutDlgProc - processes messages for the about dialog.
 */
BOOL _EXPORT FAR PASCAL AboutDlgProc( HWND hwnd, unsigned msg,
                                UINT wparam, LONG lparam )
{
	lparam = lparam;                    /* turn off warning */

	switch( msg ) {
		case WM_INITDIALOG:
			return( TRUE );

		case WM_COMMAND:
			if( LOWORD( wparam ) == IDOK ) {
			EndDialog( hwnd, TRUE );
			return( TRUE );
        		}
        		break;
    	}
	return( FALSE );

} /* AboutDlgProc */

/*
 * WindowProc - handle messages for the main application window
 */
LONG _EXPORT FAR PASCAL WindowProc( HWND hwnd, unsigned msg,
                                     UINT wparam, LONG lparam )
{
	/*FARPROC     proc;
	extra_data  *edata_ptr;
	char        buff[128];*/

	switch( msg ) {
		case WM_COMMAND:
			/*switch( LOWORD( wparam ) ) {
				case MENU_ABOUT:
					proc = MakeProcInstance( (FARPROC)AboutDlgProc, MyInstance );
					DialogBox( MyInstance,"AboutBox", hwnd, (DLGPROC)proc );
					FreeProcInstance( proc );
					break;

				case MENU_CMDSTR:
					edata_ptr = (extra_data *) GetWindowLong( hwnd, EXTRA_DATA_OFFSET );
#ifdef __NT__
					sprintf( buff, "Command string was \"%s\"", edata_ptr->cmdline );
#else
					sprintf( buff, "Command string was \"%Fs\"", edata_ptr->cmdline );
#endif
					MessageBox( NULL, buff, "Program Information", MB_OK );
					break;
			}*/
			break;

		case WM_DESTROY:
      			PostQuitMessage( 0 );
       			break;

		case WM_CLOSE:
			DestroyWindow(hwnd);
			break;

		case WM_SIZE:
			WINDOW_WIDTH = lparam & 0xffff;
			WINDOW_HEIGHT = (lparam & 0xffff0000) >> 16;
			break;

		case WM_LBUTTONDOWN:
			KeyPush(1, K_MOUSE1);
			break;

		case WM_LBUTTONUP:{
			KeyPush(0, K_MOUSE1);
			break;
		}

		case WM_RBUTTONDOWN:
			enable_mouse = 1 - enable_mouse;
			if(enable_mouse)
			{
				ShowCursor(FALSE);
			}
			else
			{
				ShowCursor(TRUE);
			}
			break;

		case WM_RBUTTONUP:
			break;

		case WM_KEYDOWN:
			KeyPush(1, ConvertToQuakeKey((unsigned char)wparam));
			break;

		case WM_KEYUP:
			KeyPush(0, ConvertToQuakeKey((unsigned char)wparam));
			break;

		default:
			return( DefWindowProc( hwnd, msg, wparam, lparam ) );
	}
	return( 0L );

} /* WindowProc */

