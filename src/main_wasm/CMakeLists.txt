set(TARGET_NAME Tsugaru_WASM)

add_executable(${TARGET_NAME} main_wasm.cpp)

target_link_libraries(${TARGET_NAME}
	towns
	townscommand
	townsargv
	townsrender
	outside_world
	osinit
	fssimplewindow_connection
	yssimplesound
)

set(EMSCRIPTEN_LINK_FLAGS "-s WASM=1 -s USE_SDL=2 -s LEGACY_GL_EMULATION=1 -s ALLOW_MEMORY_GROWTH=1 -s INITIAL_MEMORY=256MB -s DISABLE_EXCEPTION_CATCHING=0 --preload-file ${CMAKE_SOURCE_DIR}/../roms@/roms -s EXPORTED_FUNCTIONS='[\"_tsugaru_init\",\"_tsugaru_insert_cd\",\"_tsugaru_key_event\",\"_tsugaru_get_framebuffer\",\"_tsugaru_get_fb_width\",\"_tsugaru_get_fb_height\",\"_tsugaru_restart_with_cd\",\"_main\"]' -s EXPORTED_RUNTIME_METHODS='[\"ccall\",\"cwrap\",\"FS\"]'")

set_target_properties(${TARGET_NAME} PROPERTIES LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS}")
