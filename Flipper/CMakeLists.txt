cmake_minimum_required(VERSION 3.13)
project(wendigo_flipper C)

set(CMAKE_C_STANDARD 11)

file(GLOB SOURCES
	"scenes/*.c"
	"*.c"
)

add_executable(wendigo_flipper ${SOURCES})

target_include_directories(wendigo_flipper PRIVATE
	${CMAKE_SOURCES_DIR}
	${CMAKE_SOURCES_DIR}/scenes
	/home/chris/Documents/Microcontrollers/flipper/Momentum_Firmware/applications/services/gui
	/home/chris/Documents/Microcontrollers/flipper/Momentum_Firmware/lib
	/home/chris/Documents/Microcontrollers/flipper/Momentum_Firmware/lib/gui
	/home/chris/Documents/Microcontrollers/flipper/Momentum_Firmware/lib/scene
)
