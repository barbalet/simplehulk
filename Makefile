CC ?= cc
AR ?= ar
CPPFLAGS ?= -Isrc/c-core
CFLAGS ?= -std=c99 -O2 -Wall -Wextra -Wpedantic -Werror
BUILD ?= build
CORE := $(BUILD)/rules.o $(BUILD)/scenarios.o
.PHONY: all smoke scenarios test check rebuild-test sanitize clean wasm wasm-test serve
all: $(BUILD)/libsimplehulk.a $(BUILD)/c-test $(BUILD)/simple-hulk
$(BUILD):
	mkdir -p $(BUILD)
$(BUILD)/rules.o: src/c-core/rules.c src/c-core/simple_hulk.h src/c-core/internal.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
$(BUILD)/scenarios.o: src/c-core/scenarios.c src/c-core/simple_hulk.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
$(BUILD)/libsimplehulk.a: $(CORE)
	$(AR) rcs $@ $^
$(BUILD)/c-test: src/c-test/main.c src/c-test/smoke.c src/c-test/runner.c src/c-test/test.h $(BUILD)/libsimplehulk.a
	$(CC) $(CPPFLAGS) $(CFLAGS) src/c-test/main.c src/c-test/smoke.c src/c-test/runner.c $(BUILD)/libsimplehulk.a -o $@
$(BUILD)/simple-hulk: src/c-core/api_cli.c $(BUILD)/libsimplehulk.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $< $(BUILD)/libsimplehulk.a -o $@
smoke: all
	./$(BUILD)/c-test --smoke
	printf 'status\nquit\n' | ./$(BUILD)/simple-hulk 0 42
scenarios: all
	./$(BUILD)/c-test --scenarios
# First smoke-test the current build; then rebuild every source and play all missions.
test: smoke
	$(MAKE) --no-print-directory -B all
	./$(BUILD)/c-test --scenarios
check: test
rebuild-test:
	$(MAKE) --no-print-directory clean
	$(MAKE) --no-print-directory test
sanitize:
	$(MAKE) --no-print-directory BUILD=build/sanitize clean
	$(MAKE) --no-print-directory BUILD=build/sanitize CFLAGS='-std=c99 -O1 -g -Wall -Wextra -Wpedantic -Werror -fsanitize=address,undefined -fno-omit-frame-pointer' test
clean:
	rm -rf $(BUILD)
wasm:
	./scripts/build_wasm.sh
$(BUILD)/wasm-parity: src/c-test/wasm-parity.c src/wasm/bridge.c $(BUILD)/libsimplehulk.a
	$(CC) $(CPPFLAGS) $(CFLAGS) src/c-test/wasm-parity.c src/wasm/bridge.c $(BUILD)/libsimplehulk.a -o $@
wasm-test: wasm $(BUILD)/wasm-parity
	node src/c-test/wasm-test.mjs
serve:
	python3 -m http.server 8000
