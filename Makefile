SRC_DIR= src
INC_DIR= src
OBJECT_FILES= libseeq.o
SOURCE_FILES= seeq.c seeq-main.c
HEADER_FILES= seeq.h
LIBSRC_FILES= libseeq.c
LIBHDR_FILES= libseeq.h seeqcore.h

OBJECTS= $(addprefix $(SRC_DIR)/,$(OBJECT_FILES))
SOURCES= $(addprefix $(SRC_DIR)/,$(SOURCE_FILES))
HEADERS= $(addprefix $(SRC_DIR)/,$(HEADER_FILES))
LIBSRCS= $(addprefix $(SRC_DIR)/,$(LIBSRC_FILES))
LIBHDRS= $(addprefix $(SRC_DIR)/,$(LIBHDR_FILES))
INCLUDES= $(addprefix -I, $(INC_DIR))

CFLAGS= -std=c99 -g -Wall -O3
LDLIBS=
#CC= gcc

all: seeq

lib: libseeq.so

libseeq.so: $(LIBSRCS) $(LIBHDRS)
	mkdir -p lib
	$(CC) -shared -fPIC $(CFLAGS) $(INCLUDES) $(LIBSRCS) -o lib/$@
	cp src/libseeq.h lib/libseeq.h

seeq: $(OBJECTS) $(SOURCES) $(LIBSRCS) $(HEADERS) $(LIBHDRS)
	$(CC) $(CFLAGS) $(SOURCES) $(OBJECTS) $(LDLIBS) -o $@

$(SRC_DIR)/%.o: $(SRC_DIR)/%.c $(SRC_DIR)/%.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@
clean:
	rm -f $(OBJECTS) seeq
	rm -rf lib
	rm -rf build
