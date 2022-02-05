#!makefile
CC = clang

incdir = -I/usr/local/include -I/usr/local/include/libdrm
libdir = -L/usr/local/lib
lib = -lgbm -lepoxy -ldrm

incdir += -I../..
src = main.c
objs = main.o
target = example_triangle

$(target) : $(objs)
	$(CC) -o $@ $(objs) $(libdir) $(lib)

$(objs): $(src)
	$(CC) $(incdir) -c -o $@ $<

all: $(target)
	@echo Build complete: $(target)

clean:
	-rm -f $(target) $(objs)
