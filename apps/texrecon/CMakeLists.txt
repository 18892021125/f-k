file (GLOB HEADERS "*.h")
file (GLOB SOURCES "[^_]*.cpp")

set(BIN texrecon)

add_executable(${BIN} ${SOURCES} ${HEADERS})
target_link_libraries(${BIN} tex ${MVE_MVE_LIBRARY} ${MVE_UTIL_LIBRARY})
install(TARGETS ${BIN} RUNTIME DESTINATION bin)
