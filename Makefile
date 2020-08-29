CFILES = $(shell find . -iname "*.c")
CXXFILES = $(shell find . -iname "*.cpp")

SRC = $(CFILES) $(CXXFILES)
OBJ = $(CFILES:%.c=%.o) $(CXXFILES:%.cpp=%.o)
DEP = $(OBJ:%.o=%.d)

CFLAGS = -O3 
CXXFLAGS = $(CFLAGS) -std=c++11
LOADLIBES = -lpthread

TARGETS = osmtegrastats pinpoint

all: $(TARGETS)

osmtegrastats: osmtegrastats.o tegra_device_info.o

pinpoint: pinpoint.o tegra_device_info.o
	$(LINK.cc) $^ $(LOADLIBES) $(LDLIBS) -o $@

%.d: %.c
	$(CC) -MM $(CFLAGS) $< > $@

-include $(DEP)

clean:
	$(RM) $(TARGETS) $(OBJ) $(DEP)

.PHONY: all clean

