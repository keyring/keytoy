
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <epoxy/gl.h>
#include <epoxy/egl.h>

#include "devices.h"


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

  glClearColor(1.f, 0.3f, 0.3f, 1.f);
  glViewport(0, 0, canvas->width, canvas->height);

}

static void Render(canvas_t *canvas)
{
  GLfloat vertex[] = {
    -1, -1, 0,
    -1, 1, 0,
    1, 1, 0,
  };

  GLint position = glGetAttribLocation(canvas->program, "positionIn");
  glEnableVertexAttribArray(position);
  glVertexAttribPointer(position, 3, GL_FLOAT, 0, 0, vertex);

  glClear(GL_COLOR_BUFFER_BIT);
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
