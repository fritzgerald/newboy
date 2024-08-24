# Set target, configuration, version and destination folders

PLATFORM := $(shell uname -s)
ifneq ($(findstring MINGW,$(PLATFORM)),)
PLATFORM := windows32
USE_WINDRES := true
endif

# the compiler for this program
CC = clang
LIBS = -I lib
ODIR = bin
FLAGS = -g3 -std=c++11 

NULL := /dev/null
ifeq ($(PLATFORM),windows32)
NULL := NUL
endif

all: directories gameboy

directories:
	mkdir -p $(ODIR)

OBJ := build

CORE_SOURCES := $(shell ls core/*.c)
CORE_OBJECTS := $(patsubst %,$(OBJ)/%.o,$(CORE_SOURCES))

COCOA_SOURCES := $(shell ls cocoa/*.m)
COCOA_OBJECTS := $(patsubst %,$(OBJ)/%.o,$(COCOA_SOURCES))

TESTS_SOURCES := $(shell ls tests/*.c)
TESTS_OBJECTS := $(patsubst %,$(OBJ)/%.o,$(TESTS_SOURCES))

SYSROOT := $(shell xcodebuild -sdk macosx -version Path 2> $(NULL))
CODESIGN := codesign -fs -

CFLAGS += -g
CFLAGS += -F/Library/Frameworks -mmacosx-version-min=14.0 -isysroot $(SYSROOT) -IAppleCommon -fobjc-arc -I./

LDFLAGS += -Wl -framework AppKit -framework QuartzCore -framework Metal -framework UniformTypeIdentifiers -mmacosx-version-min=14.0 -isysroot $(SYSROOT)

cocoaApp: $(ODIR)/NewBoy.app
$(ODIR)/NewBoy.app: $(ODIR)/NewBoy.app/Contents/MacOS/NewBoy \
					 cocoa/Info.plist
	cp cocoa/Info.plist $(ODIR)/NewBoy.app/Contents/Info.plist
	$(CODESIGN) $@


$(ODIR)/NewBoy.app/Contents/MacOS/NewBoy: $(CORE_OBJECTS) $(COCOA_OBJECTS)
	mkdir -p $(dir $@)
	$(CC) $^ -o $@ $(LDFLAGS)

tests: $(CORE_OBJECTS) $(TESTS_OBJECTS)
	mkdir -p $(ODIR)
	$(CC) $^ -o $(ODIR)/$@ $(LDFLAGS)

$(OBJ)/%.m.o: %.m
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ)/%.c.o: %.c
	mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ)
	rm -rf $(ODIR)