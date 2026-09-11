#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <time.h>
#include <string.h>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

#include "fssimplewindow.h"

static EMSCRIPTEN_WEBGL_CONTEXT_HANDLE glContext = 0;
static int fsKeyPress[FSKEY_NUM_KEYCODE];

#define NKEYBUF 256
static int keyBuffer[NKEYBUF];
static int nKeyBufUsed = 0;
static int charBuffer[NKEYBUF];
static int nCharBufUsed = 0;

static int mapDomCodeToFsKey(const char *code, const char *key)
{
	if (!code) return FSKEY_NULL;

	if (strcmp(code, "Space") == 0) return FSKEY_SPACE;
	if (strcmp(code, "Enter") == 0) return FSKEY_ENTER;
	if (strcmp(code, "Escape") == 0) return FSKEY_ESC;
	if (strcmp(code, "Backspace") == 0) return FSKEY_BS;
	if (strcmp(code, "Tab") == 0) return FSKEY_TAB;
	if (strcmp(code, "ArrowUp") == 0) return FSKEY_UP;
	if (strcmp(code, "ArrowDown") == 0) return FSKEY_DOWN;
	if (strcmp(code, "ArrowLeft") == 0) return FSKEY_LEFT;
	if (strcmp(code, "ArrowRight") == 0) return FSKEY_RIGHT;
	if (strcmp(code, "ShiftLeft") == 0 || strcmp(code, "ShiftRight") == 0) return FSKEY_SHIFT;
	if (strcmp(code, "ControlLeft") == 0 || strcmp(code, "ControlRight") == 0) return FSKEY_CTRL;
	if (strcmp(code, "AltLeft") == 0 || strcmp(code, "AltRight") == 0) return FSKEY_ALT;
	if (strcmp(code, "Delete") == 0) return FSKEY_DEL;
	if (strcmp(code, "Insert") == 0) return FSKEY_INS;
	if (strcmp(code, "Home") == 0) return FSKEY_HOME;
	if (strcmp(code, "End") == 0) return FSKEY_END;
	if (strcmp(code, "PageUp") == 0) return FSKEY_PAGEUP;
	if (strcmp(code, "PageDown") == 0) return FSKEY_PAGEDOWN;

	if (strncmp(code, "Key", 3) == 0 && code[3] != '\0' && code[4] == '\0') {
		char c = code[3];
		if (c >= 'A' && c <= 'Z') return FSKEY_A + (c - 'A');
	}

	if (strncmp(code, "Digit", 5) == 0 && code[5] >= '0' && code[5] <= '9') {
		return FSKEY_0 + (code[5] - '0');
	}

	if (strcmp(code, "F1") == 0) return FSKEY_F1;
	if (strcmp(code, "F2") == 0) return FSKEY_F2;
	if (strcmp(code, "F3") == 0) return FSKEY_F3;
	if (strcmp(code, "F4") == 0) return FSKEY_F4;
	if (strcmp(code, "F5") == 0) return FSKEY_F5;
	if (strcmp(code, "F6") == 0) return FSKEY_F6;
	if (strcmp(code, "F7") == 0) return FSKEY_F7;
	if (strcmp(code, "F8") == 0) return FSKEY_F8;
	if (strcmp(code, "F9") == 0) return FSKEY_F9;
	if (strcmp(code, "F10") == 0) return FSKEY_F10;
	if (strcmp(code, "F11") == 0) return FSKEY_F11;
	if (strcmp(code, "F12") == 0) return FSKEY_F12;

	return FSKEY_NULL;
}

static EM_BOOL keydown_callback(int eventType, const EmscriptenKeyboardEvent *keyEvent, void *userData)
{
	int fsKey = mapDomCodeToFsKey(keyEvent->code, keyEvent->key);
	if (fsKey != FSKEY_NULL) {
		fsKeyPress[fsKey] = 1;
		FsPushKey(fsKey);
		if (keyEvent->key[0] != '\0' && keyEvent->key[1] == '\0') {
			FsPushChar((unsigned char)keyEvent->key[0]);
		}
	}
	return EM_TRUE;
}

static EM_BOOL keyup_callback(int eventType, const EmscriptenKeyboardEvent *keyEvent, void *userData)
{
	int fsKey = mapDomCodeToFsKey(keyEvent->code, keyEvent->key);
	if (fsKey != FSKEY_NULL) {
		fsKeyPress[fsKey] = 0;
	}
	return EM_TRUE;
}

extern "C" void FsPollOneEvent(void)
{
	static bool callbacksSet = false;
	if (!callbacksSet) {
		emscripten_set_keydown_callback("#canvas", nullptr, EM_TRUE, keydown_callback);
		emscripten_set_keyup_callback("#canvas", nullptr, EM_TRUE, keyup_callback);
		callbacksSet = true;
	}
	if (glContext > 0) {
		emscripten_webgl_make_context_current(glContext);
	}
}

void FsOpenWindow(const FsOpenWindowOption &opt)
{
	EmscriptenWebGLContextAttributes attr;
	emscripten_webgl_init_context_attributes(&attr);
	attr.alpha = EM_TRUE;
	attr.depth = EM_TRUE;
	attr.stencil = EM_FALSE;
	attr.antialias = EM_TRUE;

	glContext = emscripten_webgl_create_context("#offscreen", &attr);
	if (glContext <= 0) {
		glContext = emscripten_webgl_create_context(0, &attr);
	}
	if (glContext > 0) {
		emscripten_webgl_make_context_current(glContext);
	}

	FsPollOneEvent();

	if (fsOpenGLInitializationCallBack) {
		(*fsOpenGLInitializationCallBack)(fsOpenGLInitializationCallBackParam);
	}
	if (fsAfterWindowCreationCallBack) {
		(*fsAfterWindowCreationCallBack)(fsAfterWindowCreationCallBackParam);
	}
}

void FsSwapBuffers(void)
{
	if (fsSwapBuffersHook) {
		(*fsSwapBuffersHook)(fsSwapBuffersHookParam);
	}
}

void FsCloseWindow(void)
{
	if (glContext > 0) {
		emscripten_webgl_destroy_context(glContext);
		glContext = 0;
	}
}

void FsMaximizeWindow(void) {}
void FsUnmaximizeWindow(void) {}
void FsMakeFullScreen(void) {}

void FsResizeWindow(int newWid, int newHei) {}
int FsCheckWindowOpen(void) { return (glContext > 0 ? 1 : 0); }

void FsGetWindowSize(int &wid, int &hei)
{
	wid = 800;
	hei = 600;
}

void FsGetWindowPosition(int &x0, int &y0)
{
	x0 = 0;
	y0 = 0;
}

void FsSetWindowTitle(const char windowTitle[]) {}

void FsPollDevice(void)
{
	FsPollOneEvent();
	if (fsPollDeviceHook) {
		(*fsPollDeviceHook)(fsPollDeviceHookParam);
	}
}

void FsPushOnPaintEvent(void) {}

void FsSleep(int ms) {}

long long int FsPassedTime(void)
{
	return 10;
}

long long int FsSubSecondTimer(void)
{
	return (long long int)(emscripten_get_now());
}

int FsInkey(void)
{
	if (nKeyBufUsed > 0) {
		int keyCode = keyBuffer[0];
		nKeyBufUsed--;
		for (int i = 0; i < nKeyBufUsed; i++) {
			keyBuffer[i] = keyBuffer[i + 1];
		}
		return keyCode;
	}
	return 0;
}

int FsInkeyChar(void)
{
	if (nCharBufUsed > 0) {
		int asciiCode = charBuffer[0];
		nCharBufUsed--;
		for (int i = 0; i < nCharBufUsed; i++) {
			charBuffer[i] = charBuffer[i + 1];
		}
		return asciiCode;
	}
	return 0;
}

void FsPushKey(int fskey)
{
	if (nKeyBufUsed < NKEYBUF) {
		keyBuffer[nKeyBufUsed++] = fskey;
	}
}

void FsPushChar(int c)
{
	if (nCharBufUsed < NKEYBUF) {
		charBuffer[nCharBufUsed++] = c;
	}
}

int FsGetKeyState(int fsKeyCode)
{
	if (fsKeyCode > 0 && fsKeyCode < FSKEY_NUM_KEYCODE) {
		return fsKeyPress[fsKeyCode];
	}
	return 0;
}

int FsCheckWindowExposure(void)
{
	return 0;
}

void FsGetMouseState(int &lb, int &mb, int &rb, int &mx, int &my)
{
	lb = 0; mb = 0; rb = 0; mx = 0; my = 0;
}

int FsGetMouseEvent(int &lb, int &mb, int &rb, int &mx, int &my)
{
	lb = 0; mb = 0; rb = 0; mx = 0; my = 0;
	return FSMOUSEEVENT_NONE;
}

void FsSetMousePosition(int mx, int my) {}

void FsChangeToProgramDir(void) {}

int FsGetNumCurrentTouch(void) { return 0; }
const FsVec2i *FsGetCurrentTouch(void) { return nullptr; }

int FsEnableIME(void) { return 0; }
void FsDisableIME(void) {}

int FsIsNativeTextInputAvailable(void) { return 0; }
int FsOpenNativeTextInput(int x1, int y1, int wid, int hei) { return 0; }
void FsCloseNativeTextInput(void) {}
void FsSetNativeTextInputText(const wchar_t []) {}
int FsGetNativeTextInputTextLength(void) { return 0; }
void FsGetNativeTextInputText(wchar_t str[], int bufLen) { if (bufLen > 0) str[0] = 0; }
int FsGetNativeTextInputEvent(void) { return FSNATIVETEXTEVENT_NONE; }

void FsShowMouseCursor(int showFlag) {}
int FsIsMouseCursorVisible(void) { return 1; }
