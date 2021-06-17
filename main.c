#define GLFW_INCLUDE_GLCOREARB

#include <GLFW/glfw3.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "db.h"
#include "map.h"
#include "noise.h"
#include "util.h"

#define FULLSCREEN 0
#define VSYNC 1
#define SHOW_FPS 0
#define MOUSE_POS 1
#define CHUNK_SIZE 32 // the distance allowed to load blocks (except z?)
#define MAX_CHUNKS 1024
#define CREATE_CHUNK_RADIUS 6
#define RENDER_CHUNK_RADIUS 6
#define DELETE_CHUNK_RADIUS 8
#define REMOVE_BLOCK 0

// good source of information
// https://github.com/Pannoniae/MeinKraft

static GLFWwindow *window;
static int exclusive_to_window = 0;
static int left_click = 0;
static int right_click = 0;
static int flying = 0;
static int block_type = 1;
static int ortho = 0;
static float fov = 65.0;
static int debug_mode = 1;
// buffer objects
// object that stores unformatted, allocated memory (GPU)
// such as vertex data, pixel data, data retrieved by images,
// or the framebuffer
//
// @var uv_coords_buffer
// When texturing a mesh, you need a way to tell to OpenGL which part of the image has to be used for each triangle. This is done with UV coordinates.
// Each vertex can have, on top of its position, a couple of floats, U and V. These coordinates are used to access the texture, in the following way :
// http://www.opengl-tutorial.org/assets/images/tuto-5-textured-cube/UVintro.png
//
// @var int p : the position x of the person divided by max chunks :
//  floorf(roundf(char_x) / CHUNK_SIZE)
// @var int q : the position x of the person divided by max chunks :
//  floorf(roundf(char_z) / CHUNK_SIZE)
// @var position_buffer
// @var normal_buffer
// whatis: A chunk is a 256-block tall, 16×16 segment of a world (256x16x16).
//  Chunks are the method used by the world generator to divide maps into manageable pieces.
typedef struct {
    Map map;
    int p;
    int q;
    int faces;
    GLuint position_buffer;
    GLuint normal_buffer;
    GLuint uv_coords_buffer; // storing texture coordinates
} Chunk;

int is_plant(int w) {
    return w > 16;
}

int is_obstacle(int w) {
    return w != 0 && w < 16;
}

int is_transparent(int w) {
    return w == 0 || w == 10 || is_plant(w);
}

void update_matrix_2d(float *matrix) {
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width * 2, height * 2);
    mat_ortho(matrix, 0, width, 0, height, -1, 1);
}

void update_matrix_3d(
    float *matrix, float x, float y, float z, float rx, float ry)
{
    float matrix_a[16];
    float matrix_b[16];
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width * 2, height * 2);
    float aspect = (float)width / height;
    matrix_identity(matrix_a);
    matrix_translate(matrix_b, -x, -y, -z);
    mat_multiply(matrix_a, matrix_b, matrix_a);
    matrix_rotate(matrix_b, cosf(rx), 0, sinf(rx), ry);
    mat_multiply(matrix_a, matrix_b, matrix_a);
    matrix_rotate(matrix_b, 0, 1, 0, -rx);
    mat_multiply(matrix_a, matrix_b, matrix_a);
    if (ortho) {
        int size = 32;
        mat_ortho(matrix_b, -size * aspect, size * aspect, -size, size, -256, 256);
    }
    else {
        mat_perspective(matrix_b, fov, aspect, 0.1, 1024.0);
    }
    mat_multiply(matrix_a, matrix_b, matrix_a);
    matrix_identity(matrix);
    mat_multiply(matrix, matrix_a, matrix);
}

void update_matrix_item(float *matrix) {
    float a[16];
    float b[16];
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    glViewport(0, 0, width * 2, height * 2);
    float aspect = (float)width / height;
    float size = 64;
    float box = height / size / 2;
    float xoffset = 1 - size / width * 2;
    float yoffset = 1 - size / height * 2;
    matrix_identity(a);
    matrix_rotate(b, 0, 1, 0, PI / 4);
    mat_multiply(a, b, a);
    matrix_rotate(b, 1, 0, 0, -PI / 10);
    mat_multiply(a, b, a);
    mat_ortho(b, -box * aspect, box * aspect, -box, box, -1, 1);
    mat_multiply(a, b, a);
    matrix_translate(b, -xoffset, -yoffset, 0);
    mat_multiply(a, b, a);
    matrix_identity(matrix);
    mat_multiply(matrix, a, matrix);
}

GLuint make_line_buffer() {
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    int x = width / 2;
    int y = height / 2;
    int p = 10;
    float data[] = {
        x, y - p, x, y + p,
        x - p, y, x + p, y
    };
    GLuint buffer = make_buffer(
        GL_ARRAY_BUFFER, sizeof(data), data
    );
    return buffer;
}

GLuint make_cube_buffer(float x, float y, float z, float n) {
    float data[144];
    make_cube_wireframe(data, x, y, z, n);
    GLuint buffer = make_buffer(
        GL_ARRAY_BUFFER, sizeof(data), data
    );
    return buffer;
}

// get_sight_vector
// Returns the current line of sight vector indicating the direction
//        the player is looking
// (-0.000000 169 710800176 -399691872)
//
// @var float rx rotation axis x
// @var float ry rotation axis y
// @var float dx vector x
// @var float dy vector y
// @var float dz vector z
// whatis: this calculates the sight vector from two coordinates to three
//   coordinates. I don't really get it, though.
void get_sight_vector(float rx, float ry, float *dx, float *dy, float *dz) {
    /**
     * y ranges from -90 to 90, or -pi/2 to pi/2, so m ranges from 0 to 1 and
     * is 1 when looking ahead parallel to the ground and 0 when looking
     * straight up or down.
     */
    float m = cosf(ry); //ry = 2; m = -0.41
    /**
     * dy ranges from -1 to 1 and is -1 when looking straight down and 1 when
     * looking straight up.
     */
    *dx = cosf(rx - RADIANS(90)) * m; // rx = 2; result = cos(2)sin(2) = -0.38
    *dy = sinf(ry);
    *dz = sinf(rx - RADIANS(90)) * m;
}

// get_motion_vector
// @var int flying : is the user flying or not
// @var int sz ? the altered strafing movement?
// @var int sx ? the altered position of the character based on strafing?
// @var int rx - rotational angle x
// @var int ry - rotational angle y
// @var int vx
// @var int vy
// @var int vz
void get_motion_vector(int flying, int sz, int sx, float rx, float ry,
    float *vx, float *vy, float *vz) {
    *vx = 0; *vy = 0; *vz = 0;
    if (!sz && !sx) {
        return;
    }
    float strafe = atan2f(sz, sx);
    if (flying) {
        float m = cosf(ry);
        float y = sinf(ry);
        if (sx) {
            y = 0;
            m = 1;
        }
        if (sz > 0) {
            y = -y;
        }
        *vx = cosf(rx + strafe) * m;
        *vy = y;
        *vz = sinf(rx + strafe) * m;
    }
    else {
        *vx = cosf(rx + strafe); // modification of positioning
        *vy = 0;
        *vz = sinf(rx + strafe);
    }
}

Chunk *find_chunk(Chunk *chunks, int chunk_count, int p, int q) {
    for (int i = 0; i < chunk_count; i++) {
        Chunk *chunk = chunks + i;
        if (chunk->p == p && chunk->q == q) {
            return chunk;
        }
    }
    return 0;
}

int chunk_distance(Chunk *chunk, int p, int q) {
    int dp = ABS(chunk->p - p);
    int dq = ABS(chunk->q - q);
    return MAX(dp, dq);
}

int chunk_visible(Chunk *chunk, float *matrix) {
    for (int dp = 0; dp <= 1; dp++) {
        for (int dq = 0; dq <= 1; dq++) {
            for (int y = 0; y < 128; y += 16) {
                float vec[4] = {
                    (chunk->p + dp) * CHUNK_SIZE - dp,
                    y,
                    (chunk->q + dq) * CHUNK_SIZE - dq,
                    1};
                mat_vec_multiply(vec, matrix, vec);
                if (vec[3] >= 0) {
                    return 1;
                }
            }
        }
    }
    return 0;
}

int highest_block(Chunk *chunks, int chunk_count, float x, float z) {
    int result = -1;
    int nx = roundf(x);
    int nz = roundf(z);
    int p = floorf(roundf(x) / CHUNK_SIZE);
    int q = floorf(roundf(z) / CHUNK_SIZE);
    Chunk *chunk = find_chunk(chunks, chunk_count, p, q);
    if (chunk) {
        Map *map = &chunk->map;
        MAP_FOR_EACH(map, e) {
            if (is_obstacle(e->w) && e->x == nx && e->z == nz) {
                result = MAX(result, e->y);
            }
        } END_MAP_FOR_EACH;
    }
    return result;
}

int _hit_test(
    Map *map, float max_distance, int previous,
    float x, float y, float z,
    float vx, float vy, float vz,
    int *hx, int *hy, int *hz)
{
    int m = 8;
    int px = 0;
    int py = 0;
    int pz = 0;
    for (int i = 0; i < max_distance * m; i++) {
        int nx = roundf(x);
        int ny = roundf(y);
        int nz = roundf(z);
        if (nx != px || ny != py || nz != pz) {
            int hw = map_get(map, nx, ny, nz);
            if (hw > 0) {
                if (previous) {
                    *hx = px; *hy = py; *hz = pz;
                }
                else {
                    *hx = nx; *hy = ny; *hz = nz;
                }
                return hw;
            }
            px = nx; py = ny; pz = nz;
        }
        x += vx / m; y += vy / m; z += vz / m;
    }
    return 0;
}

// hit_test
//
// Line of sight search from current position. If a block is
//        intersected it is returned, along with the block previously in the line
//        of sight. If no block is found, return None, None.
// checks to see if the cursor is close enough to a block, and hitting it.
//
// @var char_x (character / camera x coordinates)
// @var char_y (character / camera y coordinates)
// @var char_z (character / camera z coordinates)
// @var rx (horizontal rotation axis -> ray toward X)
// @var ry (vertical rotation axis -> ray toward Y)
// @var bx
// @var by
// @var bz
int hit_test(
        Chunk *chunks, int chunk_count, int previous,
        float char_x, float char_y, float char_z, float rx, float ry,
        int *bx, int *by, int *bz)
{
    int result = 0;
    float best = 0;
    int p = floorf(roundf(char_x) / CHUNK_SIZE);
    int q = floorf(roundf(char_z) / CHUNK_SIZE);
    float dx, dy, dz;

    // todo: figure out how this works
    get_sight_vector(rx, ry, &dx, &dy, &dz);
    for (int i = 0; i < chunk_count; i++) {
        Chunk *chunk = chunks + i;
        if (chunk_distance(chunk, p, q) > 1) {
            continue;
        }
        int hx, hy, hz;
        int hw = _hit_test(&chunk->map, 8, previous,
                           char_x, char_y, char_z, dx, dy, dz, &hx, &hy, &hz);
        if (hw > 0) {
            float d = sqrtf(
                    powf(hx - char_x, 2) + powf(hy - char_y, 2) + powf(hz - char_z, 2));
            if (best == 0 || d < best) {
                best = d;
                *bx = hx; *by = hy; *bz = hz;
                result = hw;
            }
        }
    }
    return result;
}

// collide: (collision detection)
int collide(
    Chunk *chunks, int chunk_count,
    int height, float *x, float *y, float *z)
{
    int result = 0;
    int p = floorf(roundf(*x) / CHUNK_SIZE);
    int q = floorf(roundf(*z) / CHUNK_SIZE);
    Chunk *chunk = find_chunk(chunks, chunk_count, p, q);
    if (!chunk) {
        return result;
    }
    Map *map = &chunk->map;
    int nx = roundf(*x);
    int ny = roundf(*y);
    int nz = roundf(*z);
    float px = *x - nx;
    float py = *y - ny;
    float pz = *z - nz;
    float pad = 0.25;
    for (int dy = 0; dy < height; dy++) {
        if (px < -pad && is_obstacle(map_get(map, nx - 1, ny - dy, nz))) {
            *x = nx - pad;
        }
        if (px > pad && is_obstacle(map_get(map, nx + 1, ny - dy, nz))) {
            *x = nx + pad;
        }
        if (py < -pad && is_obstacle(map_get(map, nx, ny - dy - 1, nz))) {
            *y = ny - pad;
            result = 1;
        }
        if (py > pad && is_obstacle(map_get(map, nx, ny - dy + 1, nz))) {
            *y = ny + pad;
            result = 1;
        }
        if (pz < -pad && is_obstacle(map_get(map, nx, ny - dy, nz - 1))) {
            *z = nz - pad;
        }
        if (pz > pad && is_obstacle(map_get(map, nx, ny - dy, nz + 1))) {
            *z = nz + pad;
        }
    }
    return result;
}

int player_intersects_block(
    int height,
    float x, float y, float z,
    int hx, int hy, int hz)
{
    int nx = roundf(x);
    int ny = roundf(y);
    int nz = roundf(z);
    for (int i = 0; i < height; i++) {
        if (nx == hx && ny - i == hy && nz == hz) {
            return 1;
        }
    }
    return 0;
}

void make_world(Map *map, int p, int q) {
    int pad = 1;
    for (int dx = -pad; dx < CHUNK_SIZE + pad; dx++) {
        for (int dz = -pad; dz < CHUNK_SIZE + pad; dz++) {
            int x = p * CHUNK_SIZE + dx;
            int z = q * CHUNK_SIZE + dz;
            float f = simplex2(x * 0.01, z * 0.01, 4, 0.5, 2);
            float g = simplex2(-x * 0.01, -z * 0.01, 2, 0.9, 2);
            int mh = g * 32 + 16;
            int h = f * mh;
            int w = 1;
            int t = 12;
            if (h <= t) {
                h = t;
                w = 2;
            }
            if (dx < 0 || dz < 0 || dx >= CHUNK_SIZE || dz >= CHUNK_SIZE) {
                w = -1;
            }
            for (int y = 0; y < h; y++) {
                map_set(map, x, y, z, w);
            }
            if (w == 1) {
                if (simplex2(-x * 0.1, z * 0.1, 4, 0.8, 2) > 0.6) {
                    map_set(map, x, h, z, 17);
                }
                if (simplex2(x * 0.05, -z * 0.05, 4, 0.8, 2) > 0.7) {
                    int w = 18 + simplex2(x * 0.1, z * 0.1, 4, 0.8, 2) * 7;
                    map_set(map, x, h, z, w);
                }
            }
            for (int y = 64; y < 72; y++) {
                if (simplex3(x * 0.01, y * 0.1, z * 0.01, 8, 0.5, 2) > 0.75) {
                    map_set(map, x, y, z, 16);
                }
            }
        }
    }
}

// make_single_cube
//
// @var GLuint *position_buffer
// @var GLuint *normal_buffer
// @var GLuint *uv_coords_buffer
void make_single_cube(
    GLuint *position_buffer, GLuint *normal_buffer, GLuint *uv_buffer, int w)
{
    int faces = 6;
    glDeleteBuffers(1, position_buffer);
    glDeleteBuffers(1, normal_buffer);
    glDeleteBuffers(1, uv_buffer);
    GLfloat *position_data = malloc(sizeof(GLfloat) * faces * 18);
    GLfloat *normal_data = malloc(sizeof(GLfloat) * faces * 18);
    GLfloat *uv_data = malloc(sizeof(GLfloat) * faces * 12);
    make_cube(
        position_data,
        normal_data,
        uv_data,
        1, 1, 1, 1, 1, 1,
        0, 0, 0, 0.5, w);
    *position_buffer = make_buffer(
        GL_ARRAY_BUFFER,
        sizeof(GLfloat) * faces * 18,
        position_data
    );
    *normal_buffer = make_buffer(
        GL_ARRAY_BUFFER,
        sizeof(GLfloat) * faces * 18,
        normal_data
    );
    *uv_buffer = make_buffer(
        GL_ARRAY_BUFFER,
        sizeof(GLfloat) * faces * 12,
        uv_data
    );
    free(position_data);
    free(normal_data);
    free(uv_data);
}

void draw_single_cube(
    GLuint position_buffer, GLuint normal_buffer, GLuint uv_buffer,
    GLuint position_loc, GLuint normal_loc, GLuint uv_loc)
{
    glEnableVertexAttribArray(position_loc);
    glEnableVertexAttribArray(normal_loc);
    glEnableVertexAttribArray(uv_loc);
    glBindBuffer(GL_ARRAY_BUFFER, position_buffer);
    glVertexAttribPointer(position_loc, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glBindBuffer(GL_ARRAY_BUFFER, normal_buffer);
    glVertexAttribPointer(normal_loc, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glBindBuffer(GL_ARRAY_BUFFER, uv_buffer);
    glVertexAttribPointer(uv_loc, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDrawArrays(GL_TRIANGLES, 0, 6 * 6);
    glDisableVertexAttribArray(position_loc);
    glDisableVertexAttribArray(normal_loc);
    glDisableVertexAttribArray(uv_loc);
}

// exposed_faces
// returns false if the given position is surrounded on all sides
// by blocks. True, otherwise.
void exposed_faces(
    Map *map, int x, int y, int z,
    int *f1, int *f2, int *f3, int *f4, int *f5, int *f6)
{
    *f1 = is_transparent(map_get(map, x - 1, y, z));
    *f2 = is_transparent(map_get(map, x + 1, y, z));
    *f3 = is_transparent(map_get(map, x, y + 1, z));
    *f4 = is_transparent(map_get(map, x, y - 1, z)) && (y > 0);
    *f5 = is_transparent(map_get(map, x, y, z + 1));
    *f6 = is_transparent(map_get(map, x, y, z - 1));
}

void update_chunk(Chunk *chunk) {
    Map *map = &chunk->map;

    if (chunk->faces) {
        glDeleteBuffers(1, &chunk->position_buffer);
        glDeleteBuffers(1, &chunk->normal_buffer);
        glDeleteBuffers(1, &chunk->uv_coords_buffer);
    }

    int faces = 0;
    MAP_FOR_EACH(map, e) {
        if (e->w <= 0) {
            continue;
        }
        int f1, f2, f3, f4, f5, f6;
        exposed_faces(map, e->x, e->y, e->z, &f1, &f2, &f3, &f4, &f5, &f6);
        int total = f1 + f2 + f3 + f4 + f5 + f6;
        if (is_plant(e->w)) {
            total = total ? 4 : 0;
        }
        faces += total;
    } END_MAP_FOR_EACH;

    GLfloat *position_data = malloc(sizeof(GLfloat) * faces * 18);
    GLfloat *normal_data = malloc(sizeof(GLfloat) * faces * 18);
    GLfloat *uv_data = malloc(sizeof(GLfloat) * faces * 12);
    int position_offset = 0;
    int uv_offset = 0;
    MAP_FOR_EACH(map, e) {
        if (e->w <= 0) {
            continue;
        }
        int f1, f2, f3, f4, f5, f6;
        exposed_faces(map, e->x, e->y, e->z, &f1, &f2, &f3, &f4, &f5, &f6);
        int total = f1 + f2 + f3 + f4 + f5 + f6;
        if (is_plant(e->w)) {
            total = total ? 4 : 0;
        }
        if (total == 0) {
            continue;
        }
        if (is_plant(e->w)) {
            float rotation = simplex3(e->x, e->y, e->z, 4, 0.5, 2) * 360;
            make_plant(
                position_data + position_offset,
                normal_data + position_offset,
                uv_data + uv_offset,
                e->x, e->y, e->z, 0.5, e->w, rotation);
        }
        else {
            make_cube(
                position_data + position_offset,
                normal_data + position_offset,
                uv_data + uv_offset,
                f1, f2, f3, f4, f5, f6,
                e->x, e->y, e->z, 0.5, e->w);
        }
        position_offset += total * 18;
        uv_offset += total * 12;
    } END_MAP_FOR_EACH;

    GLuint position_buffer = make_buffer(
        GL_ARRAY_BUFFER,
        sizeof(GLfloat) * faces * 18,
        position_data
    );
    GLuint normal_buffer = make_buffer(
        GL_ARRAY_BUFFER,
        sizeof(GLfloat) * faces * 18,
        normal_data
    );
    GLuint uv_buffer = make_buffer(
        GL_ARRAY_BUFFER,
        sizeof(GLfloat) * faces * 12,
        uv_data
    );
    free(position_data);
    free(normal_data);
    free(uv_data);

    chunk->faces = faces;
    chunk->position_buffer = position_buffer;
    chunk->normal_buffer = normal_buffer;
    chunk->uv_coords_buffer = uv_buffer;
}

void make_chunk(Chunk *chunk, int p, int q) {
    chunk->p = p;
    chunk->q = q;
    chunk->faces = 0;
    Map *map = &chunk->map;
    map_alloc(map);
    make_world(map, p, q);
    db_update_chunk(map, p, q);
    update_chunk(chunk);
}

void draw_chunk(
    Chunk *chunk, GLuint position_loc, GLuint normal_loc, GLuint uv_loc)
{
    glEnableVertexAttribArray(position_loc);
    glEnableVertexAttribArray(normal_loc);
    glEnableVertexAttribArray(uv_loc);
    glBindBuffer(GL_ARRAY_BUFFER, chunk->position_buffer);
    glVertexAttribPointer(position_loc, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glBindBuffer(GL_ARRAY_BUFFER, chunk->normal_buffer);
    glVertexAttribPointer(normal_loc, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glBindBuffer(GL_ARRAY_BUFFER, chunk->uv_coords_buffer);
    glVertexAttribPointer(uv_loc, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDrawArrays(GL_TRIANGLES, 0, chunk->faces * 6);
    glDisableVertexAttribArray(position_loc);
    glDisableVertexAttribArray(normal_loc);
    glDisableVertexAttribArray(uv_loc);
}

void draw_lines(GLuint buffer, GLuint position_loc, int size, int count) {
    glEnableVertexAttribArray(position_loc);
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glVertexAttribPointer(position_loc, size, GL_FLOAT, GL_FALSE, 0, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDrawArrays(GL_LINES, 0, count);
    glDisableVertexAttribArray(position_loc);
}

// ensure_chunks
//
// @var Chunk *chunks[MAX_CHUNKS] : (the point to the chunks variable)
// @var int *chunk_count : the amount of current chunks already created
// @var int p : the position x of the person divided by max chunks :
//  floorf(roundf(char_x) / CHUNK_SIZE)
// @var int q : the position x of the person divided by max chunks :
//  floorf(roundf(char_z) / CHUNK_SIZE)
// @var int force : todo: figure out what this it
void ensure_chunks(Chunk *chunks, int *chunk_count, int p, int q, int force) {
    int count = *chunk_count;
    for (int i = 0; i < count; i++) {
        Chunk *chunk = chunks + i;
        if (chunk_distance(chunk, p, q) >= DELETE_CHUNK_RADIUS) {
            map_free(&chunk->map);
            glDeleteBuffers(1, &chunk->position_buffer);
            glDeleteBuffers(1, &chunk->normal_buffer);
            glDeleteBuffers(1, &chunk->uv_coords_buffer);
            Chunk *other = chunks + (count - 1);
            chunk->map = other->map;
            chunk->p = other->p;
            chunk->q = other->q;
            chunk->faces = other->faces;
            chunk->position_buffer = other->position_buffer;
            chunk->normal_buffer = other->normal_buffer;
            chunk->uv_coords_buffer = other->uv_coords_buffer;
            count--;
        }
    }
    int n = CREATE_CHUNK_RADIUS; // radius is 6 chunks
    // whatis: this appears to create a square of chunks around the
    //  location of the character (area X x Z)
    for (int i = -n; i <= n; i++) {
        for (int j = -n; j <= n; j++) {
            int x_char_chunk = p + i; // x_char_chunk = (floorf(roundf(char_x) / 32) + i, 0 / 32 + 0: 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1
            int z_char_chunk = q + j; // z_char_chunk = (floorf(roundf(char_z) / 32) + j, 0 / 32 + 0: 0, 1, 2, 3, 4 ,5, 0, 1, 2, 3, 4 ,5
            if (!find_chunk(chunks, count, x_char_chunk, z_char_chunk)) {
                make_chunk(chunks + count, x_char_chunk, z_char_chunk);
                count++;
                if (!force) {
                    *chunk_count = count;
                    return;
                }
            }
        }
    }
    *chunk_count = count;
}

/// _set_block checks to see if the chunk exists, and if it does, we update it.
/// we also update it in the database.
void _set_block(
    Chunk *chunks, int chunk_count,
    int p, int q, int x, int y, int z, int texture)
{
    Chunk *chunk = find_chunk(chunks, chunk_count, p, q);
    if (chunk) {
        Map *map = &chunk->map;
        map_set(map, x, y, z, texture);
        update_chunk(chunk);
    }
    db_insert_block(p, q, x, y, z, texture);
}

// adds a block
void set_block(Chunk *chunks, int chunk_count, int x, int y, int z, int texture) {
    int p = floorf((float)x / CHUNK_SIZE);
    int q = floorf((float)z / CHUNK_SIZE);
    _set_block(chunks, chunk_count, p, q, x, y, z, texture);
    printf("_set_blocks(%d %d %d %d %d %d %d)\n", chunk_count, p, q, x, y, z, texture);

    texture = texture ? -1 : 0;
    int p0 = x == p * CHUNK_SIZE;
    int q0 = z == q * CHUNK_SIZE;
    int p1 = x == p * CHUNK_SIZE + CHUNK_SIZE - 1;
    int q1 = z == q * CHUNK_SIZE + CHUNK_SIZE - 1;
    for (int dp = -1; dp <= 1; dp++) {
        for (int dq = -1; dq <= 1; dq++) {
            if (dp == 0 && dq == 0) continue;
            if (dp < 0 && !p0) continue;
            if (dp > 0 && !p1) continue;
            if (dq < 0 && !q0) continue;
            if (dq > 0 && !q1) continue;
            _set_block(chunks, chunk_count, p + dp, q + dq, x, y, z, texture);
            printf("_set_blocks(%d %d %d %d %d %d %d)\n", chunk_count, p + dp, q + dq, x, y, z, texture);
        }
    }
}

void on_key(GLFWwindow *window, int key, int scancode, int action, int mods) {
    if (action != GLFW_PRESS) {
        return;
    }
    if (key == GLFW_KEY_ESCAPE) {
        if (exclusive_to_window) {
            exclusive_to_window = 0;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }
    if (key == GLFW_KEY_TAB) {
        flying = !flying;
    }
    if (key >= '1' && key <= '9') {
        block_type = key - '1' + 1;
    }
    if (key == 'E') {
        block_type = block_type % 10 + 1;
    }
}

void on_mouse_button(GLFWwindow *window, int button, int action, int mods) {
    if (action != GLFW_PRESS) {
        return;
    }
    if (button == 0) {
        if (exclusive_to_window) {
            if (mods & GLFW_MOD_SUPER) {
                right_click = 1;
            }
            else {
                left_click = 1;
            }
        }
        else {
            exclusive_to_window = 1;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
    }
    if (button == 1) {
        if (exclusive_to_window) {
            right_click = 1;
        }
    }
}

void create_window() {
    #ifdef __APPLE__
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    #endif
    int width = 1024;
    int height = 768;
    GLFWmonitor *monitor = NULL;
    if (FULLSCREEN) {
        int mode_count;
        monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode *modes = glfwGetVideoModes(monitor, &mode_count);
        width = modes[mode_count - 1].width;
        height = modes[mode_count - 1].height;
    }
    window = glfwCreateWindow(width, height, "GodRings", monitor, NULL);
}

int main(int argc, char **argv) {
    srand(time(NULL));
    rand();
    if (!glfwInit()) {
        return -1;
    }
    create_window();
//    if (!window) {
//        glfwTerminate();
//        return -1;
//    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(VSYNC);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetKeyCallback(window, on_key);
    glfwSetMouseButtonCallback(window, on_mouse_button);

    #ifndef __APPLE__
        if (glewInit() != GLEW_OK) {
            return -1;
        }
    #endif

    if (db_init()) {
        return -1;
    }

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LINE_SMOOTH);
    glLogicOp(GL_INVERT);
    glClearColor(0.53, 0.81, 0.92, 1.00);

    GLuint vertex_array;
    glGenVertexArrays(1, &vertex_array);
    glBindVertexArray(vertex_array);

    GLuint texture;
    glGenTextures(1, &texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    load_png_texture("texture.png");

    // whatis: this loads the following shaders and then defines variables to
    //  reference the uniform and regular variables in the shader
    GLuint block_program = load_program(
        "shaders/block_vertex.glsl", "shaders/block_fragment.glsl");

    // whatis: the following is the defining of the variables to be used in the shader
    // glGetUniformLocation
    //
    // https://www.khronos.org/opengl/wiki/Uniform_(GLSL)
    // https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glUniform.xhtml
    //
    // glGetUniformLocation gets the location of the uniform variable.
    // This location value can then be passed to glUniform to set the
    // value of the uniform variable or to glGetUniform in order to
    // query the current value of the uniform variable. You can set it
    // with this value using glUniform[0-4][i,f,ui,fv,iv,uiv...]
    // whatis: gets the uniform variables in the shader program for querying and mod'ng later
    GLuint matrix_loc = glGetUniformLocation(block_program, "matrix");
    GLuint camera_loc = glGetUniformLocation(block_program, "camera");
    GLuint sampler_loc = glGetUniformLocation(block_program, "sampler");
    GLuint timer_loc = glGetUniformLocation(block_program, "timer");
    // glGetAttribLocation
    //
    // https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glGetAttribLocation.xhtml
    //
    // returns the index of the generic vertex attribute that is bound
    // to that attribute variable.
    //
    // After a program object has been linked successfully, the index
    // values for attribute variables remain fixed until the next link
    // command occurs.
    // whatis: gets the attribute (variable positions), for reference
    // todo: figure out if these can be changed after compiling shader
    GLuint position_loc = glGetAttribLocation(block_program, "position");
    GLuint normal_loc = glGetAttribLocation(block_program, "normal");
    GLuint uv_loc = glGetAttribLocation(block_program, "uv");

    // whatis: this loads the next shader and defines the uniforms within it
    GLuint line_program = load_program(
        "shaders/line_vertex.glsl", "shaders/line_fragment.glsl");
    GLuint line_matrix_loc = glGetUniformLocation(line_program, "matrix");
    GLuint line_position_loc = glGetAttribLocation(line_program, "position");

    GLuint item_position_buffer = 0;
    GLuint item_normal_buffer = 0;
    GLuint item_uv_buffer = 0; // texture corrdinates UxV = XxY (coords)
    int previous_block_type = 0;

    Chunk chunks[MAX_CHUNKS];
    int chunk_count = 0;

    // whatis: this defines the character positioning, as well as the mouse
    //  positioning, loads the position from db (if applicable)
    FPS fps = {0, 0};
    float matrix[16];
    float char_x = (rand_double() - 0.5) * 10000; // char_x position of character
    float char_z = (rand_double() - 0.5) * 10000; // char_z position of character
    float char_y = 0; // char_y position of the character
    float dy = 0; // jumping position
    float rx = 0; // horizontal rotation (mouse)
    float ry = 0; // vertical rotation (mouse)
    double mouse_dx = 0; // mouse position (x)
    double mouse_dy = 0; // mouse position (y)

    // whatis: loads the state of the character in char_x, char_y, char_z positioning
    int loaded = db_load_state(&char_x, &char_y, &char_z, &rx, &ry); // character position, and mouse position

    // whatis: rounds the location of the character to ensure the blocks remain snapped in place
    //  it looks to see if any of the chunks have loaded, and if not, it generates the chunks
    //  relative to the character
    ensure_chunks(chunks, &chunk_count,
                  floorf(roundf(char_x) / CHUNK_SIZE), // https://rocmdocs.amd.com/en/latest/Programming_Guides/HIP-GUIDE.html
                  floorf(roundf(char_z) / CHUNK_SIZE), 1);
    if (!loaded) {
        char_y = highest_block(chunks, chunk_count, char_x, char_z) + 2;
    }

    // returns the position of the cursor, in screen coordinates,
    // relative to the upper-left corner of the content area
    glfwGetCursorPos(window, &mouse_dx, &mouse_dy);

    // debugging values for mouse movement (remove these)
    float rxTmp = 0.0f;
    float ryTmp = 0.0f;

    double previous = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        // fps
        update_fps(&fps, SHOW_FPS);
        double now = glfwGetTime();
        double dt = MIN(now - previous, 0.2);
        previous = now;


        // whatis: exculsive is when the mouse is exclusively bound to the window, and will
        //   not be allowed outside of it. that means you have full rotational function
        //   and do not have to worry about moving the mouse off the screen. also, it means
        //   the functionality is based on deltas of the mouse, instead of raw positioning
        // the maximum rx is around 6.28, the maximum ry is around 1.5
        if (exclusive_to_window && (mouse_dx || mouse_dy)) {
            // whatis: this is the section that changes the location of the camera
            //  based on the position of the mouse... the more you move, or the faster
            //  you move, the faster the viewport changes. mx and mouse_dx are the delta of
            //  mouse movement
            double mx, my;
            glfwGetCursorPos(window, &mx, &my); // returns position of cursor relative to window
            float m = 0.0025; // sensitivity
            // rx is the final rotation vector of the mouse after math is done
            // mx is the mouse position on the actual screen... the NEW location
            // mouse_dx is the OLD location of the mouse (directly before move)
            // dx is the delta from one point to the next ... the NEW LOCATION subtracting the old location (new - old)
            // m is the sensitivity (we have to make it translate it into a [-1, 0..., 1] viewpoint or else it will move too much
            // whatis:
            //  if rx = 0; and mx = 0; and mouse_dy is 3, then it will
            //  turn into rx = -3. * 0.0025 =
            rx += (mx - mouse_dx) * m; // the movement of the camera
            ry -= (my - mouse_dy) * m;
            if (debug_mode) {
                if (rx != rxTmp || ry != ryTmp) {
                    rxTmp = rx;
                    ryTmp = ry;
                    printf("(rx, ry) = %f, %f... (mx - mouse_dx)... (%f - %f) = %f, (my - mouse_dy)... (%f - %f) = %f \n", rxTmp, ryTmp, mx, mouse_dx, mx - mouse_dx, my, mouse_dy, my - mouse_dy);
                }
            }

            // the minimum is 0, the maximum is 6.28
            if (rx < 0) {
                rx += RADIANS(360);
            }
            if (rx >= RADIANS(360)){
                rx -= RADIANS(360);
            }
            ry = MAX(ry, -RADIANS(90));
            ry = MIN(ry, RADIANS(90));
            mouse_dx = mx;
            mouse_dy = my;
        }
        else {
            glfwGetCursorPos(window, &mouse_dx, &mouse_dy);
        }

        // whatis: this is what removes blocks
        if (left_click) {
            left_click = 0;
            int hx, hy, hz;
            if (hit_test(chunks, chunk_count, 0, char_x, char_y, char_z, rx, ry,
                         &hx, &hy, &hz))
            {
                if (debug_mode) {
                    printf("hit_test succeeded");
                }
                if (hy > 0) {
                    set_block(chunks, chunk_count, hx, hy, hz, REMOVE_BLOCK);
                    printf("Left Click: %d %d %d %d %d\n", chunk_count, hx, hy, hz, REMOVE_BLOCK);
                }
            }
        }

        // whatis: this is what adds blocks
        if (right_click) {
            right_click = 0;
            int hx, hy, hz;
            int hw = hit_test(chunks, chunk_count, 1, char_x, char_y, char_z, rx, ry,
                              &hx, &hy, &hz);
            if (is_obstacle(hw)) {
                if (!player_intersects_block(2, char_x, char_y, char_z, hx, hy, hz)) {
                    set_block(chunks, chunk_count, hx, hy, hz, block_type);
                    printf("Right Click: %d %d %d %d %d\n", chunk_count, hx, hy, hz, block_type);
                }
            }
        }

        int sz = 0;
        int sx = 0;
        ortho = glfwGetKey(window, 'F'); // shows the display from up above
        fov = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) ? 15.0 : 65.0; // change in aspect ratio?
        if (glfwGetKey(window, 'Q')) break;
        if (glfwGetKey(window, 'W')) sz--;
        if (glfwGetKey(window, 'S')) sz++;
        if (glfwGetKey(window, 'A')) sx--;
        if (glfwGetKey(window, 'D')) sx++;
        if (dy == 0 && glfwGetKey(window, GLFW_KEY_SPACE)) {
            dy = 8;
        }
        float vx, vy, vz;
        get_motion_vector(flying, sz, sx, rx, ry, &vx, &vy, &vz);
        if (glfwGetKey(window, 'Z')) {
            vx = -1; vy = 0; vz = 0;
        }
        if (glfwGetKey(window, 'X')) {
            vx = 1; vy = 0; vz = 0;
        }
        if (glfwGetKey(window, 'C')) {
            vx = 0; vy = -1; vz = 0;
        }
        if (glfwGetKey(window, 'V')) {
            vx = 0; vy = 1; vz = 0;
        }
        if (glfwGetKey(window, 'B')) {
            vx = 0; vy = 0; vz = -1;
        }
        if (glfwGetKey(window, 'N')) {
            vx = 0; vy = 0; vz = 1;
        }
        float speed = flying ? 20 : 5;
        int step = 8;
        float ut = dt / step;
        vx = vx * ut * speed;
        vy = vy * ut * speed;
        vz = vz * ut * speed;
        for (int i = 0; i < step; i++) {
            if (flying) {
                dy = 0;
            }
            else {
                dy -= ut * 25;
                dy = MAX(dy, -250);
            }
            char_x += vx;
            char_y += vy + dy * ut;
            char_z += vz;
            if (collide(chunks, chunk_count, 2, &char_x, &char_y, &char_z)) {
                dy = 0;
            }
        }

        int p = floorf(roundf(char_x) / CHUNK_SIZE);
        int q = floorf(roundf(char_z) / CHUNK_SIZE);
        ensure_chunks(chunks, &chunk_count, p, q, 0);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        update_matrix_3d(matrix, char_x, char_y, char_z, rx, ry);

        // render chunks
        glUseProgram(block_program);
        glUniformMatrix4fv(matrix_loc, 1, GL_FALSE, matrix);
        glUniform3f(camera_loc, char_x, char_y, char_z);
        glUniform1i(sampler_loc, 0);
        glUniform1f(timer_loc, glfwGetTime());
        for (int i = 0; i < chunk_count; i++) {
            Chunk *chunk = chunks + i;
            if (chunk_distance(chunk, p, q) > RENDER_CHUNK_RADIUS) {
                continue;
            }
            if (!chunk_visible(chunk, matrix)) {
                continue;
            }
            draw_chunk(chunk, position_loc, normal_loc, uv_loc);
        }

        // render focused block wireframe
        int hx, hy, hz;
        int hw = hit_test(chunks, chunk_count, 0, char_x, char_y, char_z, rx, ry, &hx, &hy, &hz);
//        printf("hit_test without clicking(%d %f %f %f %f %f %f %d %d %d) \n",
//               chunks->map.data->w,
//               chunks->map.data->x,
//               chunks->map.data->y,
//               chunks->map.data->z,
//               chunk_count, 0, char_x, char_y, char_z, rx, ry, hx, hy, hz);
        if (is_obstacle(hw)) {
            glUseProgram(line_program);
            glLineWidth(1);
            glEnable(GL_COLOR_LOGIC_OP);
            glUniformMatrix4fv(line_matrix_loc, 1, GL_FALSE, matrix);
            GLuint buffer = make_cube_buffer(hx, hy, hz, 0.51);
            draw_lines(buffer, line_position_loc, 3, 48);
            glDeleteBuffers(1, &buffer);
            glDisable(GL_COLOR_LOGIC_OP);
        }

        update_matrix_2d(matrix);

        // render crosshairs
        glUseProgram(line_program);
        glLineWidth(4);
        glEnable(GL_COLOR_LOGIC_OP);
        glUniformMatrix4fv(line_matrix_loc, 1, GL_FALSE, matrix);
        GLuint buffer = make_line_buffer();
        draw_lines(buffer, line_position_loc, 2, 4);
        glDeleteBuffers(1, &buffer);
        glDisable(GL_COLOR_LOGIC_OP);

        // render selected item
        update_matrix_item(matrix);
        if (block_type != previous_block_type) {
            previous_block_type = block_type;
            make_single_cube(
                &item_position_buffer, &item_normal_buffer, &item_uv_buffer,
                block_type);
        }
        glUseProgram(block_program);
        glUniformMatrix4fv(matrix_loc, 1, GL_FALSE, matrix);
        glUniform3f(camera_loc, 0, 0, 5);
        glUniform1i(sampler_loc, 0);
        glUniform1f(timer_loc, glfwGetTime());
        glDisable(GL_DEPTH_TEST);
        draw_single_cube(
            item_position_buffer, item_normal_buffer, item_uv_buffer,
            position_loc, normal_loc, uv_loc);
        glEnable(GL_DEPTH_TEST);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    db_save_state(char_x, char_y, char_z, rx, ry);
    db_close();
    glfwTerminate();
    return 0;
}
