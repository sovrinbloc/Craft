#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "png.h"
#include "util.h"

int rand_int(int n) {
    int result;
    while (n <= (result = rand() / (RAND_MAX / n)));
    return result;
}

double rand_double() {
    return (double)rand() / (double)RAND_MAX;
}

void update_fps(FPS *fps, int show) {
    fps->frames++;
    double now = glfwGetTime();
    double elapsed = now - fps->since;
    if (elapsed >= 1) {
        int result = fps->frames / elapsed;
        fps->frames = 0;
        fps->since = now;
        if (show) {
            printf("%d\n", result);
        }
    }
}

char *load_file(const char *path) {
    FILE *file = fopen(path, "rb");
    fseek(file, 0, SEEK_END);
    int length = ftell(file);
    rewind(file);
    char *data = calloc(length + 1, sizeof(char));
    fread(data, 1, length, file);
    fclose(file);
    return data;
}

GLuint make_buffer(GLenum target, GLsizei size, const void *data) {
    GLuint buffer;
    glGenBuffers(1, &buffer);
    glBindBuffer(target, buffer);
    glBufferData(target, size, data, GL_STATIC_DRAW);
    glBindBuffer(target, 0);
    return buffer;
}

// make_shader compiles the input source file
//
// @var GLenum type (GL_FRAGMENT_SHADER, GL_VERTEX_SHADER) shader type
// @var char *source Filesystem Location
//
// @return shader: the value from which the shader can be referenced inside the GPU
// https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glShaderSource.xhtml
GLuint make_shader(GLenum type, const char *source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE) {
        GLint length;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        GLchar *info = calloc(length, sizeof(GLchar));
        glGetShaderInfoLog(shader, length, NULL, info);
        fprintf(stderr, "glCompileShader failed:\n%s\n", info);
        free(info);
    }
    return shader;
}

// load_shader loads the data from the filepath
// and sends it off to be compiled and stored
//
// @var GLenum type : (GL_VERTEX_SHADER, GL_FRAGMENT_SHADER) Shader Type
// @var char *path : source file location
//
// @return result : location of the shader to be referenced in the GPU
GLuint load_shader(GLenum type, const char *path) {
    char *data = load_file(path);
    GLuint result = make_shader(type, data);
    free(data);
    return result;
}

// attach the shader to OpenGL. if fails, return error message
// now they are in the GPU, so they can be deleted from memory
//
// @var GLuint shader1
// @var GLuint shader2
//
// @var GLuint program | the reference to the program on the GPU
//
// https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glCreateProgram.xhtml
// https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glAttachShader.xhtml
// https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glLinkProgram.xhtml
GLuint make_program(GLuint shader1, GLuint shader2) {
    GLuint program = glCreateProgram();
    glAttachShader(program, shader1); // attach the compiled shader to the program
    glAttachShader(program, shader2); // attach the compiled shader to the program
    glLinkProgram(program); // link the program to OpenGL (GPU)
    GLint status;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (status == GL_FALSE) {
        GLint length;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        GLchar *info = calloc(length, sizeof(GLchar));
        glGetProgramInfoLog(program, length, NULL, info);
        fprintf(stderr, "glLinkProgram failed: %s\n", info);
        free(info);
    }

    // delet the programs and shaders from memory
    glDetachShader(program, shader1);
    glDetachShader(program, shader2);
    glDeleteShader(shader1);
    glDeleteShader(shader2);
    return program;
}

// load_program loads the shaders, compiles the shaders,
// stores the shaders, and returns the program for Vertex and Fragment
//
// @var char *path1 | the file location to the shader
// @var char *path2 | the file location to the shader
//
// @return GLuint program | return the reference to the shader program
GLuint load_program(const char *path1, const char *path2) {
    GLuint shader1 = load_shader(GL_VERTEX_SHADER, path1);
    GLuint shader2 = load_shader(GL_FRAGMENT_SHADER, path2);
    GLuint program = make_program(shader1, shader2);
    return program;
}

// normalize calculates the unit vector in the same position as
// the original vector
//
// @float *x
// @float *y
// @float *z
void normalize(float *x, float *y, float *z) {
    float d = sqrtf((*x) * (*x) + (*y) * (*y) + (*z) * (*z));
    *x /= d; *y /= d; *z /= d;
}

void matrix_identity(float *matrix) {
    matrix[0] = 1;
    matrix[1] = 0;
    matrix[2] = 0;
    matrix[3] = 0;
    matrix[4] = 0;
    matrix[5] = 1;
    matrix[6] = 0;
    matrix[7] = 0;
    matrix[8] = 0;
    matrix[9] = 0;
    matrix[10] = 1;
    matrix[11] = 0;
    matrix[12] = 0;
    matrix[13] = 0;
    matrix[14] = 0;
    matrix[15] = 1;
}

void matrix_translate(float *matrix, float dx, float dy, float dz) {
    matrix[0] = 1;
    matrix[1] = 0;
    matrix[2] = 0;
    matrix[3] = 0;
    matrix[4] = 0;
    matrix[5] = 1;
    matrix[6] = 0;
    matrix[7] = 0;
    matrix[8] = 0;
    matrix[9] = 0;
    matrix[10] = 1;
    matrix[11] = 0;
    matrix[12] = dx;
    matrix[13] = dy;
    matrix[14] = dz;
    matrix[15] = 1;
}

void matrix_rotate(float *matrix, float x, float y, float z, float angle) {
    normalize(&x, &y, &z);
    float s = sinf(angle);
    float c = cosf(angle);
    float m = 1 - c;
    matrix[0] = m * x * x + c;
    matrix[1] = m * x * y - z * s;
    matrix[2] = m * z * x + y * s;
    matrix[3] = 0;
    matrix[4] = m * x * y + z * s;
    matrix[5] = m * y * y + c;
    matrix[6] = m * y * z - x * s;
    matrix[7] = 0;
    matrix[8] = m * z * x - y * s;
    matrix[9] = m * y * z + x * s;
    matrix[10] = m * z * z + c;
    matrix[11] = 0;
    matrix[12] = 0;
    matrix[13] = 0;
    matrix[14] = 0;
    matrix[15] = 1;
}

void mat_vec_multiply(float *out_vector, float *transform_matrix, float *vector) {
    float result[4];
    for (int i = 0; i < 4; i++) {
        float total = 0;
        for (int j = 0; j < 4; j++) {
            int p = j * 4 + i;
            int q = j;
            total += transform_matrix[p] * vector[q];
        }
        result[i] = total;
    }
    for (int i = 0; i < 4; i++) {
        out_vector[i] = result[i];
    }
}

// mat_multiply multiples one matrix by another and returns the result
void mat_multiply(float *out_matrix, float *matrix_a, float *matrix_b) {
    float result[16];
    for (int c = 0; c < 4; c++) {
        for (int r = 0; r < 4; r++) {
            int index = c * 4 + r;
            float total = 0;
            for (int i = 0; i < 4; i++) {
                int p = i * 4 + r;
                int q = c * 4 + i;
                total += matrix_a[p] * matrix_b[q];
            }
            result[index] = total;
        }
    }
    for (int i = 0; i < 16; i++) {
        out_matrix[i] = result[i];
    }
}

void mat_frustum(
        float *out_matrix, float left, float right, float bottom,
        float top, float znear_val, float zfar_val)
{
    GLfloat x, y, a, b, c, d;
    x = (2.0F*znear_val) / (right-left);
    y = (2.0F*znear_val) / (top-bottom);
    a = (right+left) / (right-left);
    b = (top+bottom) / (top-bottom);
    c = -(zfar_val+znear_val) / ( zfar_val-znear_val);
    d = -(2.0F*zfar_val*znear_val) / (zfar_val-znear_val);

    out_matrix[0] = x;
    out_matrix[1] = 0.0;
    out_matrix[2] = 0.0;
    out_matrix[3] = 0.0;
    out_matrix[4] = 0.0;
    out_matrix[5] = y;
    out_matrix[6] = 0.0;
    out_matrix[7] = 0.0;
    out_matrix[8] = a;
    out_matrix[9] = b;
    out_matrix[10] = c;
    out_matrix[11] = -1.0;
    out_matrix[12] = 0.0;
    out_matrix[13] = 0.0;
    out_matrix[14] = d;
    out_matrix[15] = 0.0;
}

void mat_perspective(
    float *matrix, float fov, float aspect,
    float znear, float zfar)
{
    float ymax, xmax;
    ymax = znear * tanf(fov * PI / 360.0);
    xmax = ymax * aspect;
    mat_frustum(matrix, -xmax, xmax, -ymax, ymax, znear, zfar);
}

void mat_ortho(
    float *matrix,
    float left, float right, float bottom, float top, float near, float far)
{
    matrix[0] = 2 / (right - left);
    matrix[1] = 0;
    matrix[2] = 0;
    matrix[3] = 0;
    matrix[4] = 0;
    matrix[5] = 2 / (top - bottom);
    matrix[6] = 0;
    matrix[7] = 0;
    matrix[8] = 0;
    matrix[9] = 0;
    matrix[10] = -2 / (far - near);
    matrix[11] = 0;
    matrix[12] = -(right + left) / (right - left);
    matrix[13] = -(top + bottom) / (top - bottom);
    matrix[14] = -(far + near) / (far - near);
    matrix[15] = 1;
}

void make_plant(
    float *vertex, float *normal, float *texture,
    float x, float y, float z, float n, int w, float rotation)
{
    float *v = vertex;
    float *d = normal;
    float *t = texture;
    float s = 0.0625;
    float a = 0;
    float b = s;
    float du, dv;
    w--;
    du = (w % 16) * s;
    dv = (w / 16 * 3) * s;
    // left
    *(v++) = x; *(v++) = y - n; *(v++) = z - n;
    *(v++) = x; *(v++) = y + n; *(v++) = z + n;
    *(v++) = x; *(v++) = y + n; *(v++) = z - n;
    *(v++) = x; *(v++) = y - n; *(v++) = z - n;
    *(v++) = x; *(v++) = y - n; *(v++) = z + n;
    *(v++) = x; *(v++) = y + n; *(v++) = z + n;
    *(d++) = -1; *(d++) = 0; *(d++) = 0;
    *(d++) = -1; *(d++) = 0; *(d++) = 0;
    *(d++) = -1; *(d++) = 0; *(d++) = 0;
    *(d++) = -1; *(d++) = 0; *(d++) = 0;
    *(d++) = -1; *(d++) = 0; *(d++) = 0;
    *(d++) = -1; *(d++) = 0; *(d++) = 0;
    *(t++) = a + du; *(t++) = a + dv;
    *(t++) = b + du; *(t++) = b + dv;
    *(t++) = a + du; *(t++) = b + dv;
    *(t++) = a + du; *(t++) = a + dv;
    *(t++) = b + du; *(t++) = a + dv;
    *(t++) = b + du; *(t++) = b + dv;
    // right
    *(v++) = x; *(v++) = y - n; *(v++) = z - n;
    *(v++) = x; *(v++) = y + n; *(v++) = z + n;
    *(v++) = x; *(v++) = y - n; *(v++) = z + n;
    *(v++) = x; *(v++) = y - n; *(v++) = z - n;
    *(v++) = x; *(v++) = y + n; *(v++) = z - n;
    *(v++) = x; *(v++) = y + n; *(v++) = z + n;
    *(d++) = 1; *(d++) = 0; *(d++) = 0;
    *(d++) = 1; *(d++) = 0; *(d++) = 0;
    *(d++) = 1; *(d++) = 0; *(d++) = 0;
    *(d++) = 1; *(d++) = 0; *(d++) = 0;
    *(d++) = 1; *(d++) = 0; *(d++) = 0;
    *(d++) = 1; *(d++) = 0; *(d++) = 0;
    *(t++) = b + du; *(t++) = a + dv;
    *(t++) = a + du; *(t++) = b + dv;
    *(t++) = a + du; *(t++) = a + dv;
    *(t++) = b + du; *(t++) = a + dv;
    *(t++) = b + du; *(t++) = b + dv;
    *(t++) = a + du; *(t++) = b + dv;
    // front
    *(v++) = x - n; *(v++) = y - n; *(v++) = z;
    *(v++) = x + n; *(v++) = y - n; *(v++) = z;
    *(v++) = x + n; *(v++) = y + n; *(v++) = z;
    *(v++) = x - n; *(v++) = y - n; *(v++) = z;
    *(v++) = x + n; *(v++) = y + n; *(v++) = z;
    *(v++) = x - n; *(v++) = y + n; *(v++) = z;
    *(d++) = 0; *(d++) = 0; *(d++) = -1;
    *(d++) = 0; *(d++) = 0; *(d++) = -1;
    *(d++) = 0; *(d++) = 0; *(d++) = -1;
    *(d++) = 0; *(d++) = 0; *(d++) = -1;
    *(d++) = 0; *(d++) = 0; *(d++) = -1;
    *(d++) = 0; *(d++) = 0; *(d++) = -1;
    *(t++) = b + du; *(t++) = a + dv;
    *(t++) = a + du; *(t++) = a + dv;
    *(t++) = a + du; *(t++) = b + dv;
    *(t++) = b + du; *(t++) = a + dv;
    *(t++) = a + du; *(t++) = b + dv;
    *(t++) = b + du; *(t++) = b + dv;
    // back
    *(v++) = x - n; *(v++) = y - n; *(v++) = z;
    *(v++) = x + n; *(v++) = y + n; *(v++) = z;
    *(v++) = x + n; *(v++) = y - n; *(v++) = z;
    *(v++) = x - n; *(v++) = y - n; *(v++) = z;
    *(v++) = x - n; *(v++) = y + n; *(v++) = z;
    *(v++) = x + n; *(v++) = y + n; *(v++) = z;
    *(d++) = 0; *(d++) = 0; *(d++) = 1;
    *(d++) = 0; *(d++) = 0; *(d++) = 1;
    *(d++) = 0; *(d++) = 0; *(d++) = 1;
    *(d++) = 0; *(d++) = 0; *(d++) = 1;
    *(d++) = 0; *(d++) = 0; *(d++) = 1;
    *(d++) = 0; *(d++) = 0; *(d++) = 1;
    *(t++) = a + du; *(t++) = a + dv;
    *(t++) = b + du; *(t++) = b + dv;
    *(t++) = b + du; *(t++) = a + dv;
    *(t++) = a + du; *(t++) = a + dv;
    *(t++) = a + du; *(t++) = b + dv;
    *(t++) = b + du; *(t++) = b + dv;
    // rotate the plant
    float mat[16];
    float vec[4] = {0};
    matrix_rotate(mat, 0, 1, 0, RADIANS(rotation));
    for (int i = 0; i < 24; i++) {
        // vertex
        v = vertex + i * 3;
        vec[0] = *(v++) - x; vec[1] = *(v++) - y; vec[2] = *(v++) - z;
        mat_vec_multiply(vec, mat, vec);
        v = vertex + i * 3;
        *(v++) = vec[0] + x; *(v++) = vec[1] + y; *(v++) = vec[2] + z;
        // normal
        d = normal + i * 3;
        vec[0] = *(d++); vec[1] = *(d++); vec[2] = *(d++);
        mat_vec_multiply(vec, mat, vec);
        d = normal + i * 3;
        *(d++) = vec[0]; *(d++) = vec[1]; *(d++) = vec[2];
    }
}

void make_cube(
    float *vertex, float *normal, float *texture,
    int left, int right, int top, int bottom, int front, int back,
    float x, float y, float z, float n, int w)
{
    float *v = vertex;
    float *d = normal;
    float *t = texture;
    float s = 0.0625;
    float a = 0;
    float b = s;
    float du, dv;
    float ou, ov;
    w--;
    ou = (w % 16) * s;
    ov = (w / 16 * 3) * s;
    if (left) {
        du = ou; dv = ov + s;
        *(v++) = x - n; *(v++) = y - n; *(v++) = z - n;
        *(v++) = x - n; *(v++) = y + n; *(v++) = z + n;
        *(v++) = x - n; *(v++) = y + n; *(v++) = z - n;
        *(v++) = x - n; *(v++) = y - n; *(v++) = z - n;
        *(v++) = x - n; *(v++) = y - n; *(v++) = z + n;
        *(v++) = x - n; *(v++) = y + n; *(v++) = z + n;
        *(d++) = -1; *(d++) = 0; *(d++) = 0;
        *(d++) = -1; *(d++) = 0; *(d++) = 0;
        *(d++) = -1; *(d++) = 0; *(d++) = 0;
        *(d++) = -1; *(d++) = 0; *(d++) = 0;
        *(d++) = -1; *(d++) = 0; *(d++) = 0;
        *(d++) = -1; *(d++) = 0; *(d++) = 0;
        *(t++) = a + du; *(t++) = a + dv;
        *(t++) = b + du; *(t++) = b + dv;
        *(t++) = a + du; *(t++) = b + dv;
        *(t++) = a + du; *(t++) = a + dv;
        *(t++) = b + du; *(t++) = a + dv;
        *(t++) = b + du; *(t++) = b + dv;
    }
    if (right) {
        du = ou; dv = ov + s;
        *(v++) = x + n; *(v++) = y - n; *(v++) = z - n;
        *(v++) = x + n; *(v++) = y + n; *(v++) = z + n;
        *(v++) = x + n; *(v++) = y - n; *(v++) = z + n;
        *(v++) = x + n; *(v++) = y - n; *(v++) = z - n;
        *(v++) = x + n; *(v++) = y + n; *(v++) = z - n;
        *(v++) = x + n; *(v++) = y + n; *(v++) = z + n;
        *(d++) = 1; *(d++) = 0; *(d++) = 0;
        *(d++) = 1; *(d++) = 0; *(d++) = 0;
        *(d++) = 1; *(d++) = 0; *(d++) = 0;
        *(d++) = 1; *(d++) = 0; *(d++) = 0;
        *(d++) = 1; *(d++) = 0; *(d++) = 0;
        *(d++) = 1; *(d++) = 0; *(d++) = 0;
        *(t++) = b + du; *(t++) = a + dv;
        *(t++) = a + du; *(t++) = b + dv;
        *(t++) = a + du; *(t++) = a + dv;
        *(t++) = b + du; *(t++) = a + dv;
        *(t++) = b + du; *(t++) = b + dv;
        *(t++) = a + du; *(t++) = b + dv;
    }
    if (top) {
        du = ou; dv = ov + s + s;
        *(v++) = x - n; *(v++) = y + n; *(v++) = z - n;
        *(v++) = x - n; *(v++) = y + n; *(v++) = z + n;
        *(v++) = x + n; *(v++) = y + n; *(v++) = z + n;
        *(v++) = x - n; *(v++) = y + n; *(v++) = z - n;
        *(v++) = x + n; *(v++) = y + n; *(v++) = z + n;
        *(v++) = x + n; *(v++) = y + n; *(v++) = z - n;
        *(d++) = 0; *(d++) = 1; *(d++) = 0;
        *(d++) = 0; *(d++) = 1; *(d++) = 0;
        *(d++) = 0; *(d++) = 1; *(d++) = 0;
        *(d++) = 0; *(d++) = 1; *(d++) = 0;
        *(d++) = 0; *(d++) = 1; *(d++) = 0;
        *(d++) = 0; *(d++) = 1; *(d++) = 0;
        *(t++) = a + du; *(t++) = b + dv;
        *(t++) = a + du; *(t++) = a + dv;
        *(t++) = b + du; *(t++) = a + dv;
        *(t++) = a + du; *(t++) = b + dv;
        *(t++) = b + du; *(t++) = a + dv;
        *(t++) = b + du; *(t++) = b + dv;
    }
    if (bottom) {
        du = ou; dv = ov + 0;
        *(v++) = x - n; *(v++) = y - n; *(v++) = z - n;
        *(v++) = x + n; *(v++) = y - n; *(v++) = z - n;
        *(v++) = x + n; *(v++) = y - n; *(v++) = z + n;
        *(v++) = x - n; *(v++) = y - n; *(v++) = z - n;
        *(v++) = x + n; *(v++) = y - n; *(v++) = z + n;
        *(v++) = x - n; *(v++) = y - n; *(v++) = z + n;
        *(d++) = 0; *(d++) = -1; *(d++) = 0;
        *(d++) = 0; *(d++) = -1; *(d++) = 0;
        *(d++) = 0; *(d++) = -1; *(d++) = 0;
        *(d++) = 0; *(d++) = -1; *(d++) = 0;
        *(d++) = 0; *(d++) = -1; *(d++) = 0;
        *(d++) = 0; *(d++) = -1; *(d++) = 0;
        *(t++) = a + du; *(t++) = a + dv;
        *(t++) = b + du; *(t++) = a + dv;
        *(t++) = b + du; *(t++) = b + dv;
        *(t++) = a + du; *(t++) = a + dv;
        *(t++) = b + du; *(t++) = b + dv;
        *(t++) = a + du; *(t++) = b + dv;
    }
    if (front) {
        du = ou; dv = ov + s;
        *(v++) = x - n; *(v++) = y - n; *(v++) = z + n;
        *(v++) = x + n; *(v++) = y - n; *(v++) = z + n;
        *(v++) = x + n; *(v++) = y + n; *(v++) = z + n;
        *(v++) = x - n; *(v++) = y - n; *(v++) = z + n;
        *(v++) = x + n; *(v++) = y + n; *(v++) = z + n;
        *(v++) = x - n; *(v++) = y + n; *(v++) = z + n;
        *(d++) = 0; *(d++) = 0; *(d++) = -1;
        *(d++) = 0; *(d++) = 0; *(d++) = -1;
        *(d++) = 0; *(d++) = 0; *(d++) = -1;
        *(d++) = 0; *(d++) = 0; *(d++) = -1;
        *(d++) = 0; *(d++) = 0; *(d++) = -1;
        *(d++) = 0; *(d++) = 0; *(d++) = -1;
        *(t++) = b + du; *(t++) = a + dv;
        *(t++) = a + du; *(t++) = a + dv;
        *(t++) = a + du; *(t++) = b + dv;
        *(t++) = b + du; *(t++) = a + dv;
        *(t++) = a + du; *(t++) = b + dv;
        *(t++) = b + du; *(t++) = b + dv;
    }
    if (back) {
        du = ou; dv = ov + s;
        *(v++) = x - n; *(v++) = y - n; *(v++) = z - n;
        *(v++) = x + n; *(v++) = y + n; *(v++) = z - n;
        *(v++) = x + n; *(v++) = y - n; *(v++) = z - n;
        *(v++) = x - n; *(v++) = y - n; *(v++) = z - n;
        *(v++) = x - n; *(v++) = y + n; *(v++) = z - n;
        *(v++) = x + n; *(v++) = y + n; *(v++) = z - n;
        *(d++) = 0; *(d++) = 0; *(d++) = 1;
        *(d++) = 0; *(d++) = 0; *(d++) = 1;
        *(d++) = 0; *(d++) = 0; *(d++) = 1;
        *(d++) = 0; *(d++) = 0; *(d++) = 1;
        *(d++) = 0; *(d++) = 0; *(d++) = 1;
        *(d++) = 0; *(d++) = 0; *(d++) = 1;
        *(t++) = a + du; *(t++) = a + dv;
        *(t++) = b + du; *(t++) = b + dv;
        *(t++) = b + du; *(t++) = a + dv;
        *(t++) = a + du; *(t++) = a + dv;
        *(t++) = a + du; *(t++) = b + dv;
        *(t++) = b + du; *(t++) = b + dv;
    }
}

void make_cube_wireframe(float *vertex, float x, float y, float z, float n) {
    float *v = vertex;
    *(v++) = x - n; *(v++) = y - n; *(v++) = z - n;
    *(v++) = x - n; *(v++) = y - n; *(v++) = z + n;
    *(v++) = x - n; *(v++) = y - n; *(v++) = z + n;
    *(v++) = x - n; *(v++) = y + n; *(v++) = z + n;
    *(v++) = x - n; *(v++) = y + n; *(v++) = z + n;
    *(v++) = x - n; *(v++) = y + n; *(v++) = z - n;
    *(v++) = x - n; *(v++) = y + n; *(v++) = z - n;
    *(v++) = x - n; *(v++) = y - n; *(v++) = z - n;
    *(v++) = x + n; *(v++) = y - n; *(v++) = z - n;
    *(v++) = x + n; *(v++) = y - n; *(v++) = z + n;
    *(v++) = x + n; *(v++) = y - n; *(v++) = z + n;
    *(v++) = x + n; *(v++) = y + n; *(v++) = z + n;
    *(v++) = x + n; *(v++) = y + n; *(v++) = z + n;
    *(v++) = x + n; *(v++) = y + n; *(v++) = z - n;
    *(v++) = x + n; *(v++) = y + n; *(v++) = z - n;
    *(v++) = x + n; *(v++) = y - n; *(v++) = z - n;

    *(v++) = x - n; *(v++) = y - n; *(v++) = z - n;
    *(v++) = x - n; *(v++) = y - n; *(v++) = z + n;
    *(v++) = x - n; *(v++) = y - n; *(v++) = z + n;
    *(v++) = x + n; *(v++) = y - n; *(v++) = z + n;
    *(v++) = x + n; *(v++) = y - n; *(v++) = z + n;
    *(v++) = x + n; *(v++) = y - n; *(v++) = z - n;
    *(v++) = x + n; *(v++) = y - n; *(v++) = z - n;
    *(v++) = x - n; *(v++) = y - n; *(v++) = z - n;
    *(v++) = x - n; *(v++) = y + n; *(v++) = z - n;
    *(v++) = x - n; *(v++) = y + n; *(v++) = z + n;
    *(v++) = x - n; *(v++) = y + n; *(v++) = z + n;
    *(v++) = x + n; *(v++) = y + n; *(v++) = z + n;
    *(v++) = x + n; *(v++) = y + n; *(v++) = z + n;
    *(v++) = x + n; *(v++) = y + n; *(v++) = z - n;
    *(v++) = x + n; *(v++) = y + n; *(v++) = z - n;
    *(v++) = x - n; *(v++) = y + n; *(v++) = z - n;

    *(v++) = x - n; *(v++) = y - n; *(v++) = z - n;
    *(v++) = x - n; *(v++) = y + n; *(v++) = z - n;
    *(v++) = x - n; *(v++) = y + n; *(v++) = z - n;
    *(v++) = x + n; *(v++) = y + n; *(v++) = z - n;
    *(v++) = x + n; *(v++) = y + n; *(v++) = z - n;
    *(v++) = x + n; *(v++) = y - n; *(v++) = z - n;
    *(v++) = x + n; *(v++) = y - n; *(v++) = z - n;
    *(v++) = x - n; *(v++) = y - n; *(v++) = z - n;
    *(v++) = x - n; *(v++) = y - n; *(v++) = z + n;
    *(v++) = x - n; *(v++) = y + n; *(v++) = z + n;
    *(v++) = x - n; *(v++) = y + n; *(v++) = z + n;
    *(v++) = x + n; *(v++) = y + n; *(v++) = z + n;
    *(v++) = x + n; *(v++) = y + n; *(v++) = z + n;
    *(v++) = x + n; *(v++) = y - n; *(v++) = z + n;
    *(v++) = x + n; *(v++) = y - n; *(v++) = z + n;
    *(v++) = x - n; *(v++) = y - n; *(v++) = z + n;
}

void load_png_texture(const char *file_name) {
    png_byte header[8];

    FILE *fp = fopen(file_name, "rb");
    if (fp == 0) {
        perror(file_name);
        return;
    }

    // read the header
    fread(header, 1, 8, fp);

    if (png_sig_cmp(header, 0, 8)) {
        fprintf(stderr, "error: %s is not a PNG.\n", file_name);
        fclose(fp);
        return;
    }

    /* Create and initialize the png_struct
     * with the desired error handler
     * functions.  If you want to use the
     * default stderr and longjump method,
     * you can supply NULL for the last
     * three parameters.  We also supply the
     * the compiler header file version, so
     * that we know if the application
     * was compiled with a compatible version
     * of the library.  REQUIRED
     */
    png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr) {
        fprintf(stderr, "error: png_create_read_struct returned 0.\n");
        fclose(fp);
        return;
    }

    /* Allocate/initialize the memory
     * for image information.  REQUIRED. */
    // create png info struct
    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        fprintf(stderr, "error: png_create_info_struct returned 0.\n");
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        fclose(fp);
        return;
    }

    /* Set error handling if you are
     * using the setjmp/longjmp method
     * (this is the normal method of
     * doing things with libpng).
     * REQUIRED unless you  set up
     * your own error handlers in
     * the png_create_read_struct()
     * earlier.
     */
    // create png info struct
    png_infop end_info = png_create_info_struct(png_ptr);
    if (!end_info) {
        fprintf(stderr, "error: png_create_info_struct returned 0.\n");
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        return;
    }

    // the code in this if statement gets called if libpng encounters an error
    if (setjmp(png_jmpbuf(png_ptr))) {
        fprintf(stderr, "error from libpng\n");
        png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
        fclose(fp);
        return;
    }

    // init png reading
    /* Set up the output control if
     * you are using standard C streams */
    png_init_io(png_ptr, fp);

    // let libpng know you already read the first 8 bytes
    /* If we have already
     * read some of the signature */
    png_set_sig_bytes(png_ptr, 8);

    // read all the info up to the image data
    png_read_info(png_ptr, info_ptr);

    // variables to pass to get info
    int bit_depth, color_type;
    png_uint_32 width, height;

    // get info about png
    /*
     * If you have enough memory to read
     * in the entire image at once, and
     * you need to specify only
     * transforms that can be controlled
     * with one of the PNG_TRANSFORM_*
     * bits (this presently excludes
     * dithering, filling, setting
     * background, and doing gamma
     * adjustment), then you can read the
     * entire image (including pixels)
     * into the info structure with this
     * call
     *
     * PNG_TRANSFORM_STRIP_16 |
     * PNG_TRANSFORM_PACKING  forces 8 bit
     * PNG_TRANSFORM_EXPAND forces to
     *  expand a palette into RGB
     */
    png_get_IHDR(png_ptr, info_ptr, &width, &height, &bit_depth, &color_type, NULL, NULL, NULL);

    if (bit_depth != 8) {
        fprintf(stderr, "%s: Unsupported bit depth %d.  Must be 8.\n", file_name, bit_depth);
        return;
    }

    GLint format;
    switch(color_type) {
        case PNG_COLOR_TYPE_RGB:
            format = GL_RGB;
            break;
        case PNG_COLOR_TYPE_RGB_ALPHA:
            format = GL_RGBA;
            break;
        default:
            fprintf(stderr, "%s: Unknown libpng color type %d.\n", file_name, color_type);
            return;
    }

    // Update the png info struct.
    png_read_update_info(png_ptr, info_ptr);

    // Row size in bytes.
    int rowbytes = png_get_rowbytes(png_ptr, info_ptr);

    // glTexImage2d requires rows to be 4-byte aligned
    rowbytes += 3 - ((rowbytes - 1) % 4);

    // Allocate the image_data as a big block, to be given to opengl
    png_byte * image_data = (png_byte *)malloc(rowbytes * height * sizeof(png_byte) + 15);
    if (image_data == NULL) {
        fprintf(stderr, "error: could not allocate memory for PNG image data\n");
        png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
        fclose(fp);
        return;
    }

    // row_pointers is for pointing to image_data for reading the png with libpng
    png_byte ** row_pointers = (png_byte **)malloc(height * sizeof(png_byte *));
    if (row_pointers == NULL) {
        fprintf(stderr, "error: could not allocate memory for PNG row pointers\n");
        png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
        free(image_data);
        fclose(fp);
        return;
    }

    // set the individual row_pointers to point at the correct offsets of image_data
    for (unsigned int i = 0; i < height; i++) {
        // note that png is ordered top to
        // bottom, but OpenGL expect it bottom to top
        // so the order or swapped
        row_pointers[height - 1 - i] = image_data + i * rowbytes;
    }

    // read the png into image_data through row_pointers
    /* Clean up after the read,
     * and free any memory allocated */
    png_read_image(png_ptr, row_pointers);

    // submit the texture data
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, image_data);

    // clean up
    /* Clean up after the read,
     * and free any memory allocated */
    png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
    free(image_data);
    free(row_pointers);

    /* Close the file */
    fclose(fp);
}
