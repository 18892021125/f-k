file (GLOB HEADERS "*.h")
file (GLOB SOURCES "[^_]*.cpp")

set(LIBRARY tex)
add_library(${LIBRARY} STATIC ${SOURCES} ${HEADERS})
add_dependencies(${LIBRARY} ext_rayint ext_eigen)
target_link_libraries(${LIBRARY} mrf ${MVE_MVE_LIBRARY} ${MVE_UTIL_LIBRARY} ${JPEG_LIBRARIES} ${PNG_LIBRARIES} ${TIFF_LIBRARIES})
install(TARGETS ${LIBRARY} ARCHIVE DESTINATION lib)
