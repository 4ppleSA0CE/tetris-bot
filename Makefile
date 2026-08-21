BUILD := build
CMAKE ?= cmake

.PHONY: all test clean

all: $(BUILD)/CMakeCache.txt
	$(CMAKE) --build $(BUILD) -j

# ninja is not installed on this machine; the Unix Makefiles generator is required.
$(BUILD)/CMakeCache.txt: CMakeLists.txt
	$(CMAKE) -S . -B $(BUILD) -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=RelWithDebInfo

test: all
	./$(BUILD)/tb_tests

clean:
	rm -rf $(BUILD)
