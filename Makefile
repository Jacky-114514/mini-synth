# ============================================================
#  mini-synth —— 一个用来学合成器原理的最小项目
#  用法：
#     make            编译
#     make demo       生成演示曲 build/demo.wav 并播放
#     make clean      清理
# ============================================================

CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
CPPFLAGS += -Isrc

TARGET  := build/mini-synth
SOURCES := $(wildcard src/*.cpp src/*/*.cpp)
OBJECTS := $(SOURCES:src/%.cpp=build/obj/%.o)

.PHONY: all demo run clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@mkdir -p $(dir $@)
	$(CXX) $(OBJECTS) -o $@

build/obj/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# 生成演示曲，并在 Windows 上播放（WSL 内部没有声卡）
demo: $(TARGET)
	@mkdir -p build
	./$(TARGET) --demo --out build/demo.wav --play

# 直接跑一次演示曲
run: demo

clean:
	rm -rf build
