.DEFAULT_GOAL := help
PROJECT_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))
.PHONY: help dev monitor build flash

help:
	@echo "make monitor (or make dev) - open Serial Monitor"
	@echo "make build                - build firmware"
	@echo "make flash                - build and upload firmware"

dev: monitor

monitor build flash:
	@bash "$(PROJECT_DIR)scripts/esp.sh" $@ $(if $(DRY_RUN),--dry-run,)
