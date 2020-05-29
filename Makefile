CFILES = $(shell find . -iname "*.c")
CXXFILES = $(shell find . -iname "*.cpp")

SRC = $(CFILES) $(CXXFILES)
OBJ = $(CFILES:%.c=%.o) $(CXXFILES:%.cpp=%.o)
DEP = $(OBJ:%.o=%.d)

CFLAGS = -O3

TARGETS = osmtegrastats tegraenergyperf

all: $(TARGETS)

osmtegrastats: osmtegrastats.o tegra_device_info.o

tegraenergyperf: tegraenergyperf.o tegra_device_info.o
	$(LINK.cc) $^ $(LOADLIBES) $(LDLIBS) -o $@

%.d: %.c
	$(CC) -MM $(CFLAGS) $< > $@

-include $(DEP)

clean:
	$(RM) $(TARGETS) $(OBJ) $(DEP)

.PHONY: all clean

