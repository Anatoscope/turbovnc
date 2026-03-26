include_directories(${X11_INCLUDE_DIR})

add_executable(vncconnect vncconnect.c
	${CMAKE_SOURCE_DIR}/unix/tvncconfig/vncExt.c)

target_link_libraries(vncconnect ${X11_LIBRARIES})

install(TARGETS vncconnect DESTINATION ${CMAKE_INSTALL_FULL_BINDIR})
strip_debug_info(vncconnect)

install(FILES vncconnect.man DESTINATION ${CMAKE_INSTALL_FULL_MANDIR}/man1
	RENAME vncconnect.1)
