# MIT License
#
# Copyright (c) 2020, Friedt Professional Engineering Services, Inc
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

.PHONY: all clean check format

CFLAGS :=
CFLAGS += -Wall -Wextra -Werror
CFLAGS += -O0 -g

CXXFLAGS :=
CXXFLAGS += $(CFLAGS)

CFLAGS += -std=c11
CXXFLAGS += -std=c++14

GTEST_CFLAGS := $(shell pkg-config --cflags gtest)
GTEST_LIBS := $(shell pkg-config --libs gtest)
GMOCK_CFLAGS := $(shell pkg-config --cflags gmock)
GMOCK_LIBS := $(shell pkg-config --libs gmock gmock_main)
ABSL_CFLAGS := -I/usr/local/include
ABSL_LIBS := -Wl,--whole-archive $(shell find /usr/local/lib -name 'libabsl*.a') -Wl,--no-whole-archive

LDLIBS :=
LDLIBS += -lpthread

CSRC = $(shell find * -name '*.c')
CPPSRC = $(shell find * -name '*.cpp' | grep -v "test.cpp$$")
TSTSRC = $(shell find * -name '*-test.cpp')

COBJ = $(CSRC:.c=.o)
CPPOBJ = $(CPPSRC:.cpp=.o)
OBJ = $(COBJ) $(CPPOBJ)

HDR = $(shell find * -name '*.h')

TSTEXE := $(TSTSRC:.cpp=)

EXE :=
EXE += demo
EXE += $(TSTEXE)

LIB = libfoo.a

all: $(EXE)

$(LIB): $(filter-out main.o, $(COBJ) $(CPPOBJ))
	ar cr $@ $^

demo: $(LIB) Makefile
	$(CC) $(CFLAGS) $(LDFLAGS) main.c -o $@ $(LIB) $(LDLIBS)

%-test: %-test.cpp $(LIB) Makefile
	$(CXX) $(CXXFLAGS) $(GTEST_CFLAGS) $(GMOCK_CFLAGS) $(ABSL_CFLAGS) $(LDFLAGS) $< -o $@ $(LIB) $(LDLIBS) $(GTEST_LIBS) $(GMOCK_LIBS) $(ABSL_LIBS)

%.o: %.c $(HDR) Makefile
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(COBJ) $(EXE)

check: $(TSTEXE)
	if [ -z "$(TSTEXE)" ]; then \
		exit 0; \
	fi; \
	for t in $(TSTEXE); do \
		./$$t; \
	done

format:
	clang-format -i $(CSRC) $(CPPSRC) $(TSTSRC)
