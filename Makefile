#
# Copyright (c) 2016 Carnegie Mellon University.
# ---------------------------------------
# All rights reserved.
#
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file. See the AUTHORS file for names of contributors.

#-----------------------------------------------
# Uncomment exactly one of the lines labeled (A), (B), and (C) below
# to switch between compilation modes.

# (A) Production use (optimized mode)
# OPT ?= -O2
# (B) Debug mode, w/ full line-level debugging symbols
OPT ?= -g2
# (C) Profiling mode: opt, but w/debugging symbols
# OPT ?= -O2 -g2
#-----------------------------------------------

OUTDIR=build

LFLAGS=
IFLAGS=
ifdef PREFIX
	LFLAGS += -L$(PREFIX)/lib
	IFLAGS += -I$(PREFIX)/include
endif

CFLAGS = -I./include $(OPT) $(IFLAGS)
CXXFLAGS = -std=c++1y -I./include $(OPT) $(IFLAGS)
CXX=g++
CC=gcc

default: all

all: $(OUTDIR)/rpcserver

clean:
	-rm -rf $(OUTDIR)

$(OUTDIR):
	mkdir -p $@

$(OUTDIR)/src: | $(OUTDIR)
	mkdir -p $@

.PHONY: DIRS
DIRS: $(OUTDIR)/src

$(OUTDIR)/rpcserver: DIRS $(OUTDIR)/src/main.o
	$(CXX) $(LFLAGS) -pthread $(OUTDIR)/src/main.o -o $@ -lglog -ldeltafs -ldeltafs-common

$(OUTDIR)/%.o: %.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OUTDIR)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
