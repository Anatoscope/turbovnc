if(EXISTS /dev/urandom)
	message(STATUS "Using /dev/urandom for random number generation")
	add_definitions("-DUseDevUrandom")
endif()

include_directories(${X11_INCLUDE_DIR})

add_executable(vncpasswd vncpasswd.c)

target_link_libraries(vncpasswd ${X11_LIBRARIES} vncauth)

install(TARGETS vncpasswd DESTINATION ${CMAKE_INSTALL_FULL_BINDIR})
strip_debug_info(vncpasswd)

install(FILES vncpasswd.man DESTINATION ${CMAKE_INSTALL_FULL_MANDIR}/man1
	RENAME vncpasswd.1)
