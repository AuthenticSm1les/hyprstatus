ifeq ($(CXX),g++)
    EXTRA_FLAGS = --no-gnu-unique
else
    EXTRA_FLAGS =
endif

CXXFLAGS ?= -O2
CXXFLAGS += -fPIC -std=c++2b -Wno-c++11-narrowing
PKGS      = pixman-1 libdrm hyprland libinput libudev libsystemd wayland-server xkbcommon wireplumber-0.5
INCLUDES := $(shell pkg-config --cflags $(PKGS))
LIBS     := $(shell pkg-config --libs libudev libsystemd wireplumber-0.5)

SRC    = $(shell find src -name "*.cpp")
OBJ    = $(SRC:src/%.cpp=build/%.o)
DEP    = $(OBJ:.o=.d)
TARGET = hyprstatus.so

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) -shared $(LDFLAGS) $(OBJ) -o $@ $(LIBS)

build/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(EXTRA_FLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

-include $(DEP)

clean:
	rm -rf ./$(TARGET) build

.PHONY: all clean
