TARGET = fake-08

RGB_LIB_DISTRIBUTION = ../../../..
RGB_INCDIR = $(RGB_LIB_DISTRIBUTION)/include
RGB_LIBDIR = $(RGB_LIB_DISTRIBUTION)/lib
RGB_LIBRARY_NAME = rgbmatrix
RGB_LIBRARY = $(RGB_LIBDIR)/lib$(RGB_LIBRARY_NAME).a

CXX ?= g++
CC = $(CXX)

CXXFLAGS = -O3 -Wall -Wextra -Wno-unused-parameter -std=c++17 -MMD -MP \
           -I$(RGB_INCDIR) \
           -I../../source \
           -I../../libs/z8lua \
           -I../../libs/lodepng \
           -I../../libs/simpleini \
           -I../../libs/miniz

CFLAGS = $(CXXFLAGS)

LDFLAGS += -L$(RGB_LIBDIR) -l$(RGB_LIBRARY_NAME) -lrt -lm -lpthread -lz

CORE_CPP_SOURCES = \
	../../source/Audio.cpp \
	../../source/cart.cpp \
	../../source/emojiconversion.cpp \
	../../source/filehelpers.cpp \
	../../source/filter.cpp \
	../../source/fontdata.cpp \
	../../source/graphics.cpp \
	../../source/hostCommonFunctions.cpp \
	../../source/Input.cpp \
	../../source/logger.cpp \
	../../source/mathhelpers.cpp \
	../../source/nibblehelpers.cpp \
	../../source/picoluaapi.cpp \
	../../source/printHelper.cpp \
	../../source/stringToDataHelpers.cpp \
	../../source/synth.cpp \
	../../source/vm.cpp \
	../../libs/lodepng/lodepng.cpp \
	source/RGBMatrixHost.cpp \
	source/main.cpp

CORE_C_SOURCES = \
	../../libs/miniz/miniz.c \
	../../libs/z8lua/lapi.c \
	../../libs/z8lua/lauxlib.c \
	../../libs/z8lua/lbaselib.c \
	../../libs/z8lua/lbitlib.c \
	../../libs/z8lua/lcode.c \
	../../libs/z8lua/lcorolib.c \
	../../libs/z8lua/lctype.c \
	../../libs/z8lua/ldblib.c \
	../../libs/z8lua/ldebug.c \
	../../libs/z8lua/ldo.c \
	../../libs/z8lua/ldump.c \
	../../libs/z8lua/lfunc.c \
	../../libs/z8lua/lgc.c \
	../../libs/z8lua/linit.c \
	../../libs/z8lua/liolib.c \
	../../libs/z8lua/llex.c \
	../../libs/z8lua/lmem.c \
	../../libs/z8lua/loadlib.c \
	../../libs/z8lua/lobject.c \
	../../libs/z8lua/lopcodes.c \
	../../libs/z8lua/loslib.c \
	../../libs/z8lua/lparser.c \
	../../libs/z8lua/lstate.c \
	../../libs/z8lua/lstring.c \
	../../libs/z8lua/lstrlib.c \
	../../libs/z8lua/ltable.c \
	../../libs/z8lua/ltablib.c \
	../../libs/z8lua/ltm.c \
	../../libs/z8lua/lundump.c \
	../../libs/z8lua/lvm.c \
	../../libs/z8lua/lzio.c

CPP_OBJECTS = $(CORE_CPP_SOURCES:.cpp=.o)
C_OBJECTS = $(CORE_C_SOURCES:.c=.o)
OBJECTS = $(CPP_OBJECTS) $(C_OBJECTS)
DEPS = $(OBJECTS:.o=.d)

all: $(TARGET)

$(RGB_LIBRARY): FORCE
	$(MAKE) -C $(RGB_LIBDIR)

$(TARGET): $(OBJECTS) $(RGB_LIBRARY)
	$(CXX) $(OBJECTS) -o $@ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(DEPS) $(TARGET)

-include $(DEPS)

FORCE:
.PHONY: all clean FORCE
