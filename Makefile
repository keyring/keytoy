
target = keytoy

CC = clang++

incdir = -I/usr/local/include -I/usr/local/include/libdrm -I/usr/local/include/libepoll-shim
libdir = -L/usr/local/lib
lib = -lgbm -lepoxy -ldrm -ludev -linput -lepoll-shim

project_root = .
external_root = $(project_root)/external
imgui_dir = $(external_root)/imgui

incdir += -I$(external_root)
incdir += -I$(imgui_dir) -I$(imgui_dir)/backends

src = main.cpp

src += $(imgui_dir)/imgui.cpp $(imgui_dir)/imgui_demo.cpp $(imgui_dir)/imgui_draw.cpp $(imgui_dir)/imgui_tables.cpp $(imgui_dir)/imgui_widgets.cpp
src += $(imgui_dir)/backends/imgui_impl_opengl3.cpp
objs = ${src:.cpp=.o}



$(target) : $(objs)
	$(CC) -o $@ $(objs) $(libdir) $(lib)

$(objs): $(src)
	$(CC) $(incdir) -c -o $@ $<

all: $(target)
	@echo Build complete: $(target)

tags:
	find . -name "*.c" -o -name "*.cpp" -o -name "*.h" -print | etags -f .tags -

clean:
	-rm -f $(target) $(objs)
