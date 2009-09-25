cmake_minimum_required(VERSION 2.6)

ADD_DEFINITIONS(-Wall -std=c99 -D_GNU_SOURCE)
OPTION(DEBUG "include debug symbol")

IF(DEBUG)
	ADD_DEFINITIONS(-ggdb3)
ELSE(DEBUG)
	ADD_DEFINITIONS(-DNDEBUG)
ENDIF(DEBUG)

FIND_LIBRARY(PNG png /lib /usr/lib /usr/local/lib)
IF(NOT PNG)
	MESSAGE(FATAL_ERROR "libpng has not been found")
ENDIF(NOT PNG)

FIND_LIBRARY(CRYPTO crypto /lib /usr/lib /usr/local/lib)
IF(NOT CRYPTO)
	MESSAGE(FATAL_ERROR "libcrypto has not been found")
ENDIF(NOT CRYPTO)

SET(sources hidim.c hidim.h)
SET(libs png crypto)

ADD_EXECUTABLE(hidim ${sources})
TARGET_LINK_LIBRARIES(hidim ${libs})
