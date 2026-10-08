cmake_minimum_required(VERSION 3.10)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
	if(DEFINED ENV{VITASDK})
		set(CMAKE_TOOLCHAIN_FILE
			"$ENV{VITASDK}/share/vita.toolchain.cmake"
			CACHE PATH "VitaSDK toolchain file")
	else()
		message(FATAL_ERROR "Set VITASDK or CMAKE_TOOLCHAIN_FILE")
	endif()
endif()

project(screenshot_noshard C)
include("${VITASDK}/share/vita.cmake" REQUIRED)

add_executable(screenshot_noshard
	main.c
)

set_target_properties(screenshot_noshard PROPERTIES
	C_STANDARD 11
	C_STANDARD_REQUIRED ON
	C_EXTENSIONS ON
)

target_compile_options(screenshot_noshard PRIVATE
	-Wall
	-Wextra
	-Werror
	-O2
	-fno-pic
	-fno-PIC
)

target_link_libraries(screenshot_noshard
	taihen_stub
	SceKernelModulemgr_stub
	SceIofilemgr_stub
	c
	SceLibKernel_stub
	gcc
)

set_target_properties(screenshot_noshard PROPERTIES
	LINK_FLAGS "-nostdlib -Wl,-q"
)

vita_create_self(screenshot_noshard.suprx screenshot_noshard
	UNSAFE
	CONFIG ${CMAKE_CURRENT_SOURCE_DIR}/screenshot_noshard.yml
)
