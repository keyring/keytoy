#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <epoxy/gl.h>
#include <epoxy/egl.h>

#include "devices.h"

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_opengl3.h"

static const char VERTEX_SHADER[] =
  "attribute vec3 positionIn;"
  "void main(){"
  "    gl_Position = vec4(positionIn, 1);"
  "}";

static const char FRAGMENT_SHADER[] =
  "precision mediump float;"
  "void main(){"
  "    gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);"
  "}";

static void InitGLES(canvas_t *canvas)
{
  GLint major = 0;
  GLint minor = 0;

  const char *gl_version = (const char *)glGetString(GL_VERSION);

  sscanf(gl_version, "OpenGL ES %d.%d ", &major, &minor);

  printf("GL version: %s\nmajor: %d, minor: %d\n", gl_version, major, minor);


}

static void NewFrame(canvas_t *canvas)
{
  ImGuiIO &io = ImGui::GetIO();
  io.DisplaySize = ImVec2((float)canvas->width, (float)canvas->height);
  io.DisplayFramebufferScale = ImVec2(1.f, 1.f);
  io.DeltaTime = 1.f / 60.f;
}

static void Render(canvas_t *canvas)
{
  // Our state
  bool show_demo_window = true;
  bool show_another_window = false;
  ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

  // Start the Dear ImGui frame
  ImGui_ImplOpenGL3_NewFrame();

  NewFrame(canvas);


  ImGui::NewFrame();


  // 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.

  //  ImGui::Begin("h");                          // Create a window called "Hello, world!" and append into it.

  ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)

  //ImGui::End();
  ImGui::EndFrame();

  // Rendering
  ImGui::Render();


  glViewport(0, 0, canvas->width, canvas->height);
  glClearColor(0.45f, 0.55f, 0.6f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT);

  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

  eglSwapBuffers(canvas->display, canvas->surface);

}

int main(void)
{
  device_t render_device;
  CreateRenderDevice(&render_device);

  canvas_t render_context;
  CreateRenderContext(&render_device, &render_context);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO(); (void)io;

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();

  ImGui_ImplOpenGL3_Init("#version 300 es");

  InitGLES(&render_context);

  Render(&render_context);

  OutputDisplay(&render_device);

  // Cleanup
  ImGui_ImplOpenGL3_Shutdown();

  ImGui::DestroyContext();

  return 0;
}
