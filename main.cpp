#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <epoxy/gl.h>
#include <epoxy/egl.h>

#include <errno.h>
#include <sys/epoll.h>
#include <libudev.h>
#include <libinput.h>
#include <linux/input.h>
#include <iostream>

#include "devices.h"
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_opengl3.h"

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "glm/gtc/type_ptr.hpp"

#ifdef __cplusplus
extern "C" {
#endif

#include "xcursor/wlr_xcursor.h"

#ifdef __cplusplus
}
#endif

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

/* opengl origin is bottom-left */
static const GLfloat V2[] = {
  0.f, 0.f,  // bottom-left
  1.f, 0.f,  // bottom-right
  0.f, 1.f,  // top-left
  1.f, 1.f,  // top-right
};

static const GLfloat V3[] = {
  0.f, 0.f, 0.f,  // bottom-left
  1.f, 0.f, 0.f,  // bottom-right
  0.f, 1.f, 0.f,  // top-left
  1.f, 1.f, 0.f,  // top-right
};

/* image data origin is top-left (difference from opengl is Y-axis)*/
static const GLfloat T2[] = {
  0.f, 1.f,  // bottom-left
  1.f, 1.f,  // bottom-right
  0.f, 0.f,  // top-left
  1.f, 0.f,  // top-right
};

static const GLuint QUAD_VERTEX_NUM = 4;

static const char VERTEX_SHADER[] =
  "uniform mat4 mvp;"
  "attribute vec3 a_position;"
  "attribute vec2 a_texcoord;"
  "varying vec2 v_texcoord;"
  "void main(){"
  "    gl_Position = mvp * vec4(a_position, 1.0);"
  "    v_texcoord = a_texcoord;"
  "}";

static const char FRAGMENT_SHADER[] =
  "precision mediump float;"
  "varying vec2 v_texcoord;"
  "uniform sampler2D s_texture;"
  "void main(){"
  "    gl_FragColor = texture2D(s_texture, v_texcoord);"
  "}";


static void CreateTexture(GLuint *texture_id, GLint width, GLint height,  GLubyte *data)
{

  glGenTextures(1, texture_id);

  glBindTexture(GL_TEXTURE_2D, *texture_id);

  // set filtering mode
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  // set wrap mode
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

  // load image data
  if (!data) {

    printf("failed to load image. use pixels replace\n");

    GLubyte pixels[4 * 4] = {
      255,    0,    0,   255, // red
      0,    255,    0,   255, // green
      0,      0,  255,   255, // blue
      255,  255,    0,   255, // yellow
    };

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    data = pixels;
    width = 2;
    height = 2;
  }
  // upload texture data
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
  glGenerateMipmap(GL_TEXTURE_2D);
}

static GLuint LoadShader(const char *source, GLenum type)
{
  GLuint shader;
  GLint compiled;

  shader = glCreateShader(type);
  glShaderSource(shader, 1, &source, NULL);
  glCompileShader(shader);

  glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
  if (!compiled) {
    GLint infolen = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infolen);
    if (infolen > 1) {
      char *infolog = (char *)malloc(infolen);
      glGetShaderInfoLog(shader, infolen, NULL, infolog);
      fprintf(stderr, "Error compiling shader:\n %s \n", infolog);
      free(infolog);
    }
    glDeleteShader(shader);
    return 0;
  }

  return shader;
}

static void CreateProgram(canvas_t *canvas)
{
  GLint linked;
  GLuint vertex_shader;
  GLuint fragment_shader;
  assert((vertex_shader = LoadShader(VERTEX_SHADER, GL_VERTEX_SHADER)) != 0);
  assert((fragment_shader = LoadShader(FRAGMENT_SHADER, GL_FRAGMENT_SHADER)) != 0);
  assert((canvas->program = glCreateProgram()) != 0);
  glAttachShader(canvas->program, vertex_shader);
  glAttachShader(canvas->program, fragment_shader);
  glLinkProgram(canvas->program);
  glGetProgramiv(canvas->program, GL_LINK_STATUS, &linked);

  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);

  if (!linked) {
    GLint infolen = 0;
    glGetProgramiv(canvas->program, GL_INFO_LOG_LENGTH, &infolen);
    if (infolen > 1){
      char *infolog = (char *)malloc(infolen);
      glGetProgramInfoLog(canvas->program, infolen, NULL, infolog);
      fprintf(stderr, "Error linking program:\n %s \n", infolog);
      free(infolog);
    }
    glDeleteProgram(canvas->program);
    exit(1);
  }
}

static void InitGLES(canvas_t *canvas)
{
  GLint major = 0;
  GLint minor = 0;

  const char *gl_version = (const char *)glGetString(GL_VERSION);

  sscanf(gl_version, "OpenGL ES %d.%d ", &major, &minor);

  printf("GL version: %s\nmajor: %d, minor: %d\n", gl_version, major, minor);

  glViewport(0, 0, canvas->width, canvas->height);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  glClearColor(0.45f, 0.55f, 0.6f, 1.f);
}

static void NewFrame(canvas_t *canvas)
{
  ImGuiIO &io = ImGui::GetIO();
  io.DisplaySize = ImVec2((float)canvas->width, (float)canvas->height);
  io.DisplayFramebufferScale = ImVec2(1.f, 1.f);
  io.DeltaTime = 1.f / 60.f;
}

static void RenderCursor(canvas_t *canvas, wlr_xcursor_image *cursor_image, double posx, double posy)
{
  glUseProgram(canvas->program);

  glm::mat4 projection = glm::ortho(0.f, 1.f*canvas->width, 0.f, 1.f*canvas->height, -1.f, 1.f);
  glm::mat4 model = glm::mat4(1.f);
  model = glm::translate(model, glm::vec3(posx, 1.0*canvas->height - posy - cursor_image->height, 0.f));

  //  model = glm::translate(model, glm::vec3(0.5f*48, 0.5f*48, 0.f));

  model = glm::scale(model, glm::vec3(cursor_image->width, cursor_image->height, 1.f));

  glm::mat4 mvp = projection * model;

  glUniformMatrix4fv(glGetUniformLocation(canvas->program, "mvp"), 1, GL_FALSE, glm::value_ptr(mvp));
 
  GLint position = glGetAttribLocation(canvas->program, "a_position");
  glEnableVertexAttribArray(position);
  glVertexAttribPointer(position, 3, GL_FLOAT, GL_FALSE, 0, (void *)V3);

  GLint texcoord = glGetAttribLocation(canvas->program, "a_texcoord");
  glEnableVertexAttribArray(texcoord);
  glVertexAttribPointer(texcoord, 2, GL_FLOAT, GL_FALSE, 0, (void *)T2);


  glEnable(GL_BLEND);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, canvas->texture_id);
  glUniform1i(glGetUniformLocation(canvas->program, "s_texture"), 0);

  glDrawArrays(GL_TRIANGLE_STRIP, 0, QUAD_VERTEX_NUM);

}

static void RenderIMGUI(canvas_t *canvas)
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

  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}


static void Render(canvas_t *canvas)
{

}

wlr_xcursor *InitCursor()
{
  wlr_xcursor_theme *cursor_theme = wlr_xcursor_theme_load("Adwaita", 48);
  if (!cursor_theme){
    printf("load cursor theme FAILED!\n");
    return NULL;
  }

  wlr_xcursor *cursor = wlr_xcursor_theme_get_cursor(cursor_theme, "left_ptr");
  if (!cursor){
    printf("load cursor FAILED\n");
    return NULL;
  }

  return cursor;

}

int main(void)
{
  /*    input     */
  struct udev *udev = udev_new();
  struct libinput *li;
  struct libinput_event *li_event;
  struct libinput_event_keyboard *li_event_kb;
  struct libinput_event_pointer *li_event_pt;
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

  wlr_xcursor *cursor = InitCursor();
  struct wlr_xcursor_image *cursor_image = cursor->images[0];
  if (!cursor_image) {
    printf("load cursor images FAILED!\n");
  }

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();

  // Setup Dear ImGui style
  ImGui::StyleColorsDark();

  ImGui_ImplOpenGL3_Init("#version 300 es");

  InitGLES(&render_context);
  CreateProgram(&render_context);
  CreateTexture(&(render_context.texture_id), cursor_image->width, cursor_image->height, cursor_image->buffer);
  /*    render     */

  bool is_need_quit = false;
  int count = 0;
  int event_count = 0;

  uint32_t screen_width = render_device.default_fb_width;
  uint32_t screen_height = render_device.default_fb_height;

  double cursor_posx = screen_width * 0.5;
  double cursor_posy = screen_height * 0.5;

  // loop
  while(!is_need_quit) {

    event_count = epoll_wait(epoll_fd, ep_events, ARRAY_LENGTH(ep_events), 0);

    libinput_dispatch(li);
    while ((li_event = libinput_get_event(li))) {
      li_event_type = libinput_event_get_type(li_event);
      printf("event_type: %d\n", li_event_type);

      switch (li_event_type) {
      case LIBINPUT_EVENT_DEVICE_ADDED: {
        struct libinput_device *dev = libinput_event_get_device(li_event);
        const char *name = libinput_device_get_name(dev);
        printf("Found Input Device: %s.\n", name);
      }
        break;
      case LIBINPUT_EVENT_KEYBOARD_KEY: {
        if ( (li_event_kb = libinput_event_get_keyboard_event(li_event)) != NULL){
          keycode = libinput_event_keyboard_get_key(li_event_kb);
          printf("keycode: %d\n", keycode);
          if (count++ > 5){
            printf("count: %d\n", count);
            is_need_quit = true;
          }
        }
      }
        break;
      case LIBINPUT_EVENT_POINTER_MOTION: {
        if ((li_event_pt = libinput_event_get_pointer_event(li_event)) != NULL) {
          double cursor_posx_dx = libinput_event_pointer_get_dx(li_event_pt);
          double cursor_posy_dy = libinput_event_pointer_get_dy(li_event_pt);

          cursor_posx = fmin(screen_width, fmax(0, cursor_posx + cursor_posx_dx));
          cursor_posy = fmin(screen_height, fmax(0, cursor_posy + cursor_posy_dy));
          //printf("cursorx: %lf, cursory: %lf", cursor_posx_dx, cursor_posy_dy);
          io.AddMousePosEvent((float)cursor_posx, (float)cursor_posy);
        }
      }
        break;


      case LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE: {
        if ((li_event_pt = libinput_event_get_pointer_event(li_event)) != NULL) {
          cursor_posx = libinput_event_pointer_get_absolute_x_transformed(li_event_pt, screen_width);
          cursor_posy = libinput_event_pointer_get_absolute_y_transformed(li_event_pt, screen_height);
        }
      }
        break;

      case LIBINPUT_EVENT_POINTER_BUTTON: {
        if ((li_event_pt = libinput_event_get_pointer_event(li_event)) != NULL) {
          uint32_t button = libinput_event_pointer_get_button(li_event_pt);
          bool is_press = libinput_event_pointer_get_button_state(li_event_pt) == LIBINPUT_BUTTON_STATE_PRESSED;
          int code = 0;
          switch (button) {
          case BTN_LEFT:
            code = 0;
            break;
          case BTN_RIGHT:
            code = 1;
          case BTN_MIDDLE:
            code = 2;
          }
          io.AddMouseButtonEvent(code, is_press);
          std::cout << "button: " << button << ", is_pressed: " << is_press << std::endl;
        }

      }
        break;


      default:
        break;
      }
      libinput_event_destroy(li_event);
    }

    glClear(GL_COLOR_BUFFER_BIT);
    //Render(&render_context);
    RenderIMGUI(&render_context);
    RenderCursor(&render_context, cursor_image, cursor_posx, cursor_posy);
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
