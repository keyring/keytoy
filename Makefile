#!makefile
incdir = -I/usr/local/include -I/usr/local/include/libdrm
libdir = -L/usr/local/lib
lib = -lgbm -lepoxy -lpng -ldrm
obj = keytoy

all:
	clang -std=c99 -g $(incdir) -o $(obj) main.c $(libdir) $(lib) 

clean:
	-rm -f *.o *.out $(obj)
