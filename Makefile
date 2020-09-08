CFILES = $(shell find . -iname "*.c")
CXXFILES = $(shell find . -iname "*.cpp")

SRC = $(CFILES) $(CXXFILES)
OBJ = $(CFILES:%.c=%.o) $(CXXFILES:%.cpp=%.o)
DEP = $(OBJ:%.o=%.d)

CFLAGS = -O3 
CXXFLAGS = $(CFLAGS) -std=c++11
LOADLIBES = -lpthread

TARGETS = pinpoint

all: $(TARGETS)

pinpoint: pinpoint.o tegra_device_info.o mcp_com.o
	$(LINK.cc) $^ $(LOADLIBES) $(LDLIBS) -o $@

%.d: %.c
	$(CC) -MM $(CFLAGS) $< > $@

-include $(DEP)

clean:
	$(RM) $(TARGETS) $(OBJ) $(DEP)

.PHONY: all clean

