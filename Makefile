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
THRIFTDIR=include/if

THRIFT_DIR := /usr/local/include/thrift
BOOST_DIR := /usr/local/include

LFLAGS=
IFLAGS= -I$(THRIFT_DIR) -I$(BOOST_DIR)
ifdef PREFIX
	LFLAGS += -L$(PREFIX)/lib
	IFLAGS += -I$(PREFIX)/include
endif

CFLAGS = -I./include $(OPT) $(IFLAGS)
CXXFLAGS = -std=c++1y -I./include -I./$(THRIFTDIR)/gen-cpp $(OPT) $(IFLAGS)
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

GENCPP=$(wildcard $(THRIFTDIR)/gen-cpp/*.cpp)

$(OUTDIR)/rpcserver: DIRS $(OUTDIR)/src/main.o $(GENCPP)
	$(CXX) $(LFLAGS) -pthread $(OUTDIR)/src/main.o $(GENCPP) -o $@ -lglog -ldeltafs -ldeltafs-common -lthrift

$(THRIFTDIR)/gen-cpp/%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OUTDIR)/%.o: %.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OUTDIR)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
