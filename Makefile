BUILD := build
CMAKE ?= cmake

.PHONY: all test clean

all: $(BUILD)/CMakeCache.txt
	$(CMAKE) --build $(BUILD) -j

# ninja is not installed on this machine; the Unix Makefiles generator is required.
$(BUILD)/CMakeCache.txt: CMakeLists.txt
	$(CMAKE) -S . -B $(BUILD) -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
	ln -sf $(BUILD)/compile_commands.json compile_commands.json

test: all
	./$(BUILD)/tb_tests

clean:
	rm -rf $(BUILD)

# --- web layer -------------------------------------------------------------
# build.sh sources ~/emsdk/emsdk_env.sh itself, so `make wasm` works from a
# shell that has never seen emsdk.
.PHONY: wasm js web-test demo dist-size

wasm:
	./build.sh

js:
	npx tsc -p tsconfig.json

dist-size:
	node tests/dist_size.mjs

demo: js
	npx vite --config demo/vite.config.ts

# ONE list of web tests, and it lives in package.json's "test:web" script.
# `make web-test` and `npm run test:web` must never be able to disagree about
# which suites ran, so this target delegates instead of restating them.
web-test: wasm js
	npm run test:web
