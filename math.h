#ifndef _MATH_H_

#define _MATH_H_

#define m_min(a, b)            ((a) < (b) ? (a) : (b))
#define m_max(a, b)            ((a) > (b) ? (a) : (b))
#define m_abs(a, b)            ((a) >  0  ? (a) : -(a))
#define m_mod(a, m)            (((a) % (m)) >= 0 ? ((a) % (m)) : (((a) % (m)) + (m)))
#define m_clamp(x, a, b)       (m_min(b, m_max(a, x)))
#define m_square(a)            ((a)*(a))
//#define INFINITY (*(float *)&positive_inf)

#define vec3_f(f) ((Vec3) { f, f, f })
#define vec3_x ((Vec3) { 1.0f, 0.0f, 0.0f })
#define vec3_y ((Vec3) { 0.0f, 1.0f, 0.0f })
#define vec3_z ((Vec3) { 0.0f, 0.0f, 1.0f })

#define sat_i8(x) (int8_t) (m_clamp((x), -128, 127))

#define PI_f (3.14159265359f)

const uint32_t positive_inf = 0x7F800000; // 0xFF << 23

typedef struct { float x, y; } Vec2;
typedef struct { float x, y, z; } Vec3;
typedef struct { float x, y, z, w; } Vec4;
typedef union {
    float nums[4][4];
    struct { Vec4 x, y, z, w; };
    Vec4 cols[4];
} Mat4;
typedef union {
    struct {
        union { Vec3 xyz; struct { float x,y,z; }; };
        float w;
    };
    float elements[4];
} Quaternion;

//Utillity
float to_radians(float degrees);
float lerp(float a, float b, float t);
float fabsf(float f);
float sign(float f);
float step(float edge, float x);

//Vector operations
//2d
Vec2 vec2(float x, float y);
Vec2 add2(Vec2 a, Vec2 b);
Vec2 add2_f(Vec2 a, float f);
Vec2 sub2(Vec2 a, Vec2 b);
Vec2 sub2_f(Vec2 a, float f);
Vec2 div2(Vec2 a, Vec2 b);
Vec2 div2_f(Vec2 a, float f);
Vec2 mul2(Vec2 a, Vec2 b);
Vec2 mul2_f(Vec2 a, float f);
float dot2(Vec2 a, Vec2 b);
float mag2(Vec2 a);
float magmag2(Vec2 a);
Vec2 norm2(Vec2 a);
Vec2 abs2(Vec2 a);
Vec2 sign2(Vec2 a);
Vec2 vec2_rot(float rot);
float rot_vec2(Vec2 rot);

//3d
Vec3 vec3(float x, float y, float z);
Vec3 add3(Vec3 a, Vec3 b);
Vec3 add3_f(Vec3 a, float f);
Vec3 sub3(Vec3 a, Vec3 b);
Vec3 sub3_f(Vec3 a, float f);
Vec3 div3(Vec3 a, Vec3 b);
Vec3 div3_f(Vec3 a, float f);
Vec3 mul3(Vec3 a, Vec3 b);
Vec3 mul3_f(Vec3 a, float f);
float dot3(Vec3 a, Vec3 b);
float mag3(Vec3 a);
float magmag3(Vec3 a);
Vec3 norm3(Vec3 a);
Vec3 abs3(Vec3 a);
Vec3 sign3(Vec3 a);
Vec3 max3_f(Vec3 v, float f);
Vec3 yzx3(Vec3 v);
Vec3 zxy3(Vec3 v);
Vec3 step3(Vec3 a, Vec3 b);
Vec3 lerp3(Vec3 a, Vec3 b, float t);
Vec3 cross3(Vec3 a, Vec3 b);

//4d
Vec4 vec4(float x, float y, float z, float w);
Vec4 add4(Vec4 a, Vec4 b);
Vec4 add4_f(Vec4 a, float f);
Vec4 sub4(Vec4 a, Vec4 b);
Vec4 sub4_f(Vec4 a, float f);
Vec4 div4(Vec4 a, Vec4 b);
Vec4 div4_f(Vec4 a, float f);
Vec4 mul4(Vec4 a, Vec4 b);
Vec4 mul4_f(Vec4 a, float f);
float dot4(Vec4 a, Vec4 b);
float mag4(Vec4 a);
float magmag4(Vec4 a);
Vec4 norm4(Vec4 a);
Vec4 abs4(Vec4 a);
Vec4 sign4(Vec4 a);

//Matrix
Mat4 mul4x4(Mat4 a, Mat4 b);
Mat4 ident4x4();
Mat4 transpose4x4(Mat4 a);
Mat4 translate4x4(Vec3 pos);
Mat4 rotate4x4(Vec3 axis, float angle);
Mat4 perspective4x4(float fov, float aspect, float near, float far);
Mat4 look_at4x4(Vec3 eye, Vec3 focus, Vec3 up);

//Uncomment to use in true single header style
#define MATH_IMPLEMENTATION

#ifdef MATH_IMPLEMENTATION
#ifndef MATH_IMPLEMENTATION_ONCE
#define MATH_IMPLEMENTATION_ONCE

float to_radians(float degrees) {
   float result = degrees * (PI_f / 180.0f);

   return (result);
}

float lerp(float a, float b, float t) {
   return (1.0f-t)*a+t*b;
}

float fabsf(float f) {
    return (f < 0.0f) ? -f : f;
}

float sign(float f) {
    if (f > 0.0) return -1.0f;
    if (f < 0.0) return  1.0f;
    else         return  0.0f;
}

float step(float edge, float x) {
    return (x < edge) ? 0.0f : 1.0f;
}

Vec2 vec2(float x, float y) {
    return (Vec2) { x, y };
}

Vec3 vec3(float x, float y, float z) {
    return (Vec3) { x, y, z };
}

Vec4 vec4(float x, float y, float z, float w) {
    return (Vec4) { x, y, z, w};
}

Vec2 add2(Vec2 a, Vec2 b) {
   return vec2(a.x+b.x,a.y+b.y);
}

Vec3 add3(Vec3 a, Vec3 b) {
   return vec3(a.x+b.x,a.y+b.y,a.z+b.z);
}

Vec4 add4(Vec4 a, Vec4 b) {
   return vec4(a.x+b.x,a.y+b.y,a.z+b.z,a.w+b.w);
}

Vec2 sub2(Vec2 a, Vec2 b) {
   return vec2(a.x-b.x,a.y-b.y);
}

Vec3 sub3(Vec3 a, Vec3 b) {
   return vec3(a.x-b.x,a.y-b.y,a.z-b.z);
}

Vec4 sub4(Vec4 a, Vec4 b) {
   return vec4(a.x-b.x,a.y-b.y,a.z-b.z,a.w-b.w);
}

Vec2 add2_f(Vec2 a, float f) {
   return vec2(a.x+f,a.y+f);
}

Vec3 add3_f(Vec3 a, float f) {
   return vec3(a.x+f,a.y+f,a.z+f);
}

Vec4 add4_f(Vec4 a, float f) {
   return vec4(a.x+f,a.y+f,a.z+f,a.w+f);
}

Vec2 sub2_f(Vec2 a, float f) {
   return vec2(a.x-f,a.y-f);
}

Vec3 sub3_f(Vec3 a, float f) {
   return vec3(a.x-f,a.y-f,a.z-f);
}

Vec4 sub4_f(Vec4 a, float f) {
   return vec4(a.x-f,a.y-f,a.z-f,a.w-f);
}

Vec2 div2(Vec2 a, Vec2 b) {
   return vec2(a.x/b.x,a.y/b.y);
}

Vec3 div3(Vec3 a, Vec3 b) {
   return vec3(a.x/b.x,a.y/b.y,a.z/b.z);
}

Vec4 div4(Vec4 a, Vec4 b) {
   return vec4(a.x/b.x,a.y/b.y,a.z/b.z,a.w/b.w);
}

Vec2 div2_f(Vec2 a, float f) {
   return vec2(a.x/f,a.y/f);
}

Vec3 div3_f(Vec3 a, float f) {
   return vec3(a.x/f,a.y/f,a.z/f);
}

Vec4 div4_f(Vec4 a, float f) {
   return vec4(a.x/f,a.y/f,a.z/f,a.w/f);
}

Vec2 mul2(Vec2 a, Vec2 b) {
   return vec2(a.x*b.x,a.y*b.y);
}

Vec3 mul3(Vec3 a, Vec3 b) {
   return vec3(a.x*b.x,a.y*b.y,a.z*b.z);
}

Vec4 mul4(Vec4 a, Vec4 b) {
   return vec4(a.x*b.x,a.y*b.y,a.z*b.z,a.w*b.w);
}

Vec2 mul2_f(Vec2 a, float f) {
   return vec2(a.x*f,a.y*f);
}

Vec3 mul3_f(Vec3 a, float f) {
   return vec3(a.x*f,a.y*f,a.z*f);
}

Vec4 mul4_f(Vec4 a, float f) {
   return vec4(a.x*f,a.y*f,a.z*f,a.w*f);
}

float dot2(Vec2 a, Vec2 b) {
   return a.x*b.x+a.y*b.y;
}

float dot3(Vec3 a, Vec3 b) {
   return a.x*b.x+a.y*b.y+a.z*b.z;
}

float dot4(Vec4 a, Vec4 b) {
   return a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w;
}

float mag2(Vec2 a) {
    return sqrtf(dot2(a, a));
}

float mag3(Vec3 a) {
    return sqrtf(dot3(a, a));
}

float mag4(Vec4 a) {
    return sqrtf(dot4(a, a));
}

float magmag2(Vec2 a) {
    return dot2(a, a);
}

float magmag3(Vec3 a) {
    return dot3(a, a);
}

float magmag4(Vec4 a) {
    return dot4(a, a);
}

Vec2 norm2(Vec2 a) {
    return div2_f(a, mag2(a));
}

Vec3 norm3(Vec3 a) {
    return div3_f(a, mag3(a));
}

Vec4 norm4(Vec4 a) {
    return div4_f(a, mag4(a));
}

Vec2 abs2(Vec2 a) {
    return vec2(fabsf(a.x), fabsf(a.y));
}

Vec3 abs3(Vec3 a) {
    return vec3(fabsf(a.x), fabsf(a.y), fabsf(a.z));
}

Vec4 abs4(Vec4 a) {
    return vec4(fabsf(a.x), fabsf(a.y), fabsf(a.z), fabsf(a.w));
}

Vec2 sign2(Vec2 a) {
    return vec2(sign(a.x), sign(a.y));
}

Vec3 sign3(Vec3 a) {
    return vec3(sign(a.x), sign(a.y), sign(a.z));
}

Vec4 sign4(Vec4 a) {
    return vec4(sign(a.x), sign(a.y), sign(a.z), sign(a.w));
}

Vec2 vec2_rot(float rot) {
    return vec2(cosf(rot), sinf(rot));
}

float rot_vec2(Vec2 rot) {
    return atan2f(rot.y, rot.x);
}

Vec3 max3_f(Vec3 v, float f) {
    return vec3(m_max(v.x, f), m_max(v.y, f), m_max(v.z, f));
}

Vec3 yzx3(Vec3 v) { return vec3(v.y, v.z, v.x); }
Vec3 zxy3(Vec3 v) { return vec3(v.z, v.x, v.y); }
Vec3 step3(Vec3 a, Vec3 b) {
    return vec3(step(a.x, b.x), step(a.y, b.y), step(a.z, b.z));
}

Vec3 lerp3(Vec3 a, Vec3 b, float t) {
    return add3(mul3_f(a, 1.0f - t), mul3_f(b, t));
}

Vec3 cross3(Vec3 a, Vec3 b) {
    return vec3((a.y * b.z) - (a.z * b.y),
                (a.z * b.x) - (a.x * b.z),
                (a.x * b.y) - (a.y * b.x));
}

Mat4 mul4x4(Mat4 a, Mat4 b) {
    Mat4 out = {0};
    int8_t k, r, c;
    for (c = 0; c < 4; ++c)
        for (r = 0; r < 4; ++r) {
            out.nums[c][r] = 0.0f;
            for (k = 0; k < 4; ++k)
                out.nums[c][r] += a.nums[k][r] * b.nums[c][k];
        }
    return out;
}

Mat4 ident4x4() {
    Mat4 res = {0};
    res.nums[0][0] = 1.0f;
    res.nums[1][1] = 1.0f;
    res.nums[2][2] = 1.0f;
    res.nums[3][3] = 1.0f;
    return res;
}

Mat4 transpose4x4(Mat4 a) {
    Mat4 res;
    for(int c = 0; c < 4; ++c)
        for(int r = 0; r < 4; ++r)
            res.nums[r][c] = a.nums[c][r];
    return res;
}

Mat4 translate4x4(Vec3 pos) {
    Mat4 res = ident4x4();
    res.nums[3][0] = pos.x;
    res.nums[3][1] = pos.y;
    res.nums[3][2] = pos.z;
    return res;
}

Mat4 rotate4x4(Vec3 axis, float angle) {
    Mat4 res = ident4x4();

    axis = norm3(axis);

    float sin_theta = sinf(angle);
    float cos_theta = cosf(angle);
    float cos_value = 1.0f - cos_theta;

    res.nums[0][0] = (axis.x * axis.x * cos_value) + cos_theta;
    res.nums[0][1] = (axis.x * axis.y * cos_value) + (axis.z * sin_theta);
    res.nums[0][2] = (axis.x * axis.z * cos_value) - (axis.y * sin_theta);

    res.nums[1][0] = (axis.y * axis.x * cos_value) - (axis.z * sin_theta);
    res.nums[1][1] = (axis.y * axis.y * cos_value) + cos_theta;
    res.nums[1][2] = (axis.y * axis.z * cos_value) + (axis.x * sin_theta);

    res.nums[2][0] = (axis.z * axis.x * cos_value) + (axis.y * sin_theta);
    res.nums[2][1] = (axis.z * axis.y * cos_value) - (axis.x * sin_theta);
    res.nums[2][2] = (axis.z * axis.z * cos_value) + cos_theta;

    return res;
}

/* equivalent to XMMatrixPerspectiveFovLH
   https://docs.microsoft.com/en-us/windows/win32/api/directxmath/nf-directxmath-xmmatrixperspectivefovlh
*/
Mat4 perspective4x4(float fov, float aspect, float near, float far) {
    fov *= 0.5f;
    float height = cosf(fov) / sinf(fov);
    float width = height / aspect;
    float f_range = far / (far - near);

    Mat4 res = {0};
    res.nums[0][0] = width;
    res.nums[1][1] = height;
    res.nums[2][3] = 1.0f;
    res.nums[2][2] = f_range;
    res.nums[3][2] = -f_range * near;
    return res;
}

Mat4 look_at4x4(Vec3 eye, Vec3 focus, Vec3 up) {
    Vec3 eye_dir = sub3(focus, eye);
    Vec3 R2 = norm3(eye_dir);

    Vec3 R0 = norm3(cross3(up, R2));
    Vec3 R1 = cross3(R2, R0);

    Vec3 neg_eye = mul3_f(eye, -1.0f);

    float D0 = dot3(R0, neg_eye);
    float D1 = dot3(R1, neg_eye);
    float D2 = dot3(R2, neg_eye);

    return (Mat4) {{
        { R0.x, R1.x, R2.x, 0.0f },
        { R0.y, R1.y, R2.y, 0.0f },
        { R0.z, R1.z, R2.z, 0.0f },
        {   D0,   D1,   D2, 1.0f }
    }};
}

#endif
#endif
#endif
