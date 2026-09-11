#include <iostream>
#include <string>
#include <vector>
#include <exception>
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

static std::vector<uint8_t> g_framebuffer;
static int g_fb_width = 640;
static int g_fb_height = 480;

static void wasm_main_loop(void)
{
	if (g_window && g_towns && g_outside_world && g_sound && g_townsThread) {
		g_window->Interval();

		// Run 1 frame (~16.6ms) of VM emulation per tick
		uint64_t frameEndTime = g_towns->state.townsTime + TOWNS_RENDERING_FREQUENCY;
		g_towns->debugger.ClearStopFlag();
		while (g_towns->state.townsTime < frameEndTime && 0 == g_towns->GetStopFlags()) {
			while (g_towns->state.townsTime <= g_towns->state.nextFastDevicePollingTime && 0 == g_towns->GetStopFlags()) {
				g_towns->RunOneInstruction();
				g_towns->pic.ProcessIRQ(g_towns->CPU(), g_towns->mem);
			}
			g_towns->RunScheduledTasks();
			g_towns->RunFastDevicePolling();
		}

		g_towns->ProcessSound(g_outside_world);
		g_towns->cdrom.UpdateCDDAState(g_towns->state.townsTime);
		g_outside_world->ProcessAppSpecific(*g_towns);

		if (g_towns->state.nextDevicePollingTime < g_towns->state.townsTime) {
			g_outside_world->UpdateStatusBarInfo(*g_towns);
			g_window->Communicate(g_outside_world);
			g_outside_world->DevicePolling(*g_towns);
			g_sound->Polling();
			g_towns->rex3586.Polling();
			g_towns->state.nextDevicePollingTime = g_towns->state.townsTime + FMTownsCommon::DEVICE_POLLING_INTERVAL;
		}

		static TownsRender render;
		g_towns->ForceRender(render, *g_outside_world, *g_window);
		g_window->Render(true);

		if (g_window->winThr.mostRecentImage.wid > 0 && g_window->winThr.mostRecentImage.hei > 0) {
			g_fb_width = g_window->winThr.mostRecentImage.wid;
			g_fb_height = g_window->winThr.mostRecentImage.hei;
			g_framebuffer = g_window->winThr.mostRecentImage.rgba;
			for (size_t i = 3; i < g_framebuffer.size(); i += 4) {
				g_framebuffer[i] = 255;
			}
		}
	}
}

extern "C" {

EMSCRIPTEN_KEEPALIVE uint8_t* tsugaru_get_framebuffer()
{
	if (g_framebuffer.empty()) {
		g_framebuffer.resize(g_fb_width * g_fb_height * 4, 0);
	}
	return g_framebuffer.data();
}

EMSCRIPTEN_KEEPALIVE int tsugaru_get_fb_width()
{
	return g_fb_width;
}

EMSCRIPTEN_KEEPALIVE int tsugaru_get_fb_height()
{
	return g_fb_height;
}

EMSCRIPTEN_KEEPALIVE void tsugaru_init()
{
	if (g_towns != nullptr) {
		return;
	}
	OSInit();
	auto *fs_conn = new FsSimpleWindowConnection;
	g_outside_world = fs_conn;
	g_sound = g_outside_world->CreateSound();
	g_window = g_outside_world->CreateWindowInterface();

	g_argv.autoStart = true;
	g_argv.ROMPath = "roms";
	try {
		g_towns = new FMTownsTemplate<i486DXDefaultFidelity>;
	} catch (const std::exception &e) {
		std::cout << "[INIT] Exception creating g_towns: " << e.what() << std::endl;
		return;
	} catch (...) {
		std::cout << "[INIT] Unknown exception creating g_towns" << std::endl;
		return;
	}

	if (FMTownsCommon::Setup(*g_towns, g_outside_world, g_window, g_argv)) {
		std::cout << "FMTownsCommon::Setup succeeded!" << std::endl;
		g_window->Start();
		g_townsThread = new TownsThread;
		g_townsThread->SetRunMode(TownsThread::RUNMODE_RUN);
		g_townsThread->SetReturnOnPause(true);
		g_townsThread->VMStart(g_towns, g_outside_world, nullptr);

		emscripten_set_main_loop(wasm_main_loop, 0, 0);
	} else {
		std::cout << "FMTownsCommon::Setup failed!" << std::endl;
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

EMSCRIPTEN_KEEPALIVE int main(int argc, char *argv[])
{
	tsugaru_init();
	return 0;
}

}
