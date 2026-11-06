/* test-gles-teapot.c - Mesa EGL/GLES2 Utah 茶壶（从 EwokOS teapot 移植）
 *
 * 几何：Newell 经典犹他茶壶，306 控制点 / 32 个双三次 Bezier patch，
 * 运行时细分为 32*10*10*2 个三角形，法线解析求得。
 * 光照：每顶点双面 Blinn-Phong（CPU 计算，颜色作为 attribute 上传），
 * 顶点着色器只做 uMVP 变换，和 EwokOS 的做法一致。
 * 上屏：direct（/dev/fb 直写）或 tfu（离屏 BO + TFU）。
 *
 *   /bin/test-gles-teapot direct 300
 *   /bin/test-gles-teapot tfu 300
 *   /bin/test-gles-teapot            # 自检（两帧读回）
 */
#include <fcntl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>

#include "client.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef EGL_PLATFORM_DEVICE_EXT
#define EGL_PLATFORM_DEVICE_EXT 0x313F
#endif

#define GPU_LOW_SCREEN_PARAM 141
#define GPU_LOW_BO_CREATE 110
#define GPU_LOW_BO_DESTROY 112
#define GPU_LOW_SUBMIT_TFU 122

#define W 640
#define H 480

typedef struct {
  unsigned int src_handle, src_offset;
  unsigned int dst_handle, dst_offset;
  unsigned int w, h;
  unsigned int src_fmt, dst_fmt;
  unsigned int flags;
} gpu_submit_tfu_req;

typedef struct {
  unsigned int pa, size, flags;
} gpu_bo_create_req;

static const GLfloat cpdata[306][3] = {
    {1.4,0,2.4},{1.4,-0.784,2.4},{0.78,-1.4,2.4},{0,-1.4,2.4},
    {1.3375,0,2.53125},{1.3375,-0.749,2.53125},{0.749,-1.3375,2.53125},{0,-1.3375,2.53125},
    {1.4375,0,2.53125},{1.4375,-0.805,2.53125},{0.805,-1.4375,2.53125},{0,-1.4375,2.53125},
    {1.5,0,2.4},{1.5,-0.84,2.4},{0.84,-1.5,2.4},{0,-1.5,2.4},
    {-0.784,-1.4,2.4},{-1.4,-0.784,2.4},{-1.4,0,2.4},{-0.749,-1.3375,2.53125},
    {-1.3375,-0.749,2.53125},{-1.3375,0,2.53125},{-0.805,-1.4375,2.53125},{-1.4375,-0.805,2.53125},
    {-1.4375,0,2.53125},{-0.84,-1.5,2.4},{-1.5,-0.84,2.4},{-1.5,0,2.4},
    {-1.4,0.784,2.4},{-0.784,1.4,2.4},{0,1.4,2.4},{-1.3375,0.749,2.53125},
    {-0.749,1.3375,2.53125},{0,1.3375,2.53125},{-1.4375,0.805,2.53125},{-0.805,1.4375,2.53125},
    {0,1.4375,2.53125},{-1.5,0.84,2.4},{-0.84,1.5,2.4},{0,1.5,2.4},
    {0.784,1.4,2.4},{1.4,0.784,2.4},{0.749,1.3375,2.53125},{1.3375,0.749,2.53125},
    {0.805,1.4375,2.53125},{1.4375,0.805,2.53125},{0.84,1.5,2.4},{1.5,0.84,2.4},
    {1.75,0,1.875},{1.75,-0.98,1.875},{0.98,-1.75,1.875},{0,-1.75,1.875},
    {2,0,1.35},{2,-1.12,1.35},{1.12,-2,1.35},{0,-2,1.35},
    {2,0,0.9},{2,-1.12,0.9},{1.12,-2,0.9},{0,-2,0.9},
    {-0.98,-1.75,1.875},{-1.75,-0.98,1.875},{-1.75,0,1.875},{-1.12,-2,1.35},
    {-2,-1.12,1.35},{-2,0,1.35},{-1.12,-2,0.9},{-2,-1.12,0.9},
    {-2,0,0.9},{-1.75,0.98,1.875},{-0.98,1.75,1.875},{0,1.75,1.875},
    {-2,1.12,1.35},{-1.12,2,1.35},{0,2,1.35},{-2,1.12,0.9},
    {-1.12,2,0.9},{0,2,0.9},{0.98,1.75,1.875},{1.75,0.98,1.875},
    {1.12,2,1.35},{2,1.12,1.35},{1.12,2,0.9},{2,1.12,0.9},
    {2,0,0.45},{2,-1.12,0.45},{1.12,-2,0.45},{0,-2,0.45},
    {1.5,0,0.225},{1.5,-0.84,0.225},{0.84,-1.5,0.225},{0,-1.5,0.225},
    {1.5,0,0.15},{1.5,-0.84,0.15},{0.84,-1.5,0.15},{0,-1.5,0.15},
    {-1.12,-2,0.45},{-2,-1.12,0.45},{-2,0,0.45},{-0.84,-1.5,0.225},
    {-1.5,-0.84,0.225},{-1.5,0,0.225},{-0.84,-1.5,0.15},{-1.5,-0.84,0.15},
    {-1.5,0,0.15},{-2,1.12,0.45},{-1.12,2,0.45},{0,2,0.45},
    {-1.5,0.84,0.225},{-0.84,1.5,0.225},{0,1.5,0.225},{-1.5,0.84,0.15},
    {-0.84,1.5,0.15},{0,1.5,0.15},{1.12,2,0.45},{2,1.12,0.45},
    {0.84,1.5,0.225},{1.5,0.84,0.225},{0.84,1.5,0.15},{1.5,0.84,0.15},
    {-1.6,0,2.025},{-1.6,-0.3,2.025},{-1.5,-0.3,2.25},{-1.5,0,2.25},
    {-2.3,0,2.025},{-2.3,-0.3,2.025},{-2.5,-0.3,2.25},{-2.5,0,2.25},
    {-2.7,0,2.025},{-2.7,-0.3,2.025},{-3,-0.3,2.25},{-3,0,2.25},
    {-2.7,0,1.8},{-2.7,-0.3,1.8},{-3,-0.3,1.8},{-3,0,1.8},
    {-1.5,0.3,2.25},{-1.6,0.3,2.025},{-2.5,0.3,2.25},{-2.3,0.3,2.025},
    {-3,0.3,2.25},{-2.7,0.3,2.025},{-3,0.3,1.8},{-2.7,0.3,1.8},
    {-2.7,0,1.575},{-2.7,-0.3,1.575},{-3,-0.3,1.35},{-3,0,1.35},
    {-2.5,0,1.125},{-2.5,-0.3,1.125},{-2.65,-0.3,0.9375},{-2.65,0,0.9375},
    {-2,-0.3,0.9},{-1.9,-0.3,0.6},{-1.9,0,0.6},{-3,0.3,1.35},
    {-2.7,0.3,1.575},{-2.65,0.3,0.9375},{-2.5,0.3,1.125},{-1.9,0.3,0.6},
    {-2,0.3,0.9},{1.7,0,1.425},{1.7,-0.66,1.425},{1.7,-0.66,0.6},
    {1.7,0,0.6},{2.6,0,1.425},{2.6,-0.66,1.425},{3.1,-0.66,0.825},
    {3.1,0,0.825},{2.3,0,2.1},{2.3,-0.25,2.1},{2.4,-0.25,2.025},
    {2.4,0,2.025},{2.7,0,2.4},{2.7,-0.25,2.4},{3.3,-0.25,2.4},
    {3.3,0,2.4},{1.7,0.66,0.6},{1.7,0.66,1.425},{3.1,0.66,0.825},
    {2.6,0.66,1.425},{2.4,0.25,2.025},{2.3,0.25,2.1},{3.3,0.25,2.4},
    {2.7,0.25,2.4},{2.8,0,2.475},{2.8,-0.25,2.475},{3.525,-0.25,2.49375},
    {3.525,0,2.49375},{2.9,0,2.475},{2.9,-0.15,2.475},{3.45,-0.15,2.5125},
    {3.45,0,2.5125},{2.8,0,2.4},{2.8,-0.15,2.4},{3.2,-0.15,2.4},
    {3.2,0,2.4},{3.525,0.25,2.49375},{2.8,0.25,2.475},{3.45,0.15,2.5125},
    {2.9,0.15,2.475},{3.2,0.15,2.4},{2.8,0.15,2.4},{0,0,3.15},
    {0,-0.002,3.15},{0.002,0,3.15},{0.8,0,3.15},{0.8,-0.45,3.15},
    {0.45,-0.8,3.15},{0,-0.8,3.15},{0,0,2.85},{0.2,0,2.7},
    {0.2,-0.112,2.7},{0.112,-0.2,2.7},{0,-0.2,2.7},{-0.002,0,3.15},
    {-0.45,-0.8,3.15},{-0.8,-0.45,3.15},{-0.8,0,3.15},{-0.112,-0.2,2.7},
    {-0.2,-0.112,2.7},{-0.2,0,2.7},{0,0.002,3.15},{-0.8,0.45,3.15},
    {-0.45,0.8,3.15},{0,0.8,3.15},{-0.2,0.112,2.7},{-0.112,0.2,2.7},
    {0,0.2,2.7},{0.45,0.8,3.15},{0.8,0.45,3.15},{0.112,0.2,2.7},
    {0.2,0.112,2.7},{0.4,0,2.55},{0.4,-0.224,2.55},{0.224,-0.4,2.55},
    {0,-0.4,2.55},{1.3,0,2.55},{1.3,-0.728,2.55},{0.728,-1.3,2.55},
    {0,-1.3,2.55},{1.3,0,2.4},{1.3,-0.728,2.4},{0.728,-1.3,2.4},
    {0,-1.3,2.4},{-0.224,-0.4,2.55},{-0.4,-0.224,2.55},{-0.4,0,2.55},
    {-0.728,-1.3,2.55},{-1.3,-0.728,2.55},{-1.3,0,2.55},{-0.728,-1.3,2.4},
    {-1.3,-0.728,2.4},{-1.3,0,2.4},{-0.4,0.224,2.55},{-0.224,0.4,2.55},
    {0,0.4,2.55},{-1.3,0.728,2.55},{-0.728,1.3,2.55},{0,1.3,2.55},
    {-1.3,0.728,2.4},{-0.728,1.3,2.4},{0,1.3,2.4},{0.224,0.4,2.55},
    {0.4,0.224,2.55},{0.728,1.3,2.55},{1.3,0.728,2.55},{0.728,1.3,2.4},
    {1.3,0.728,2.4},{0,0,0},{1.5,0,0.15},{1.5,0.84,0.15},
    {0.84,1.5,0.15},{0,1.5,0.15},{1.5,0,0.075},{1.5,0.84,0.075},
    {0.84,1.5,0.075},{0,1.5,0.075},{1.425,0,0},{1.425,0.798,0},
    {0.798,1.425,0},{0,1.425,0},{-0.84,1.5,0.15},{-1.5,0.84,0.15},
    {-1.5,0,0.15},{-0.84,1.5,0.075},{-1.5,0.84,0.075},{-1.5,0,0.075},
    {-0.798,1.425,0},{-1.425,0.798,0},{-1.425,0,0},{-1.5,-0.84,0.15},
    {-0.84,-1.5,0.15},{0,-1.5,0.15},{-1.5,-0.84,0.075},{-0.84,-1.5,0.075},
    {0,-1.5,0.075},{-1.425,-0.798,0},{-0.798,-1.425,0},{0,-1.425,0},
    {0.84,-1.5,0.15},{1.5,-0.84,0.15},{0.84,-1.5,0.075},{1.5,-0.84,0.075},
    {0.798,-1.425,0},{1.425,-0.798,0}
};

/* Each patch lists 16 control-point indices (1-based) in row-major 4x4 order. */
static const int patches[32][16] = {
    {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16},
    {4,17,18,19,8,20,21,22,12,23,24,25,16,26,27,28},
    {19,29,30,31,22,32,33,34,25,35,36,37,28,38,39,40},
    {31,41,42,1,34,43,44,5,37,45,46,9,40,47,48,13},
    {13,14,15,16,49,50,51,52,53,54,55,56,57,58,59,60},
    {16,26,27,28,52,61,62,63,56,64,65,66,60,67,68,69},
    {28,38,39,40,63,70,71,72,66,73,74,75,69,76,77,78},
    {40,47,48,13,72,79,80,49,75,81,82,53,78,83,84,57},
    {57,58,59,60,85,86,87,88,89,90,91,92,93,94,95,96},
    {60,67,68,69,88,97,98,99,92,100,101,102,96,103,104,105},
    {69,76,77,78,99,106,107,108,102,109,110,111,105,112,113,114},
    {78,83,84,57,108,115,116,85,111,117,118,89,114,119,120,93},
    {121,122,123,124,125,126,127,128,129,130,131,132,133,134,135,136},
    {124,137,138,121,128,139,140,125,132,141,142,129,136,143,144,133},
    {133,134,135,136,145,146,147,148,149,150,151,152,69,153,154,155},
    {136,143,144,133,148,156,157,145,152,158,159,149,155,160,161,69},
    {162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177},
    {165,178,179,162,169,180,181,166,173,182,183,170,177,184,185,174},
    {174,175,176,177,186,187,188,189,190,191,192,193,194,195,196,197},
    {177,184,185,174,189,198,199,186,193,200,201,190,197,202,203,194},
    {204,204,204,204,207,208,209,210,211,211,211,211,212,213,214,215},
    {204,204,204,204,210,217,218,219,211,211,211,211,215,220,221,222},
    {204,204,204,204,219,224,225,226,211,211,211,211,222,227,228,229},
    {204,204,204,204,226,230,231,207,211,211,211,211,229,232,233,212},
    {212,213,214,215,234,235,236,237,238,239,240,241,242,243,244,245},
    {215,220,221,222,237,246,247,248,241,249,250,251,245,252,253,254},
    {222,227,228,229,248,255,256,257,251,258,259,260,254,261,262,263},
    {229,232,233,212,257,264,265,234,260,266,267,238,263,268,269,242},
    {270,270,270,270,279,280,281,282,275,276,277,278,93,120,119,114},
    {270,270,270,270,282,289,290,291,278,286,287,288,114,113,112,105},
    {270,270,270,270,291,298,299,300,288,295,296,297,105,104,103,96},
    {270,270,270,270,300,305,306,279,297,303,304,275,96,95,94,93}
};


#define NPATCH      32
#define TEAPOT_GRID 10

/* ---- 全局状态（与 EwokOS teapot 相同） ---- */
static int win_width = W;
static int win_height = H;

struct teapot_mesh {
    GLfloat* vertices;          /* CPU: 6 floats/vertex x,y,z, nx,ny,nz */
    GLfloat* colors;            /* CPU: 4 floats/vertex rgba             */
    int nvertices;
    int patch_first[NPATCH];
    int patch_count[NPATCH];
    GLuint pos_vbo;
    GLuint color_vbo;
};
static struct teapot_mesh teapot;

static GLfloat spin_angle = 0.0f;
static GLfloat camera_angle = 0.0f;
static GLfloat camera_elev = 15.0f;
static GLfloat projection_matrix[16];

static GLuint program = 0;
static GLint loc_mvp = -1;
static GLint attr_pos = 0, attr_color = 1;

static const char *teapot_vs =
    "attribute vec4 aPosition;\n"
    "attribute vec4 aColor;\n"
    "uniform mat4 uMVP;\n"
    "varying vec4 vColor;\n"
    "void main() {\n"
    "    vColor = aColor;\n"
    "    gl_Position = uMVP * aPosition;\n"
    "}\n";

static const char *teapot_fs =
    "precision mediump float;\n"
    "varying vec4 vColor;\n"
    "void main() {\n"
    "    gl_FragColor = vColor;\n"
    "}\n";

/* ------------------------------------------------------------------ *
 * Minimal matrix helpers (row-vector convention, as in gears.c).
 * ------------------------------------------------------------------ */
static void multiply(GLfloat *m, const GLfloat *n)
{
    GLfloat tmp[16];
    const GLfloat *row, *column;
    div_t d;
    int i, j;

    for (i = 0; i < 16; i++) {
        tmp[i] = 0;
        d = div(i, 4);
        row = n + d.quot * 4;
        column = m + d.rem;
        for (j = 0; j < 4; j++)
            tmp[i] += row[j] * column[j * 4];
    }
    memcpy(m, tmp, sizeof(tmp));
}

static void rotate(GLfloat *m, GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
    GLfloat c = cosf(angle * M_PI / 180.0f);
    GLfloat s = sinf(angle * M_PI / 180.0f);
    GLfloat r[16] = {
        x * x * (1 - c) + c,     y * x * (1 - c) + z * s, x * z * (1 - c) - y * s, 0,
        x * y * (1 - c) - z * s, y * y * (1 - c) + c,     y * z * (1 - c) + x * s, 0,
        x * z * (1 - c) + y * s, y * z * (1 - c) - x * s, z * z * (1 - c) + c,     0,
        0, 0, 0, 1
    };
    multiply(m, r);
}

static void translate(GLfloat *m, GLfloat x, GLfloat y, GLfloat z)
{
    GLfloat t[16] = { 1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 1, 0,  x, y, z, 1 };
    multiply(m, t);
}

static void identity(GLfloat *m)
{
    GLfloat t[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
    memcpy(m, t, sizeof(t));
}

static void transpose(GLfloat *m)
{
    GLfloat t[16] = {
        m[0], m[4], m[8],  m[12],
        m[1], m[5], m[9],  m[13],
        m[2], m[6], m[10], m[14],
        m[3], m[7], m[11], m[15]
    };
    memcpy(m, t, sizeof(t));
}

static void invert(GLfloat *m)
{
    GLfloat t[16];
    identity(t);
    t[12] = -m[12]; t[13] = -m[13]; t[14] = -m[14];
    m[12] = m[13] = m[14] = 0;
    transpose(m);
    multiply(m, t);
}

static void perspective(GLfloat *m, GLfloat fovy, GLfloat aspect, GLfloat z_near, GLfloat z_far)
{
    GLfloat tmp[16];
    identity(tmp);

    GLfloat radians = fovy / 2.0f * M_PI / 180.0f;
    GLfloat sine = sinf(radians);
    GLfloat cosine = cosf(radians);
    GLfloat delta_z = z_far - z_near;

    if (delta_z == 0 || sine == 0 || aspect == 0)
        return;

    GLfloat cotangent = cosine / sine;

    tmp[0] = cotangent / aspect;
    tmp[5] = cotangent;
    tmp[10] = -(z_far + z_near) / delta_z;
    tmp[11] = -1.0f;
    tmp[14] = -2.0f * z_near * z_far / delta_z;
    tmp[15] = 0.0f;

    memcpy(m, tmp, sizeof(tmp));
}

static void look_at(GLfloat *m, GLfloat eye_x, GLfloat eye_y, GLfloat eye_z,
                    GLfloat center_x, GLfloat center_y, GLfloat center_z,
                    GLfloat up_x, GLfloat up_y, GLfloat up_z)
{
    GLfloat f[3], s[3], u[3];
    GLfloat f_len, s_len;

    f[0] = center_x - eye_x;
    f[1] = center_y - eye_y;
    f[2] = center_z - eye_z;
    f_len = sqrtf(f[0]*f[0] + f[1]*f[1] + f[2]*f[2]);
    if (f_len == 0.0f) f_len = 1.0f;
    f[0] /= f_len; f[1] /= f_len; f[2] /= f_len;

    s[0] = f[1]*up_z - f[2]*up_y;
    s[1] = f[2]*up_x - f[0]*up_z;
    s[2] = f[0]*up_y - f[1]*up_x;
    s_len = sqrtf(s[0]*s[0] + s[1]*s[1] + s[2]*s[2]);
    if (s_len == 0.0f) s_len = 1.0f;
    s[0] /= s_len; s[1] /= s_len; s[2] /= s_len;

    u[0] = s[1]*f[2] - s[2]*f[1];
    u[1] = s[2]*f[0] - s[0]*f[2];
    u[2] = s[0]*f[1] - s[1]*f[0];

    GLfloat look_at_mat[16] = {
        s[0], u[0], -f[0], 0,
        s[1], u[1], -f[1], 0,
        s[2], u[2], -f[2], 0,
        -(s[0]*eye_x + s[1]*eye_y + s[2]*eye_z),
        -(u[0]*eye_x + u[1]*eye_y + u[2]*eye_z),
        f[0]*eye_x + f[1]*eye_y + f[2]*eye_z,
        1
    };

    memcpy(m, look_at_mat, sizeof(look_at_mat));
}

static void bernstein(float t, float b[4])
{
    float mt = 1.0f - t;
    b[0] = mt * mt * mt;
    b[1] = 3.0f * t * mt * mt;
    b[2] = 3.0f * t * t * mt;
    b[3] = t * t * t;
}

static void bernstein_d(float t, float d[4])
{
    float mt = 1.0f - t;
    d[0] = -3.0f * mt * mt;
    d[1] = 3.0f * mt * mt - 6.0f * t * mt;
    d[2] = 6.0f * t * mt - 3.0f * t * t;
    d[3] = 3.0f * t * t;
}

/* Evaluate one bicubic Bezier patch at (u,v); write position and the unit
 * surface normal (cross product of the u and v tangents). */
static void eval_patch(const int* pidx, float u, float v, float pos[3], float nrm[3])
{
    float bu[4], bv[4], du[4], dv[4];
    bernstein(u, bu);
    bernstein(v, bv);
    bernstein_d(u, du);
    bernstein_d(v, dv);

    float px = 0, py = 0, pz = 0;
    float tu_x = 0, tu_y = 0, tu_z = 0;   /* dS/du */
    float tv_x = 0, tv_y = 0, tv_z = 0;   /* dS/dv */

    int i, j;
    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            const GLfloat* cp = cpdata[pidx[i * 4 + j] - 1];
            float w  = bu[i] * bv[j];
            float wu = du[i] * bv[j];
            float wv = bu[i] * dv[j];
            px += w * cp[0];  py += w * cp[1];  pz += w * cp[2];
            tu_x += wu * cp[0]; tu_y += wu * cp[1]; tu_z += wu * cp[2];
            tv_x += wv * cp[0]; tv_y += wv * cp[1]; tv_z += wv * cp[2];
        }
    }

    pos[0] = px; pos[1] = py; pos[2] = pz;

    float nx = tu_y * tv_z - tu_z * tv_y;
    float ny = tu_z * tv_x - tu_x * tv_z;
    float nz = tu_x * tv_y - tu_y * tv_x;
    float len = sqrtf(nx*nx + ny*ny + nz*nz);
    if (len > 1e-8f) {
        nx /= len; ny /= len; nz /= len;
    } else {
        nx = 0; ny = 0; nz = 1.0f;
    }
    nrm[0] = nx; nrm[1] = ny; nrm[2] = nz;
}

static GLfloat* emit_vert(GLfloat* v, const float* pos, const float* nrm)
{
    v[0] = pos[0]; v[1] = pos[1]; v[2] = pos[2];
    v[3] = nrm[0]; v[4] = nrm[1]; v[5] = nrm[2];
    return v + 6;
}

static void build_teapot(void)
{
    const int G = TEAPOT_GRID;
    const int per_patch = G * G * 2 * 3;      /* triangles * 3 verts */
    const int total = NPATCH * per_patch;

    teapot.vertices = (GLfloat*)malloc(total * 6 * sizeof(GLfloat));
    if (teapot.vertices == NULL) {
        printf("build_teapot: out of memory\n");
        return;
    }

    /* Per-patch lattice of positions/normals (reused for every patch). */
    static float lat_pos[(TEAPOT_GRID + 1) * (TEAPOT_GRID + 1)][3];
    static float lat_nrm[(TEAPOT_GRID + 1) * (TEAPOT_GRID + 1)][3];

    GLfloat* v = teapot.vertices;
    int p, a, b;
    for (p = 0; p < NPATCH; p++) {
        const int* pidx = patches[p];
        teapot.patch_first[p] = (int)(v - teapot.vertices) / 6;

        for (a = 0; a <= G; a++) {
            float u = (float)a / (float)G;
            for (b = 0; b <= G; b++) {
                float w = (float)b / (float)G;
                int idx = a * (G + 1) + b;
                eval_patch(pidx, u, w, lat_pos[idx], lat_nrm[idx]);
            }
        }

        for (a = 0; a < G; a++) {
            for (b = 0; b < G; b++) {
                int i00 = a * (G + 1) + b;
                int i10 = (a + 1) * (G + 1) + b;
                int i01 = a * (G + 1) + b + 1;
                int i11 = (a + 1) * (G + 1) + b + 1;

                v = emit_vert(v, lat_pos[i00], lat_nrm[i00]);
                v = emit_vert(v, lat_pos[i10], lat_nrm[i10]);
                v = emit_vert(v, lat_pos[i11], lat_nrm[i11]);

                v = emit_vert(v, lat_pos[i00], lat_nrm[i00]);
                v = emit_vert(v, lat_pos[i11], lat_nrm[i11]);
                v = emit_vert(v, lat_pos[i01], lat_nrm[i01]);
            }
        }

        teapot.patch_count[p] = (int)(v - teapot.vertices) / 6 - teapot.patch_first[p];
    }

    teapot.nvertices = (int)(v - teapot.vertices) / 6;

    /* CPU colour buffer, recomputed every frame by compute_colors(). */
    teapot.colors = (GLfloat*)malloc((size_t)teapot.nvertices * 4 * sizeof(GLfloat));
    if (teapot.colors == NULL) {
        printf("build_teapot: out of memory (colours)\n");
        return;
    }

    /* Static vec4 position VBO (x,y,z,1) built from the interleaved pos+normal. */
    {
        GLfloat *pos4 = (GLfloat*)malloc((size_t)teapot.nvertices * 4 * sizeof(GLfloat));
        int i;
        if (pos4 == NULL) {
            printf("build_teapot: out of memory (positions)\n");
            return;
        }
        for (i = 0; i < teapot.nvertices; ++i) {
            pos4[i*4+0] = teapot.vertices[i*6+0];
            pos4[i*4+1] = teapot.vertices[i*6+1];
            pos4[i*4+2] = teapot.vertices[i*6+2];
            pos4[i*4+3] = 1.0f;
        }
        glGenBuffers(1, &teapot.pos_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, teapot.pos_vbo);
        glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)teapot.nvertices * 4 * sizeof(GLfloat),
                     pos4, GL_STATIC_DRAW);
        free(pos4);
    }

    /* Dynamic vec4 colour VBO; storage allocated here, filled each frame. */
    glGenBuffers(1, &teapot.color_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, teapot.color_vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)teapot.nvertices * 4 * sizeof(GLfloat),
                 NULL, GL_STREAM_DRAW);

    if (teapot.pos_vbo == 0 || teapot.color_vbo == 0)
        printf("build_teapot: glGenBuffers failed\n");
}

/* Transform a point (w=1) by a column-major mat4, mirroring mult_m4_v4. */
static void xform_point(const GLfloat *m, GLfloat x, GLfloat y, GLfloat z, GLfloat out[3])
{
    out[0] = m[0]*x + m[4]*y + m[8]*z  + m[12];
    out[1] = m[1]*x + m[5]*y + m[9]*z  + m[13];
    out[2] = m[2]*x + m[6]*y + m[10]*z + m[14];
}

/* Exponent-48 power by repeated squaring (48 == 32 + 16), matching the former
 * vertex shader's fpow and avoiding libm powf on the embedded target. */
static float fpow48(float x)
{
    float x2 = x * x, x4 = x2 * x2, x8 = x4 * x4, x16 = x8 * x8, x32 = x16 * x16;
    return x32 * x16;
}

static void norm3(const float *v, float *o)
{
    float len = sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
    if (len < 1e-8f) { o[0] = 0.0f; o[1] = 0.0f; o[2] = 0.0f; return; }
    o[0] = v[0]/len; o[1] = v[1]/len; o[2] = v[2]/len;
}

/* Per-vertex two-sided Blinn-Phong in eye space against a positional light,
 * reproducing the former GLSL vertex shader exactly (uShade = ambient 0.22,
 * diffuse 0.78, specular 1.0; material 0.85/0.32/0.20).  The result is written
 * to teapot.colors as vec4 RGBA and uploaded as a colour attribute, so the
 * vertex shader becomes a trivial passthrough that libGLESv2 runs natively. */
static void compute_colors(const GLfloat *model_view, const GLfloat *normal_matrix,
                           const GLfloat *light_eye)
{
    const float mat[3] = { 0.85f, 0.32f, 0.20f };
    const float amb = 0.22f, dif = 0.78f, spc = 1.0f;
    int i;
    if (teapot.vertices == NULL || teapot.colors == NULL) return;
    for (i = 0; i < teapot.nvertices; ++i) {
        const GLfloat *v = teapot.vertices + (size_t)i * 6;
        GLfloat ep[3], ne[3], V[3], Ld[3], L[3], Hv[3], neg[3];
        float diff, ndoth, ndotv, spec;
        int c;

        xform_point(model_view, v[0], v[1], v[2], ep);          /* eye position */
        ne[0] = normal_matrix[0]*v[3] + normal_matrix[4]*v[4] + normal_matrix[8]*v[5];
        ne[1] = normal_matrix[1]*v[3] + normal_matrix[5]*v[4] + normal_matrix[9]*v[5];
        ne[2] = normal_matrix[2]*v[3] + normal_matrix[6]*v[4] + normal_matrix[10]*v[5];
        norm3(ne, ne);                                          /* eye normal */

        neg[0] = -ep[0]; neg[1] = -ep[1]; neg[2] = -ep[2];      /* eye at origin */
        norm3(neg, V);

        ndotv = ne[0]*V[0] + ne[1]*V[1] + ne[2]*V[2];           /* two-sided flip */
        if (ndotv < 0.0f) { ne[0] = -ne[0]; ne[1] = -ne[1]; ne[2] = -ne[2]; }

        Ld[0] = light_eye[0] - ep[0];                           /* positional light */
        Ld[1] = light_eye[1] - ep[1];
        Ld[2] = light_eye[2] - ep[2];
        norm3(Ld, L);

        diff = ne[0]*L[0] + ne[1]*L[1] + ne[2]*L[2];
        if (diff < 0.0f) diff = 0.0f;

        Hv[0] = L[0] + V[0]; Hv[1] = L[1] + V[1]; Hv[2] = L[2] + V[2];
        norm3(Hv, Hv);
        ndoth = ne[0]*Hv[0] + ne[1]*Hv[1] + ne[2]*Hv[2];
        if (ndoth < 0.0f) ndoth = 0.0f;
        spec = fpow48(ndoth);

        for (c = 0; c < 3; ++c) {
            float col = mat[c] * (amb + dif * diff) + spc * spec;
            if (col < 0.0f) col = 0.0f;
            if (col > 1.0f) col = 1.0f;
            teapot.colors[(size_t)i * 4 + c] = col;
        }
        teapot.colors[(size_t)i * 4 + 3] = 1.0f;
    }
}

static void draw_teapot(void)
{
    GLfloat view[16];
    GLfloat model_view[16];
    GLfloat normal_matrix[16];
    GLfloat mvp[16];

    /* ImGui leaves its own shader program bound after RenderDrawData (the GLES2
     * backend restores only the viewport/scissor/buffer/enable-cap state), so the
     * scene program is re-bound here every frame before drawing the teapot. */
    glUseProgram(program);

    glClearColor(0.05f, 0.06f, 0.09f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    /* Camera orbits the teapot at a fixed radius, looking at the origin. */
    float dist = 9.0f;
    float elev = camera_elev * M_PI / 180.0f;
    float azim = camera_angle * M_PI / 180.0f;
    float cam_y = dist * sinf(elev);
    float r = dist * cosf(elev);
    float cam_x = r * sinf(azim);
    float cam_z = r * cosf(azim);
    look_at(view, cam_x, cam_y, cam_z, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);

    /* Model transform: spin the teapot about its own up axis, reorient the
     * data's +Z (lid direction) to world +Y, then drop it to be centred. */
    memcpy(model_view, view, sizeof(model_view));
    translate(model_view, 0.0f, -1.3f, 0.0f);
    rotate(model_view, -90.0f, 1.0f, 0.0f, 0.0f);
    rotate(model_view, spin_angle, 0.0f, 0.0f, 1.0f);

    memcpy(mvp, projection_matrix, sizeof(mvp));
    multiply(mvp, model_view);

    memcpy(normal_matrix, model_view, sizeof(normal_matrix));
    invert(normal_matrix);
    transpose(normal_matrix);

    /* World-fixed light transformed into eye space so highlights sweep across
     * the surface as the teapot spins and the camera orbits. */
    GLfloat le[3];
    xform_point(view, 6.0f, 8.0f, 10.0f, le);

    if (teapot.pos_vbo == 0 || teapot.color_vbo == 0 ||
        teapot.vertices == NULL || teapot.colors == NULL)
        return;

    /* Light every vertex on the CPU (Blinn-Phong, formerly the vertex shader)
     * and refresh the dynamic colour VBO; the trivial passthrough VS then runs
     * on libGLESv2's native gouraud fast-path instead of the GLSL interpreter. */
    compute_colors(model_view, normal_matrix, le);

    glUniformMatrix4fv(loc_mvp, 1, GL_FALSE, mvp);

    glBindBuffer(GL_ARRAY_BUFFER, teapot.color_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    (GLsizeiptr)teapot.nvertices * 4 * sizeof(GLfloat),
                    teapot.colors);

    glBindBuffer(GL_ARRAY_BUFFER, teapot.pos_vbo);
    glVertexAttribPointer(attr_pos, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), 0);
    glEnableVertexAttribArray(attr_pos);
    glBindBuffer(GL_ARRAY_BUFFER, teapot.color_vbo);
    glVertexAttribPointer(attr_color, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), 0);
    glEnableVertexAttribArray(attr_color);

    /* One draw call per patch keeps every dispatch small while sharing VBOs. */
    int p;
    for (p = 0; p < NPATCH; p++)
        glDrawArrays(GL_TRIANGLES, teapot.patch_first[p], teapot.patch_count[p]);

    glDisableVertexAttribArray(attr_color);
    glDisableVertexAttribArray(attr_pos);
}


static void dump_egl_error(const char* where) {
  printf("%s failed, egl error 0x%x\n", where, eglGetError());
}

static GLuint compile_shader(GLenum type, const char* src) {
  GLuint s = glCreateShader(type);
  glShaderSource(s, 1, &src, NULL);
  glCompileShader(s);
  GLint ok = 0;
  glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
  if (!ok) {
    char log[1024];
    GLsizei len = 0;
    glGetShaderInfoLog(s, sizeof(log), &len, log);
    printf("shader compile failed: %.*s\n", (int)len, log);
    glDeleteShader(s);
    return 0;
  }
  return s;
}

static GLuint link_program(const char* vs_src, const char* fs_src) {
  GLuint vs = compile_shader(GL_VERTEX_SHADER, vs_src);
  GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fs_src);
  if (!vs || !fs) return 0;
  GLuint p = glCreateProgram();
  glAttachShader(p, vs);
  glAttachShader(p, fs);
  glLinkProgram(p);
  GLint ok = 0;
  glGetProgramiv(p, GL_LINK_STATUS, &ok);
  if (!ok) {
    char log[1024];
    GLsizei len = 0;
    glGetProgramInfoLog(p, sizeof(log), &len, log);
    printf("program link failed: %.*s\n", (int)len, log);
    glDeleteProgram(p);
    return 0;
  }
  glDeleteShader(vs);
  glDeleteShader(fs);
  return p;
}

/* ---- 上屏（direct / tfu），RGB565 ---- */
typedef struct {
  int sw, sh, pitch, cpp, size;
  int ox, oy;
  unsigned char* fb;
  foot_cap_t cap;
  unsigned int bo_va, bo_size, bo;
  client_t* gc;
} present_t;

static int present_init(present_t* p, const char* mode, int w, int h) {
  memset(p, 0, sizeof(*p));
  p->gc = client_get("gpu");
  if (p->gc == NULL) return -1;
  long v[8];
  for (int i = 0; i < 8; i++) {
    unsigned int param = (unsigned int)i;
    v[i] = (long)client_call_ex(p->gc, (void*)GPU_LOW_SCREEN_PARAM, &param,
                                sizeof(param), 5000);
  }
  p->sw = (int)v[2]; p->sh = (int)v[3]; p->pitch = (int)v[4];
  p->cpp = (int)v[5]; p->size = (int)v[7];
  if (p->sw <= 0 || p->sh <= 0 || p->pitch <= 0 || p->cpp <= 0 || p->size <= 0)
    return -1;
  p->ox = ((p->sw - w) / 2) & ~1;
  p->oy = (p->sh - h) / 2;
  if (p->ox < 0) p->ox = 0;
  if (p->oy < 0) p->oy = 0;

  if (strcmp(mode, "direct") == 0) {
    int fd = open("/dev/fb", O_RDWR);
    if (fd < 0) return -1;
    void* base = mmap(NULL, (size_t)p->size, PROT_READ | PROT_WRITE,
                      MAP_SHARED, fd, 0);
    close(fd);
    if (base == MAP_FAILED) return -1;
    p->fb = (unsigned char*)base;
  } else {
    unsigned int sz = 0, pa = 0;
    p->cap = foot_cap_alloc_arena((unsigned long)w * h * 2, 0, 0, &pa, &sz);
    if (p->cap == FOOT_CAP_NONE) return -1;
    void* va = (void*)0x54000000UL;
    if (foot_cap_map(p->cap, va, 0, sz) != 0) return -1;
    p->bo_va = (unsigned int)(uintptr_t)va;
    p->bo_size = sz;
    gpu_bo_create_req req;
    memset(&req, 0, sizeof(req));
    req.pa = pa; req.size = sz;
    long handle = (long)client_call_ex(p->gc, (void*)GPU_LOW_BO_CREATE, &req,
                                       sizeof(req), 60000);
    if (handle <= 0) return -1;
    p->bo = (unsigned int)handle;
  }
  return 0;
}

static void present_frame(present_t* p, const uint8_t* rgba, int w, int h) {
  /* GL 的 FBO 是 +Y 向上，glReadPixels 第 0 行是图像底部；屏幕第 0 行是
   * 顶部，因此这里按行翻转。 */
  if (p->fb != NULL) {
    uint16_t* base = (uint16_t*)p->fb;
    int pitch_px = p->pitch / p->cpp;
    for (int y = 0; y < h; y++) {
      uint16_t* row = base + (size_t)(p->oy + y) * pitch_px + p->ox;
      const uint8_t* src = rgba + (size_t)(h - 1 - y) * w * 4;
      for (int x = 0; x < w; x++) {
        uint8_t r = src[x * 4 + 0], g = src[x * 4 + 1], b = src[x * 4 + 2];
        row[x] = (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
      }
    }
    foot_cache_clean(p->fb, (char*)p->fb + p->size);
  } else {
    uint16_t* dst = (uint16_t*)(uintptr_t)p->bo_va;
    for (int y = 0; y < h; y++) {
      const uint8_t* src = rgba + (size_t)(h - 1 - y) * w * 4;
      for (int x = 0; x < w; x++) {
        uint8_t r = src[x * 4 + 0], g = src[x * 4 + 1], b = src[x * 4 + 2];
        dst[(size_t)y * w + x] =
            (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
      }
    }
    foot_cache_clean(dst, (char*)dst + (size_t)w * h * 2);
    gpu_submit_tfu_req tr;
    memset(&tr, 0, sizeof(tr));
    tr.src_handle = p->bo;
    tr.dst_handle = 0;
    tr.w = (unsigned int)w;
    tr.h = (unsigned int)h;
    tr.src_fmt = 6;
    tr.dst_fmt = 6;
    tr.dst_offset = (unsigned int)p->oy * (unsigned int)p->pitch +
                    (unsigned int)p->ox * (unsigned int)p->cpp;
    long rc = (long)client_call_ex(p->gc, (void*)GPU_LOW_SUBMIT_TFU, &tr,
                                   sizeof(tr), 10000);
    if (rc != 0) printf("present: TFU failed (%ld)\n", rc);
  }
}

int main(int argc, char** argv) {
  int present_mode = argc > 1 && (strcmp(argv[1], "direct") == 0 ||
                                  strcmp(argv[1], "tfu") == 0);
  int frames = (argc > 2) ? atoi(argv[2]) : 300;
  if (frames <= 0) frames = 300;
  setvbuf(stdout, NULL, _IONBF, 0);
  printf("=== test-gles-teapot (%s) ===\n", present_mode ? argv[1] : "selftest");

  PFNEGLQUERYDEVICESEXTPROC query_devices =
      (PFNEGLQUERYDEVICESEXTPROC)eglGetProcAddress("eglQueryDevicesEXT");
  PFNEGLGETPLATFORMDISPLAYEXTPROC get_platform_display =
      (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
  if (query_devices == NULL || get_platform_display == NULL) {
    printf("EGL device entrypoints missing\n");
    return 1;
  }
  EGLDeviceEXT devs[8];
  EGLint ndev = 0;
  if (!query_devices(8, devs, &ndev) || ndev < 1) {
    printf("eglQueryDevicesEXT failed\n");
    return 1;
  }
  EGLDisplay dpy = get_platform_display(EGL_PLATFORM_DEVICE_EXT, (void*)devs[0], NULL);
  EGLint major = 0, minor = 0;
  if (dpy == EGL_NO_DISPLAY || !eglInitialize(dpy, &major, &minor)) {
    dump_egl_error("eglInitialize");
    return 1;
  }
  printf("EGL %d.%d, vendor=%s\n", major, minor, eglQueryString(dpy, EGL_VENDOR));
  if (!eglBindAPI(EGL_OPENGL_ES_API)) { dump_egl_error("eglBindAPI"); return 1; }

  EGLint cfg_attribs[] = {
      EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
      EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
      EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 16, EGL_NONE,
  };
  EGLConfig cfg;
  EGLint ncfg = 0;
  if (!eglChooseConfig(dpy, cfg_attribs, &cfg, 1, &ncfg) || ncfg < 1) {
    dump_egl_error("eglChooseConfig");
    return 1;
  }
  EGLint ctx_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
  EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctx_attribs);
  if (ctx == EGL_NO_CONTEXT) { dump_egl_error("eglCreateContext"); return 1; }
  if (!eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx)) {
    dump_egl_error("eglMakeCurrent");
    return 1;
  }
  printf("GL_VERSION=%s\n", glGetString(GL_VERSION));
  printf("GL_RENDERER=%s\n", glGetString(GL_RENDERER));

  /* FBO: RGBA8 + DEPTH16 */
  GLuint tex = 0, fbo = 0, rbo = 0;
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glGenRenderbuffers(1, &rbo);
  glBindRenderbuffer(GL_RENDERBUFFER, rbo);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, W, H);
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    printf("FBO incomplete\n");
    return 1;
  }

  program = link_program(teapot_vs, teapot_fs);
  if (!program) return 1;
  glUseProgram(program);
  loc_mvp = glGetUniformLocation(program, "uMVP");
  attr_pos = glGetAttribLocation(program, "aPosition");
  attr_color = glGetAttribLocation(program, "aColor");
  if (attr_pos < 0 || attr_color < 0) {
    printf("attribute location failed (%d/%d)\n", attr_pos, attr_color);
    return 1;
  }

  perspective(projection_matrix, 45.0f, (GLfloat)W / (GLfloat)H, 0.5f, 100.0f);
  glViewport(0, 0, W, H);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);

  build_teapot();
  if (teapot.vertices == NULL || teapot.colors == NULL || teapot.pos_vbo == 0) {
    printf("teapot build failed\n");
    return 1;
  }
  printf("utah teapot: %d vertices\n", teapot.nvertices);

  static uint8_t pixels[W * H * 4];
  present_t pres;
  int have_pres = 0;
  if (present_mode) {
    if (present_init(&pres, argv[1], W, H) != 0) {
      printf("present init failed\n");
      return 1;
    }
    have_pres = 1;
    printf("present: mode=%s screen=%dx%d pitch=%d ox=%d oy=%d\n", argv[1],
           pres.sw, pres.sh, pres.pitch, pres.ox, pres.oy);
  }

  int nframes = present_mode ? frames : 2;
  int fail = 0;
  int nonbg[2] = {0, 0};
  int c0r = 0, c0g = 0, c0b = 0;
  for (int f = 0; f < nframes; f++) {
    if (present_mode) {
      /* 45 deg/s 自转，20 deg/s 相机环绕，33ms 一帧 */
      spin_angle += 45.0f * (33.0f / 1000.0f);
      if (spin_angle >= 360.0f) spin_angle -= 360.0f;
      camera_angle += 20.0f * (33.0f / 1000.0f);
      if (camera_angle >= 360.0f) camera_angle -= 360.0f;
    } else {
      spin_angle = (f == 0) ? 0.0f : 70.0f;
      camera_angle = (f == 0) ? 0.0f : 25.0f;
    }

    draw_teapot();
    glFinish();
    glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

    if (!present_mode) {
      for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
          const uint8_t* p = pixels + ((size_t)y * W + x) * 4;
          int dx = (int)p[0] - 13, dy = (int)p[1] - 15, dz = (int)p[2] - 23;
          if (dx * dx + dy * dy + dz * dz > 900) nonbg[f]++;
        }
      }
      const uint8_t* c = pixels + ((size_t)(H / 2) * W + W / 2) * 4;
      if (f == 0) { c0r = c[0]; c0g = c[1]; c0b = c[2]; }
      printf("frame%d nonbg=%d center=%u,%u,%u\n", f, nonbg[f], c[0], c[1],
             c[2]);
    } else {
      present_frame(&pres, pixels, W, H);
      if ((f & 31) == 0) printf("frame %d/%d\n", f, nframes);
      usleep(33000);
    }
  }

  if (!present_mode) {
    if (nonbg[0] < 5000 || nonbg[1] < 5000) {
      printf("teapot coverage too small\n");
      fail = 1;
    }
    if (nonbg[0] == nonbg[1]) {
      printf("rotation did not change coverage\n");
      fail = 1;
    }
    if (!(c0r > 20 && c0r > c0b)) {
      printf("center pixel is not lit teapot\n");
      fail = 1;
    }
  }

  if (have_pres) {
    if (pres.fb != NULL) munmap(pres.fb, (size_t)pres.size);
    if (pres.cap != FOOT_CAP_NONE) {
      foot_cap_unmap(pres.cap);
      foot_cap_revoke(pres.cap);
    }
    if (pres.bo != 0) {
      unsigned int handle = pres.bo;
      client_call_ex(pres.gc, (void*)GPU_LOW_BO_DESTROY, &handle,
                     sizeof(handle), 5000);
    }
  }

  eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
  eglDestroyContext(dpy, ctx);
  eglTerminate(dpy);
  printf("=== %s ===\n", fail ? "FAIL" : "PASS");
  return fail;
}
