SRC_DIR= src
INC_DIR= src
OBJ_DIR= build
OBJ_DIR_DEV= build-dev
OBJECT_FILES= libseeq.o
SOURCE_FILES= seeq.c seeq-main.c
HEADER_FILES= seeq.h
LIBSRC_FILES= libseeq.c
LIBHDR_FILES= libseeq.h seeqcore.h

OBJECTS= $(addprefix $(OBJ_DIR)/,$(OBJECT_FILES))
OBJ_DEV= $(addprefix $(OBJ_DIR_DEV)/,$(OBJECT_FILES))
SOURCES= $(addprefix $(SRC_DIR)/,$(SOURCE_FILES))
HEADERS= $(addprefix $(SRC_DIR)/,$(HEADER_FILES))
LIBSRCS= $(addprefix $(SRC_DIR)/,$(LIBSRC_FILES))
LIBHDRS= $(addprefix $(SRC_DIR)/,$(LIBHDR_FILES))
INCLUDES= $(addprefix -I, $(INC_DIR))

CFLAGS_DEV= -std=c99 -Wall -g -Wunused-parameter -Wredundant-decls  -Wreturn-type  -Wswitch-default -Wunused-value -Wimplicit  -Wimplicit-function-declaration  -Wimplicit-int -Wimport  -Wunused  -Wunused-function  -Wunused-label -Wno-int-to-pointer-cast -Wbad-function-cast  -Wmissing-declarations -Wmissing-prototypes  -Wnested-externs  -Wold-style-definition -Wstrict-prototypes -Wpointer-sign -Wextra -Wredundant-decls -Wunused -Wunused-function -Wunused-parameter -Wunused-value  -Wunused-variable -Wformat  -Wformat-nonliteral -Wparentheses -Wsequence-point -Wuninitialized -Wundef -Wbad-function-cast -Wno-padded
CFLAGS= -std=c99 -Wall -O3
LDLIBS=
#CC= clang

all: seeq

dev: CC= clang
dev: CFLAGS= $(CFLAGS_DEV)
dev: seeq-dev

lib: lib/libseeq.so

lib/libseeq.so: $(LIBSRCS) $(LIBHDRS)
	mkdir -p lib
	$(CC) -shared -fPIC $(CFLAGS) $(INCLUDES) $(LIBSRCS) $(shell python3-config --includes) $(shell python3-config --libs) -o $@
	cp src/libseeq.h lib/libseeq.h

seeq: $(OBJECTS) $(SOURCES) $(LIBSRCS) $(HEADERS) $(LIBHDRS)
	$(CC) $(CFLAGS) $(SOURCES) $(OBJECTS) $(LDLIBS) -o $@

seeq-dev: $(OBJ_DEV) $(SOURCES) $(LIBSRCS) $(HEADERS) $(LIBHDRS)
	$(CC) $(CFLAGS) $(SOURCES) $(OBJ_DEV) $(LDLIBS) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(SRC_DIR)/%.h
	mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(OBJ_DIR_DEV)/%.o: $(SRC_DIR)/%.c $(SRC_DIR)/%.h
	mkdir -p $(OBJ_DIR_DEV)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean-dev: 
	rm -f  seeq-dev
	rm -rf build-dev

clean:
	rm -f  seeq
	rm -rf lib
	rm -rf build
	rm -f  seeq-dev
	rm -rf build-dev

# -------------------------
# Python packaging helpers
# -------------------------

.PHONY: sdist wheel dist wheels build-tools build-tools-cibw clean-dist release-create release-upload pypi-upload

PYTHON ?= python3

# Defaults for cibuildwheel. Override at invocation time if needed.
CIBW_BUILD ?= cp38-* cp39-* cp310-* cp311-* cp312-*
CIBW_SKIP ?= pp* *-musllinux*
CIBW_TEST_COMMAND ?= python -c 'import seeq; print(seeq.__version__)'

# Select Linux architectures:
# - Locally, default to the host's native arch to avoid Docker emulation issues
#   (which cause "exec format error" without QEMU/binfmt set up).
# - On CI (where `CI=true`), build both x86_64 and aarch64.
HOST_ARCH := $(shell uname -m)
ifeq ($(CI),true)
CIBW_ARCHS_LINUX ?= x86_64 aarch64
else
ifeq ($(HOST_ARCH),x86_64)
CIBW_ARCHS_LINUX ?= x86_64
else ifeq ($(HOST_ARCH),aarch64)
CIBW_ARCHS_LINUX ?= aarch64
else
CIBW_ARCHS_LINUX ?= auto
endif
endif

CIBW_ARCHS_MACOS ?= x86_64 arm64
CIBW_BEFORE_BUILD ?= pip install build

build-tools:
	$(PYTHON) -m pip install --upgrade pip setuptools wheel build

build-tools-cibw:
	$(PYTHON) -m pip install --upgrade cibuildwheel

sdist: build-tools
	$(PYTHON) -m build --sdist

wheel: build-tools
	$(PYTHON) -m build --wheel

dist: sdist wheel

wheels: build-tools-cibw
	CIBW_BUILD="$(CIBW_BUILD)" \
	CIBW_SKIP="$(CIBW_SKIP)" \
	CIBW_TEST_COMMAND="$(CIBW_TEST_COMMAND)" \
	CIBW_ARCHS_LINUX="$(CIBW_ARCHS_LINUX)" \
	CIBW_ARCHS_MACOS="$(CIBW_ARCHS_MACOS)" \
	CIBW_BEFORE_BUILD="$(CIBW_BEFORE_BUILD)" \
	$(PYTHON) -m cibuildwheel --output-dir wheelhouse

clean-dist:
	rm -rf dist wheelhouse

# -------------------------
# GitHub Releases helpers
# Requires GitHub CLI (`gh`) and authenticated user
# Usage:
#   make release-create TAG=v1.2          # creates release and uploads artifacts
#   make release-upload TAG=v1.2          # uploads artifacts to existing release
# -------------------------

TAG ?=

release-create:
	@command -v gh >/dev/null 2>&1 || { echo "Error: 'gh' (GitHub CLI) is required."; exit 1; }
	@test -n "$(TAG)" || { echo "Error: TAG is required. Example: make release-create TAG=v1.2"; exit 1; }
	@echo "Creating GitHub Release $(TAG) and uploading artifacts..."
	gh release create "$(TAG)" dist/*.tar.gz wheelhouse/*.whl -t "$(TAG)" -n "Seeq $(TAG)"

release-upload:
	@command -v gh >/dev/null 2>&1 || { echo "Error: 'gh' (GitHub CLI) is required."; exit 1; }
	@test -n "$(TAG)" || { echo "Error: TAG is required. Example: make release-upload TAG=v1.2"; exit 1; }
	@echo "Uploading artifacts to GitHub Release $(TAG)..."
	gh release upload "$(TAG)" dist/*.tar.gz wheelhouse/*.whl --clobber

# -------------------------
# PyPI upload helper (optional)
# Requires twine and configured credentials
# -------------------------

pypi-upload:
	$(PYTHON) -m pip install --upgrade twine
	$(PYTHON) -m twine upload dist/*.tar.gz wheelhouse/*.whl
