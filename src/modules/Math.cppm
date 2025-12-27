module;
#include <cmath>
#include <cstring>
export module VoxelGame.Math;

export namespace VoxelGame::Math {
    struct Vec3 { float x, y, z; };
    struct Mat4 { float m[16]; };

    Mat4 Identity() {
        Mat4 res;
        std::memset(res.m, 0, sizeof(float) * 16);
        res.m[0] = 1.0f; res.m[5] = 1.0f; res.m[10] = 1.0f; res.m[15] = 1.0f;
        return res;
    }

    Mat4 Perspective(float fov, float aspect, float znear, float zfar) {
        Mat4 res = {};
        float tanHalfFov = std::tan(fov / 2.0f);
        res.m[0] = 1.0f / (aspect * tanHalfFov);
        res.m[5] = 1.0f / (tanHalfFov);
        res.m[10] = -(zfar + znear) / (zfar - znear);
        res.m[11] = -1.0f;
        res.m[14] = -(2.0f * zfar * znear) / (zfar - znear);
        return res;
    }

    Vec3 Normalize(Vec3 v) {
        float len = std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
        if(len == 0) return {0,0,0};
        return {v.x/len, v.y/len, v.z/len};
    }

    Vec3 Cross(Vec3 a, Vec3 b) {
        return {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x};
    }

    Vec3 Sub(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
    float Dot(Vec3 a, Vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }

    Mat4 LookAt(Vec3 eye, Vec3 center, Vec3 up) {
        Vec3 f = Normalize(Sub(center, eye));
        Vec3 s = Normalize(Cross(f, up));
        Vec3 u = Cross(s, f);

        Mat4 res = Identity();
        res.m[0] = s.x; res.m[4] = s.y; res.m[8] = s.z;
        res.m[1] = u.x; res.m[5] = u.y; res.m[9] = u.z;
        res.m[2] = -f.x; res.m[6] = -f.y; res.m[10] = -f.z;
        res.m[12] = -Dot(s, eye);
        res.m[13] = -Dot(u, eye);
        res.m[14] = Dot(f, eye);
        return res;
    }
}