# Host unit tests for firmware/components/logic.
#
#   make test   run suites whose module is implemented (CI runs this)
#   make spec   run every suite, including modules still to be written
#
# When you implement a module, move its suite from SPEC_ONLY to SUITES.

LOGIC   := ../../firmware/components/logic
CC      ?= gcc
CFLAGS  := -std=c11 -Wall -Wextra -Werror -Wconversion -g -I$(LOGIC)/include
BUILD   := build

SUITES    := tag_format
SPEC_ONLY := card_fsm lamp_model power_limit

SRCS := $(wildcard $(LOGIC)/*.c)

.PHONY: test spec clean
.SECONDARY:

test: $(addprefix run-,$(SUITES))
spec: $(addprefix run-,$(SUITES) $(SPEC_ONLY))

run-%: $(BUILD)/test_%
	@echo "== $*"
	@./$<

$(BUILD)/test_%: test_%.c test.h $(SRCS) | $(BUILD)
	$(CC) $(CFLAGS) -o $@ $< $(SRCS)

$(BUILD):
	mkdir -p $@

clean:
	rm -rf $(BUILD)
