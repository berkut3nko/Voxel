module;
#include <cmath>
#include <cstring>
#include <array>
export module VoxelGame.Math;

export namespace VoxelGame::Math {
    struct Vec3 { float x, y, z; };
    struct Vec4 { float x, y, z, w; };
    struct Mat4 { float m[16]; };
    
    // --- Попередні функції ---
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
    Vec3 Cross(Vec3 a, Vec3 b) { return {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x}; }
    Vec3 Sub(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
    Vec3 Add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
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

   // --- Frustum Culling Helpers (Local impl due to file restrictions) ---
    struct Plane {
        float x, y, z, w;
        void normalize() {
            float len = std::sqrt(x*x + y*y + z*z);
            x /= len; y /= len; z /= len; w /= len;
        }
    };

    struct Frustum {
        Plane planes[6];
    };


    // Просте множення матриць 4x4 (локальне, якщо немає в Math.cppm)
    Mat4 MultiplyMat4(const Mat4& A, const Mat4& B) {
        Mat4 C = {};
        for(int i=0; i<4; i++) {
            for(int j=0; j<4; j++) {
                C.m[i*4+j] = 
                    A.m[0*4+j] * B.m[i*4+0] +
                    A.m[1*4+j] * B.m[i*4+1] +
                    A.m[2*4+j] * B.m[i*4+2] +
                    A.m[3*4+j] * B.m[i*4+3];
            }
        }
        return C;
    }

    Frustum CreateFrustum(const Mat4& vp) {
        Frustum f;
        const float* m = vp.m;

        // Left
        f.planes[0] = {m[3] + m[0], m[7] + m[4], m[11] + m[8], m[15] + m[12]};
        // Right
        f.planes[1] = {m[3] - m[0], m[7] - m[4], m[11] - m[8], m[15] - m[12]};
        // Bottom
        f.planes[2] = {m[3] + m[1], m[7] + m[5], m[11] + m[9], m[15] + m[13]};
        // Top
        f.planes[3] = {m[3] - m[1], m[7] - m[5], m[11] - m[9], m[15] - m[13]};
        // Near
        f.planes[4] = {m[3] + m[2], m[7] + m[6], m[11] + m[10], m[15] + m[14]};
        // Far
        f.planes[5] = {m[3] - m[2], m[7] - m[6], m[11] - m[10], m[15] - m[14]};

        for(int i=0; i<6; i++) f.planes[i].normalize();
        return f;
    }

    bool FrustumCheckAABB(const Frustum& f, float minX, float minY, float minZ, float maxX, float maxY, float maxZ) {
        for(int i=0; i<6; i++) {
            float px = (f.planes[i].x > 0) ? maxX : minX;
            float py = (f.planes[i].y > 0) ? maxY : minY;
            float pz = (f.planes[i].z > 0) ? maxZ : minZ;

            if(f.planes[i].x*px + f.planes[i].y*py + f.planes[i].z*pz + f.planes[i].w < 0)
                return false;
        }
        return true;
    }
}