cmake_minimum_required(VERSION 3.25)
project(libmobile VERSION 0.2.0)

# https://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
set(lt_current  0)
set(lt_revision 0)
set(lt_age      0)
math(EXPR lt_soversion "${lt_current} - ${lt_age}")
set(lt_version "${lt_soversion}.${lt_current}.${lt_age}")

include(GNUInstallDirs)

set(CMAKE_C_STANDARD 11)
option(BUILD_SHARED_LIBS "Build using shared libraries" ON)
include(CMakeOptions.txt)

set(c_defs
    MOBILE_LIBCONF_USE)

# Variables for configure_file
set(prefix ${CMAKE_INSTALL_PREFIX})
set(exec_prefix ${CMAKE_INSTALL_PREFIX})
set(libdir ${CMAKE_INSTALL_FULL_LIBDIR})
set(includedir ${CMAKE_INSTALL_FULL_INCLUDEDIR})
set(PACKAGE ${CMAKE_PROJECT_NAME})
set(VERSION ${CMAKE_PROJECT_VERSION})

set(MOBILE_ENABLE_IMPL_WEAK ${LIBMOBILE_ENABLE_IMPL_WEAK})
set(MOBILE_ENABLE_NOALLOC ${LIBMOBILE_ENABLE_NOALLOC})
set(MOBILE_ENABLE_NO32BIT ${LIBMOBILE_ENABLE_NO32BIT})

configure_file(mobile_config.cmake.h.in mobile_config.h)
configure_file(libmobile.pc.in libmobile.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libmobile.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

set(sources
    atomic.h
    callback.c
    callback.h
    commands.c
    commands.h
    compat.h
    config.c
    config.h
    debug.c
    debug.h
    dns.c
    dns.h
    global.h
    inet_pton.c
    mobile.c
    mobile_data.h
    relay.c
    relay.h
    serial.c
    serial.h
    util.c
    util.h
)

set(headers
    mobile.h
    mobile_inet.h
)

add_library(mobile ${sources})
target_include_directories(mobile PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_compile_definitions(mobile PRIVATE ${c_defs})
set_target_properties(mobile PROPERTIES
    VERSION ${lt_version} SOVERSION ${lt_soversion})
install(TARGETS mobile DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES ${headers} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
