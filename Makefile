BUILD := build
CMAKE ?= cmake

.PHONY: all test clean

all: $(BUILD)/CMakeCache.txt
	$(CMAKE) --build $(BUILD) -j

$(BUILD)/CMakeCache.txt: CMakeLists.txt
	$(CMAKE) -S . -B $(BUILD) -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
	ln -sf $(BUILD)/compile_commands.json compile_commands.json

test: all
	./$(BUILD)/tb_tests

clean:
	rm -rf $(BUILD)

.PHONY: wasm js web-test demo dist-size

wasm:
	./build.sh

js:
	npx tsc -p tsconfig.json

dist-size:
	node tests/dist_size.mjs

demo: js
	npx vite --config demo/vite.config.ts

web-test: wasm js
	npm run test:web
