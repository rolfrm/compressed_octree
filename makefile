OPT = -g3 -O4
LIB_SOURCES =  iron/mem.c iron/process.c iron/array.c iron/math.c iron/time.c  iron/log.c iron/fileio.c iron/linmath.c iron/test.c iron/error.c iron/image.c iron/gl.c main.c 
CC = gcc
TARGET = glitch
LIB_OBJECTS =$(LIB_SOURCES:.c=.o)
LDFLAGS= -L. $(OPT) -Wextra -fopenmp #-Wl,-stack_size,0x100000000 -lmcheck #-ftlo #setrlimit on linux 
LIBS= -ldl -lm -lGL -lpthread -lglfw -lGLEW -lpng
ALL= $(TARGET)
CFLAGS = -I. -std=c11 -c $(OPT) -Wall -Wextra -Werror=implicit-function-declaration -Wformat=0 -D_GNU_SOURCE -fdiagnostics-color -Wextra  -Wwrite-strings -fbounds-check -Werror -msse4.2 -mtune=corei7 -DSTB_IMAGE_IMPLEMENTATION   -fopenmp -ffast-math #-DDEBUG  

$(TARGET): $(LIB_OBJECTS)
	$(CC) $(LDFLAGS) $(LIB_OBJECTS) $(LIBS) -o $@

all: $(ALL)

.c.o: $(HEADERS)
	$(CC) $(CFLAGS) $< -o $@ -MMD -MF $@.depends 
depend: h-depend
clean:
	rm -f $(LIB_OBJECTS) $(ALL) *.o.depends

-include $(LIB_OBJECTS:.o=.o.depends)

