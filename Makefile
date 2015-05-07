SRC_DIR= src
INC_DIR= src
OBJ_DIR= build
OBJECT_FILES= libseeq.o
SOURCE_FILES= seeq.c seeq-main.c
HEADER_FILES= seeq.h
LIBSRC_FILES= libseeq.c
LIBHDR_FILES= libseeq.h seeqcore.h

OBJECTS= $(addprefix $(OBJ_DIR)/,$(OBJECT_FILES))
SOURCES= $(addprefix $(SRC_DIR)/,$(SOURCE_FILES))
HEADERS= $(addprefix $(SRC_DIR)/,$(HEADER_FILES))
LIBSRCS= $(addprefix $(SRC_DIR)/,$(LIBSRC_FILES))
LIBHDRS= $(addprefix $(SRC_DIR)/,$(LIBHDR_FILES))
INCLUDES= $(addprefix -I, $(INC_DIR))

CFLAGS= -std=c99 -Wall -g -O3 -Wunused-parameter -Wredundant-decls  -Wreturn-type  -Wswitch-default -Wunused-value -Wimplicit  -Wimplicit-function-declaration  -Wimplicit-int -Wimport  -Wunused  -Wunused-function  -Wunused-label -Wno-int-to-pointer-cast -Wbad-function-cast  -Wmissing-declarations -Wmissing-prototypes  -Wnested-externs  -Wold-style-definition -Wstrict-prototypes -Wpointer-sign -Wextra -Wredundant-decls -Wunused -Wunused-function -Wunused-parameter -Wunused-value  -Wunused-variable -Wformat  -Wformat-nonliteral -Wparentheses -Wsequence-point -Wuninitialized -Wundef -Wbad-function-cast
LDLIBS=
#CC= gcc

all: seeq

lib: lib/libseeq.so

lib/libseeq.so: $(LIBSRCS) $(LIBHDRS)
	mkdir -p lib
	$(CC) -shared -fPIC $(CFLAGS) $(INCLUDES) $(LIBSRCS) -o $@
	cp src/libseeq.h lib/libseeq.h

seeq: $(OBJECTS) $(SOURCES) $(LIBSRCS) $(HEADERS) $(LIBHDRS)
	$(CC) $(CFLAGS) $(SOURCES) $(OBJECTS) $(LDLIBS) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(SRC_DIR)/%.h
	mkdir -p build
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@
clean:
	rm -f  seeq
	rm -rf lib
	rm -rf build
