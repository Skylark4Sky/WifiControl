#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

VERBOSE = 1
PROJECT_NAME := GiSunLink
COMPONENT_ADD_LDFLAGS = -Wl,--Map=map.txt
COMPONENT_SOLUTION_PATH ?= $(abspath $(shell pwd)/)

include $(COMPONENT_SOLUTION_PATH)/components/component_conf.mk
include $(IDF_PATH)/make/project.mk


