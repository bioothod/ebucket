add_executable(ebucket_server server.cpp)
target_link_libraries(ebucket_server
	${Boost_LIBRARIES}
	${ELLIPTICS_LIBRARIES}
	${MSGPACK_LIBRARIES}
	${THEVOID_LIBRARIES}
	${SWARM_LIBRARIES}
)

install(TARGETS ebucket_server
	RUNTIME DESTINATION bin
	COMPONENT runtime
)
