
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <epoxy/gl.h>
#include <epoxy/egl.h>

#include "devices.h"

#define STB_IMAGE_IMPLEMENTATION
#include "external/stb/stb_image.h"

static void CreateTexture(GLuint *texture_id)
{

  glGenTextures(1, texture_id);

  glBindTexture(GL_TEXTURE_2D, *texture_id);

  // set filtering mode
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  // set wrap mode
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

  // load image data
  GLint width, height, channels;
  GLubyte *data = stbi_load("container.jpg", &width, &height, &channels, 0);
  if (!data) {

    printf("failed to load image. use pixels replace\n");

    GLubyte pixels[4 * 3] = {
      255,    0,    0, // red
      0,    255,    0, // green
      0,      0,  255, // blue
      255,  255,    0, // yellow
    };

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    data = pixels;
    width = 2;
    height = 2;

  }
  // upload texture data
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
  glGenerateMipmap(GL_TEXTURE_2D);

  stbi_image_free(data);
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
      char *infolog = malloc(infolen);
      glGetShaderInfoLog(shader, infolen, NULL, infolog);
      fprintf(stderr, "Error compiling shader:\n %s \n", infolog);
      free(infolog);
    }
    glDeleteShader(shader);
    return 0;
  }

  return shader;
}

static const char VERTEX_SHADER[] =
  "attribute vec3 a_position;"
  "attribute vec2 a_texcoord;"
  "varying vec2 v_texcoord;"
  "void main(){"
  "    gl_Position = vec4(a_position, 1);"
  "    v_texcoord = a_texcoord;"
  "}";

static const char FRAGMENT_SHADER[] =
  "precision mediump float;"
  "varying vec2 v_texcoord;"
  "uniform sampler2D s_texture;"
  "void main(){"
  "    gl_FragColor = texture2D(s_texture, v_texcoord);"
  "}";

static void InitGLES(canvas_t *canvas)
{
  GLint major = 0;
  GLint minor = 0;
  glGetIntegerv(GL_MAJOR_VERSION, &major);
  glGetIntegerv(GL_MINOR_VERSION, &minor);

  if (major == 0 && minor == 0) {
    const char *gl_version = (const char *)glGetString(GL_VERSION);
    sscanf(gl_version, "%d.%d", &major, &minor);
  }
  printf("opengl version: %d, %d\n", major, minor);

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
      char *infolog = malloc(infolen);
      glGetProgramInfoLog(canvas->program, infolen, NULL, infolog);
      fprintf(stderr, "Error linking program:\n %s \n", infolog);
      free(infolog);
    }
    glDeleteProgram(canvas->program);
    exit(1);
  }
  CreateTexture(&canvas->texture_id);
  glClearColor(1.f, 0.3f, 0.3f, 1.f);
  glViewport(0, 0, canvas->width, canvas->height);

}

static void Render(canvas_t *canvas)
{
  GLfloat vertex[] = {
    -1, -1, 0, 0, 0,
    -1, 1, 0,  0, 1,
    1, 1, 0,   1, 0,
  };

  void *start = &vertex[0];

  GLint position = glGetAttribLocation(canvas->program, "a_position");
  glEnableVertexAttribArray(position);
  glVertexAttribPointer(position, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), start);

  GLint texcoord = glGetAttribLocation(canvas->program, "a_texcoord");
  glEnableVertexAttribArray(texcoord);
  glVertexAttribPointer(texcoord, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), start + (3 * sizeof(GLfloat)));

  glClear(GL_COLOR_BUFFER_BIT);

  glBindTexture(GL_TEXTURE_2D, canvas->texture_id);
  glUseProgram(canvas->program);

  glDrawArrays(GL_TRIANGLES, 0, 3);

  eglSwapBuffers(canvas->display, canvas->surface);

}

int main(void)
{
  device_t render_device;
  CreateRenderDevice(&render_device);

  canvas_t render_context;
  CreateRenderContext(&render_device, &render_context);

  InitGLES(&render_context);

  Render(&render_context);

  OutputDisplay(&render_device);
  return 0;
}
