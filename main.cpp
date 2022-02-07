#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <epoxy/gl.h>
#include <epoxy/egl.h>

#include <errno.h>
#include <sys/epoll.h>
#include <libudev.h>
#include <libinput.h>

#include "devices.h"

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_opengl3.h"

static int OpenRestricted(const char *path, int flags, void *user_data)
{
  int fd = open(path, flags);
  return fd < 0 ? -errno : fd;
}

static void CloseRestricted(int fd, void *user_data)
{
  close(fd);
}

const static struct libinput_interface input_interface = {
  .open_restricted = OpenRestricted,
  .close_restricted = CloseRestricted,
};

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
  ImGui::ShowDemoWindow(&show_demo_window);

  // Rendering
  ImGui::Render();

  glViewport(0, 0, canvas->width, canvas->height);
  glClearColor(0.45f, 0.55f, 0.6f, 1.f);
  glClear(GL_COLOR_BUFFER_BIT);

  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

}

int main(void)
{
  /*    input     */
  struct udev *udev = udev_new();
  struct libinput *li;
  struct libinput_event *li_event;
  struct libinput_event_keyboard *li_event_kb;
  libinput_event_type li_event_type = LIBINPUT_EVENT_NONE;
  uint32_t keycode = 0;

  li = libinput_udev_create_context(&input_interface, NULL, udev);
  libinput_udev_assign_seat(li, "seat0");

  int li_fd = libinput_get_fd(li);

  /*    input     */

  /*    epoll event     */
  struct epoll_event ep, ep_events[32];
  int epoll_fd = epoll_create(1);

  memset(&ep, 0, sizeof(ep));
  ep.events = EPOLLIN;
  ep.data.fd = li_fd;

  if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, li_fd, &ep) < 0){
    printf("epoll_ctl FAILED!\n");
  }

  /*    epoll event     */

  /*    render     */
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
  /*    render     */

  bool is_need_quit = false;
  int count = 0;
  int event_count = 0;

  // loop
  while(!is_need_quit) {

    event_count = epoll_wait(epoll_fd, ep_events, ARRAY_LENGTH(ep_events), 0);

    libinput_dispatch(li);
    while ((li_event = libinput_get_event(li))) {
      li_event_type = libinput_event_get_type(li_event);
      printf("event_type: %d\n", li_event_type);
      if (li_event_type == LIBINPUT_EVENT_DEVICE_ADDED) {
        struct libinput_device *dev = libinput_event_get_device(li_event);
        const char *name = libinput_device_get_name(dev);
        printf("Found Input Device: %s.\n", name);
      }
      else if (li_event_type == LIBINPUT_EVENT_KEYBOARD_KEY){
        if ( (li_event_kb = libinput_event_get_keyboard_event(li_event)) != NULL){
          keycode = libinput_event_keyboard_get_key(li_event_kb);
          printf("keycode: %d\n", keycode);
          if (count++ > 5){
            printf("count: %d\n", count);
            is_need_quit = true;
          }
        }

      }

      libinput_event_destroy(li_event);
    }

    Render(&render_context);
    SwapBuffer(&render_device, &render_context);
  }

  // end
  libinput_unref(li);
  close(epoll_fd);

  RestoreDefaultFramebuffer(&render_device);

  // Cleanup
  ImGui_ImplOpenGL3_Shutdown();

  ImGui::DestroyContext();

  return 0;
}
