project(ebucket)
cmake_minimum_required(VERSION 2.6)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x -W -Wall -Wextra -fstack-protector-all -g")

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/debian/changelog" DEBCHANGELOG)

# version string: 0.3.12
# ebucket major 0
# ebucket minor 3.12
string(REGEX MATCH "([0-9]+\\.[0-9]+\\.[0-9]+)" DEBFULLVERSION "${DEBCHANGELOG}")
string(REGEX MATCH "([0-9]+)" EBUCKET_VERSION_MAJOR "${DEBFULLVERSION}")
string(REGEX MATCH "([0-9]+\\.[0-9]+$)" EBUCKET_VERSION_MINOR "${DEBFULLVERSION}")

include(cmake/locate_library.cmake)

LOCATE_LIBRARY(MSGPACK "msgpack.hpp" "msgpack")
LOCATE_LIBRARY(THEVOID "thevoid/rapidjson/reader.h" "thevoid")

find_package(Boost COMPONENTS system thread program_options REQUIRED)
find_package(Elliptics REQUIRED)

include_directories(
	"${CMAKE_CURRENT_SOURCE_DIR}/include"
	${Boost_INCLUDE_DIRS}
	${ELLIPTICS_INCLUDE_DIRS}
	${MSGPACK_INCLUDE_DIRS}
	${THEVOID_INCLUDE_DIRS}
)

link_directories(
	${Boost_LIBRARY_DIRS}
	${ELLIPTICS_LIBRARY_DIRS}
	${MSGPACK_LIBRARY_DIRS}
)

add_subdirectory(example)
add_subdirectory(tests)

file(GLOB headers "${CMAKE_CURRENT_SOURCE_DIR}/include/ebucket/*.hpp")
install(FILES ${headers} DESTINATION include/ebucket)
