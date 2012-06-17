###############################################################################
# File: bsp.mak
#
# Purpose:
#   Compile Board Support Package routines for the Mac OS X desktop build
#
# History:
#
###############################################################################

# Subsystem produced by this makefile.
TARGET = bsp.o

#==============================================================================
# Object files required to build subsystem.
OBJS = bsp_start.o bsp_voltab.o bsp_timer.o  

#==============================================================================
# Source files required to build subsystem; used to generate dependencies.
SOURCES = bsp_start.c bsp_voltab.c bsp_timer.c 


