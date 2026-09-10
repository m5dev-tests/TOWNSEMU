#include <iostream>
#include <string>
#include <emscripten.h>

#include "towns.h"
#include "townsthread.h"
#include "townscommand.h"
#include "townsargv.h"
#include "fssimplewindow_connection.h"
#include "osinit.h"

static FMTownsTemplate<i486DXDefaultFidelity> *g_towns = nullptr;
static Outside_World *g_outside_world = nullptr;
static Outside_World::Sound *g_sound = nullptr;
static Outside_World::WindowInterface *g_window = nullptr;
static TownsThread *g_townsThread = nullptr;
static TownsARGV g_argv;

static void wasm_main_loop(void)
{
	if (g_window && g_towns && g_outside_world && g_sound && g_townsThread) {
		g_window->Interval();
		g_townsThread->VMMainLoop(g_towns, g_outside_world, g_sound, g_window, nullptr);
		g_window->Render(true);
	}
}

extern "C" {

EMSCRIPTEN_KEEPALIVE void tsugaru_init()
{
	OSInit();
	auto *fs_conn = new FsSimpleWindowConnection;
	g_outside_world = fs_conn;
	g_sound = g_outside_world->CreateSound();
	g_window = g_outside_world->CreateWindowInterface();

	g_argv.autoStart = true;
	g_towns = new FMTownsTemplate<i486DXDefaultFidelity>;

	if (FMTownsCommon::Setup(*g_towns, g_outside_world, g_window, g_argv)) {
		g_window->Start();
		g_townsThread = new TownsThread;
		g_townsThread->SetRunMode(TownsThread::RUNMODE_RUN);
		g_townsThread->SetReturnOnPause(true);
		g_townsThread->VMStart(g_towns, g_outside_world, nullptr);

		emscripten_set_main_loop(wasm_main_loop, 0, 1);
	}
}

EMSCRIPTEN_KEEPALIVE void tsugaru_insert_cd(const char *path)
{
	if (g_towns && path) {
		g_towns->cdrom.LoadDiscImage(path);
	}
}

EMSCRIPTEN_KEEPALIVE void tsugaru_key_event(int key, int down)
{
	if (down) {
		FsPushKey(key);
	}
}

}

int main(int argc, char *argv[])
{
	tsugaru_init();
	return 0;
}
