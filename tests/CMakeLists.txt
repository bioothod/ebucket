add_executable(ebucket_bucket_processor_test bucket_processor_test.cpp)
target_link_libraries(ebucket_bucket_processor_test
	${Boost_LIBRARIES}
	${ELLIPTICS_LIBRARIES}
	${MSGPACK_LIBRARIES}
)
