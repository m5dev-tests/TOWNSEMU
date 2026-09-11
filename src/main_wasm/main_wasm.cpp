#include <iostream>
#include <string>
#include <vector>
#include <exception>
#include <algorithm>
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

		g_towns->timer.TimerPolling(g_towns->state.townsTime);
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

		auto &img = g_window->winThr.mostRecentImage;
		std::cout << "[FRAME] Dimensions: " << img.wid << "x" << img.hei << std::endl;
		if (img.rgba.size() >= 4) {
			std::cout << "[FRAME] First pixel RGBA: ("
			          << (int)img.rgba[0] << ", "
			          << (int)img.rgba[1] << ", "
			          << (int)img.rgba[2] << ", "
			          << (int)img.rgba[3] << ")" << std::endl;
		}

		if (img.wid > 0 && img.hei > 0) {
			g_fb_width = img.wid;
			g_fb_height = img.hei;
		}

		size_t target_size = g_fb_width * g_fb_height * 4;
		if (g_framebuffer.size() != target_size) {
			g_framebuffer.resize(target_size, 0);
		}

		if (!img.rgba.empty()) {
			size_t copy_size = std::min(target_size, img.rgba.size());
			std::copy(img.rgba.begin(), img.rgba.begin() + copy_size, g_framebuffer.begin());
		}

		for (size_t i = 3; i < g_framebuffer.size(); i += 4) {
			g_framebuffer[i] = 255;
		}
	}
}

extern "C" {

EMSCRIPTEN_KEEPALIVE uint8_t* tsugaru_get_framebuffer()
{
	if (g_framebuffer.empty()) {
		g_framebuffer.resize(g_fb_width * g_fb_height * 4, 0);
		for (size_t i = 3; i < g_framebuffer.size(); i += 4) {
			g_framebuffer[i] = 255;
		}
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

EMSCRIPTEN_KEEPALIVE const char* tsugaru_get_debug_info()
{
	static std::string info;
	std::string runModeStr = "stopped";
	if (g_townsThread) {
		int mode = g_townsThread->GetRunMode();
		switch (mode) {
		case TownsThread::RUNMODE_RUN: runModeStr = "running"; break;
		case TownsThread::RUNMODE_PAUSE: runModeStr = "paused"; break;
		case TownsThread::RUNMODE_POWER_OFF: runModeStr = "power_off"; break;
		case TownsThread::RUNMODE_ONE_INSTRUCTION: runModeStr = "one_instruction"; break;
		case TownsThread::RUNMODE_EXIT: runModeStr = "exit"; break;
		default: runModeStr = "unknown"; break;
		}
	}

	std::string crtcStatus = "CRTC not initialized";
	if (g_towns) {
		crtcStatus = "HighRes: " + std::string(g_towns->crtc.state.highResCRTCEnabled ? "yes" : "no");
		crtcStatus += ", SinglePage: " + std::string(g_towns->crtc.InSinglePageMode() ? "yes" : "no");
		crtcStatus += ", ShowPage0: " + std::string(g_towns->crtc.state.ShowPage(0) ? "yes" : "no");
		crtcStatus += ", ShowPage1: " + std::string(g_towns->crtc.state.ShowPage(1) ? "yes" : "no");
		auto renderSize = g_towns->crtc.GetRenderSize();
		crtcStatus += ", Size: " + std::to_string(renderSize.x()) + "x" + std::to_string(renderSize.y());
	}

	bool videoInit = (g_towns != nullptr && g_window != nullptr && g_fb_width > 0 && g_fb_height > 0 && !g_framebuffer.empty());

	info = "VM Run Mode: " + runModeStr + "\n";
	info += "CRTC Status: " + crtcStatus + "\n";
	info += "Video Initialized: " + std::string(videoInit ? "yes" : "no") + "\n";
	info += "Framebuffer Size: " + std::to_string(g_fb_width) + "x" + std::to_string(g_fb_height) + "\n";
	if (g_framebuffer.size() >= 4) {
		info += "First Pixel RGBA: (" + std::to_string((int)g_framebuffer[0]) + ", "
		                              + std::to_string((int)g_framebuffer[1]) + ", "
		                              + std::to_string((int)g_framebuffer[2]) + ", "
		                              + std::to_string((int)g_framebuffer[3]) + ")\n";
	}

	return info.c_str();
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

EMSCRIPTEN_KEEPALIVE void tsugaru_restart_with_cd(const char *path)
{
	if (g_towns == nullptr) {
		tsugaru_init();
	}
	if (g_towns != nullptr) {
		if (path != nullptr && path[0] != '\0') {
			g_towns->cdrom.LoadDiscImage(path);
		}
		g_towns->Reset(BOOT_KEYCOMB_CD);
	}
}

EMSCRIPTEN_KEEPALIVE int main(int argc, char *argv[])
{
	tsugaru_init();
	return 0;
}

}
