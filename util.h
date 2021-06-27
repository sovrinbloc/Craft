#ifndef _util_h_
#define _util_h_

#ifdef __APPLE__
    #define GLFW_INCLUDE_GLCOREARB
#else
    #include <GL/glew.h>
#endif

#include <GLFW/glfw3.h>

#define PI 3.14159265359
#define DEGREES(radians) ((radians) * 180 / PI)
#define RADIANS(degrees) ((degrees) * PI / 180)
#define ABS(x) ((x) < 0 ? (-(x)) : (x))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MAX_TEXTURES 16
#define TEXTURE_HEIGHT 3

typedef struct {
    unsigned int frames;
    double since;
} FPS;

int rand_int(int n);
double rand_double();
void update_fps(FPS *fps, int show);

GLuint make_buffer(GLenum target, GLsizei size, const void *data);
GLuint make_shader(GLenum type, const char *source);
GLuint load_shader(GLenum type, const char *path);
GLuint make_program(GLuint shader1, GLuint shader2);
GLuint load_program(const char *path1, const char *path2);

void normalize(float *x, float *y, float *z);
void matrix_identity(float *matrix);
void matrix_translate(float *matrix, float dx, float dy, float dz);
void matrix_rotate(float *matrix, float x, float y, float z, float angle);
void mat_vec_multiply(float *out_vector, float *transform_matrix, float *vector);
void mat_multiply(float *out_matrix, float *matrix_a, float *matrix_b);
void mat_frustum(
        float *out_matrix, float left, float right, float bottom,
        float top, float znear_val, float zfar_val);
void mat_perspective(
    float *matrix, float fov, float aspect,
    float near, float far);
void mat_ortho(
    float *matrix,
    float left, float right, float bottom, float top, float near, float far);

void make_plant(
        float *vertex, float *normal, float *texture,
        float x, float y, float z, float n, int block_texture, float rotation);
void make_cube_faces(
        float *vector, float *normal, float *texture,
        int left, int right, int top, int bottom, int front, int back,
        float x, float y, float z, float offset, int w);
void make_cube_wireframe(float *vertex, float x, float y, float z, float n);

void load_png_texture(const char *file_name);

#endif
