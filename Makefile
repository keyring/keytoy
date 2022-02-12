
target = keytoy

CC = clang
CXX = clang++

incdir = -I/usr/local/include -I/usr/local/include/libdrm -I/usr/local/include/libepoll-shim
libdir = -L/usr/local/lib
lib = -lgbm -lepoxy -ldrm -ludev -linput -lepoll-shim

project_root = .
external_root = $(project_root)/external
imgui_dir = $(external_root)/imgui

incdir += -I$(external_root)
incdir += -I$(external_root)/glm
incdir += -I$(imgui_dir) -I$(imgui_dir)/backends

src = main.cpp

src += $(imgui_dir)/imgui.cpp $(imgui_dir)/imgui_demo.cpp $(imgui_dir)/imgui_draw.cpp $(imgui_dir)/imgui_tables.cpp $(imgui_dir)/imgui_widgets.cpp
src += $(imgui_dir)/backends/imgui_impl_opengl3.cpp
src_c += $(external_root)/xcursor/xcursor.c $(external_root)/xcursor/wlr_xcursor.c

objs = ${src:.cpp=.o}
objs_c += ${src_c:.c=.o}



$(target) : $(objs) $(objs_c)
	$(CXX) -o $@ $(objs) $(objs_c) $(libdir) $(lib)

$(objs): $(src)
	$(CXX) $(incdir) -c -o $@ $<

$(objs_c): $(src_c)
	$(CC) $(incdir) -c -o $@ $<


all: $(target)
	@echo Build complete: $(target)

tags:
	find . -name "*.c" -o -name "*.cpp" -o -name "*.h" -o -name "*.hpp" -print | etags -f .tags -

clean:
	-rm -f $(target) $(objs) $(objs_c)
