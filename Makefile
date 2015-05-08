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

CFLAGS_DEV= -std=c99 -Wall -g -Wunused-parameter -Wredundant-decls  -Wreturn-type  -Wswitch-default -Wunused-value -Wimplicit  -Wimplicit-function-declaration  -Wimplicit-int -Wimport  -Wunused  -Wunused-function  -Wunused-label -Wno-int-to-pointer-cast -Wbad-function-cast  -Wmissing-declarations -Wmissing-prototypes  -Wnested-externs  -Wold-style-definition -Wstrict-prototypes -Wpointer-sign -Wextra -Wredundant-decls -Wunused -Wunused-function -Wunused-parameter -Wunused-value  -Wunused-variable -Wformat  -Wformat-nonliteral -Wparentheses -Wsequence-point -Wuninitialized -Wundef -Wbad-function-cast -Weverything -Wno-padded
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
	$(CC) -shared -fPIC $(CFLAGS) $(INCLUDES) $(LIBSRCS) -o $@
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
