add_executable(ebucket_simple simple.cpp)
target_link_libraries(ebucket_simple
	${Boost_LIBRARIES}
	${ELLIPTICS_LIBRARIES}
	${MSGPACK_LIBRARIES}
)
