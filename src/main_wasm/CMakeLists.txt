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

set(EMSCRIPTEN_LINK_FLAGS "-s WASM=1 -s USE_SDL=2 -s LEGACY_GL_EMULATION=1 -s ALLOW_MEMORY_GROWTH=1 -s EXPORTED_FUNCTIONS='[\"_tsugaru_init\",\"_tsugaru_insert_cd\",\"_tsugaru_key_event\",\"_main\"]' -s EXPORTED_RUNTIME_METHODS='[\"ccall\",\"cwrap\"]'")

set_target_properties(${TARGET_NAME} PROPERTIES LINK_FLAGS "${EMSCRIPTEN_LINK_FLAGS}")
