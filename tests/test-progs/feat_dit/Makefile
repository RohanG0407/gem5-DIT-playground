# FEAT_DIT Functional Tests Makefile

GEM5_ROOT = ../../..
GEM5_BIN = $(GEM5_ROOT)/build/ARM/gem5.opt
CONFIG = $(GEM5_ROOT)/configs/example/arm/fdp_neoverse_v2_binary.py
BINDIR = bin/arm/linux

TESTS = test_dit_enable test_dit_disable test_dit_read test_dit_toggle test_dit_compsimp

.PHONY: all build run-test clean

all: build

build:
	$(MAKE) -C src

run-test: build
	@echo "Running FEAT_DIT functional tests..."
	@passed=0; failed=0; \
	for test in $(TESTS); do \
		echo "----------------------------------------"; \
		echo "Running $$test..."; \
		$(GEM5_BIN) $(CONFIG) --binary $(BINDIR)/$$test > /dev/null 2>&1; \
		if [ $$? -eq 0 ]; then \
			echo "  PASS"; \
			passed=$$((passed + 1)); \
		else \
			echo "  FAIL"; \
			failed=$$((failed + 1)); \
		fi; \
	done; \
	echo "========================================"; \
	echo "Results: $$passed passed, $$failed failed"

clean:
	$(MAKE) -C src clean
