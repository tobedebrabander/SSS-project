CC      := gcc
WARN    := -Wall -Wextra
STD     := -std=gnu11
DEBUG   := -g -O0

# Flags that keep the attacks from being masked by other mitigations.
# Do NOT use these for libguard itself.
ATTACK_HARDEN := -fno-stack-protector -no-pie -fcf-protection=none -rdynamic

# Flags the shadow stack relies on.
SS_FLAGS := -fno-omit-frame-pointer -fno-optimize-sibling-calls -falign-functions=16

CFLAGS  := $(STD) $(WARN) $(DEBUG) $(SS_FLAGS)
LDLIBS  := -ldl

BUILD   := build
LIB     := $(BUILD)/guard.o

# Benign tests: same output with and without Guard.
BENIGN  := callbacks
# Attack tests: hijack when plain, blocked when guarded.
ATTACK  := fptr_overwrite
#ATTACK  := fptr_overwrite ret_overwrite

.PHONY: all test clean
all: $(addprefix $(BUILD)/,$(BENIGN)) \
     $(addprefix $(BUILD)/,$(addsuffix .plain,$(ATTACK))) \
     $(addprefix $(BUILD)/,$(addsuffix .guard,$(ATTACK)))

$(BUILD):
	mkdir -p $(BUILD)

$(LIB): libguard/guard.c libguard/guard.h | $(BUILD)
	$(CC) $(CFLAGS) -Ilibguard -c $< -o $@

# Benign (guarded) test binaries.
$(BUILD)/%: testcases/%.c $(LIB) | $(BUILD)
	$(CC) $(CFLAGS) $(ATTACK_HARDEN) -Ilibguard -Itestcases $< $(LIB) -o $@ $(LDLIBS)

# Attack binaries, plain: macros disabled, no libguard logic -> hijack.
$(BUILD)/%.plain: testcases/%.c | $(BUILD)
	$(CC) $(CFLAGS) $(ATTACK_HARDEN) -DGUARD_DISABLE -Ilibguard -Itestcases \
	    $< libguard/guard.c -o $@ $(LDLIBS)

# Attack binaries, guarded.
$(BUILD)/%.guard: testcases/%.c $(LIB) | $(BUILD)
	$(CC) $(CFLAGS) $(ATTACK_HARDEN) -Ilibguard -Itestcases $< $(LIB) -o $@ $(LDLIBS)

test: all
	@./run_tests.sh

clean:
	rm -rf $(BUILD) *.guard.c
