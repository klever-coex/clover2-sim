.PHONY: help clean build
.DEFAULT_GOAL := help
.ONESHELL:

-include .env
export

PROJECT_DIR ?= $(shell pwd)
WORKSPACE_DIR ?= $(PROJECT_DIR)/sim_ws

## Show this help message
help:
	@printf "Available targets:\n\n"
	@awk '/^[a-zA-Z\-_0-9%:\\]+/ { \
		helpMessage = match(lastLine, /^## (.*)/); \
		if (helpMessage) { \
		helpCommand = $$1; \
		helpMessage = substr(lastLine, RSTART + 3, RLENGTH); \
		gsub("\\\\", "", helpCommand); \
		gsub(":+$$", "", helpCommand); \
		printf "  \x1b[32;01m%-35s\x1b[0m %s\n", helpCommand, helpMessage; \
		} \
	} \
	{ lastLine = $$0 }' $(MAKEFILE_LIST) | sort -u
	@printf "\n"

## Install repos
init-repos:
	vcs import $(WORKSPACE_DIR)/src/third_party < $(WORKSPACE_DIR)/src/third_party/repos.yaml

## Recursive submodule init
init-git:
	git submodule update --init --recursive

## Install project dependency
init: init-git init-repos

## Build simulation workspace
build:
	cd $(WORKSPACE_DIR)
	colcon build --symlink-install --cmake-args -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

## Cleanup build artifacts
clean:
	rm -rf $(WORKSPACE_DIR)/build $(WORKSPACE_DIR)/log $(WORKSPACE_DIR)/install
