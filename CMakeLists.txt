CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
CMAKE_POLICY(VERSION 2.6)

PROJECT(MANASERV)

IF (NOT VERSION)
    SET(VERSION 0.1.0.0)
ENDIF()

# where to look for cmake modules
SET(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/CMake/Modules)

OPTION(WITH_SQLITE "Enable Sqlite support (used by default)" ON)
OPTION(WITH_MYSQL "Enable MySQL support" OFF)
OPTION(ENABLE_LUA "Enable Lua scripting support" ON)

# Exclude Sqlite support if the MySQL support was asked.
IF(WITH_MYSQL)
    SET(WITH_SQLITE OFF)
    SET(WITH_POSTGRESQL OFF)
ENDIF()

IF(WITH_POSTGRESQL)
    MESSAGE(STATUS "Warning: PostgreSQL support will likely not work...")
    SET(WITH_SQLITE OFF)
    SET(WITH_MYSQL OFF)
ENDIF()

IF(WITH_SQLITE)
    SET(WITH_MYSQL OFF)
    SET(WITH_POSTGRESQL OFF)
ENDIF()

IF (WIN32)
    SET(PKG_DATADIR ".")
    SET(PKG_BINDIR ".")
    SET(LOCALEDIR ".")
    STRING(REPLACE "." " " _VERSION ${VERSION})
    SEPARATE_ARGUMENTS(_VERSION)
    LIST(LENGTH _VERSION _LEN)
    IF(NOT _LEN EQUAL 4)
        MESSAGE(FATAL_ERROR "Version needs to be in the form MAJOR.MINOR.RELEASE.BUILD")
    ENDIF()
    LIST(GET _VERSION 0 VER_MAJOR)
    LIST(GET _VERSION 1 VER_MINOR)
    LIST(GET _VERSION 2 VER_RELEASE)
    LIST(GET _VERSION 3 VER_BUILD)
    CONFIGURE_FILE(src/winver.h.in src/winver.h)
ELSE (WIN32)
    SET(PKG_DATADIR ${CMAKE_INSTALL_PREFIX}/share/manaserv)
    SET(LOCALEDIR ${CMAKE_INSTALL_PREFIX}/share/locale)
    SET(PKG_BINDIR ${CMAKE_INSTALL_PREFIX}/bin)
ENDIF (WIN32)

ADD_SUBDIRECTORY(libs/enet)
ADD_SUBDIRECTORY(src)
