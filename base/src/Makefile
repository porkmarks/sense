CXX      := -c++
CXXFLAGS := -pedantic-errors -Wall -Wextra -Werror -O3
LDFLAGS  := -L/usr/lib -lstdc++ -lm
BUILD    := build
OBJ_DIR  := $(BUILD)/objects
APP_DIR  := $(BUILD)/apps
TARGET   := base
INCLUDE  := -I../../base -I. -I../../sensor/firmware -I../../common/src/rapidjson/include

SRC      := ../src/main.cpp \
	    ../src/tests.cpp \
	    ../src/Scheduler.cpp \
	    ../src/Sensors.cpp \
	    ../src/Server.cpp \
	../../sensor/firmware/rfm22b.cpp \
	../../sensor/firmware/Sensor_Comms.cpp \
	../../sensor/firmware/Storage.cpp \
	../../sensor/firmware/CRC.cpp \
	../../common/src/rfm22b_spi.cpp

OBJECTS := $(SRC:%.cpp=$(OBJ_DIR)/%.o)

all: build $(APP_DIR)/$(TARGET)

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -o $@ -c $<

$(APP_DIR)/$(TARGET): $(OBJECTS)
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(INCLUDE) $(LDFLAGS) -o $(APP_DIR)/$(TARGET) $(OBJECTS)

.PHONY: all build clean

build:
	@mkdir -p $(APP_DIR)
	@mkdir -p $(OBJ_DIR)

clean:
	rm -rf build/*
