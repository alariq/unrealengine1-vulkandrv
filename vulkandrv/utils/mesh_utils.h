#pragma once

#include "vec.h"
#include <stdint.h>

struct SVD {
    vec3 pos;
    vec3 normal;
    vec2 uv;
};

template <typename IB_t = uint16_t>
struct SVDAdapter {
    typedef IB_t ib_type;

    SVD *vb_ = nullptr;
    ib_type *ib_ = nullptr;
    size_t vb_size_ = 0;
    size_t ib_size_ = 0;
    size_t offset_ = 0;

    enum { kVertexSize = sizeof(SVD) };

    void allocate_vb(size_t size) {
        SVD* new_vb = new SVD[vb_size_ + size];
        memcpy(new_vb, vb_, sizeof(SVD)*vb_size_);
        delete[] vb_;
        vb_ = new_vb;
        vb_size_ += size;
    }
    void allocate_ib(size_t size) {
        ib_type* new_ib = new ib_type[ib_size_ + size];
        memcpy(new_ib, ib_, sizeof(ib_type)*ib_size_);
        delete[] ib_;
        ib_ = new_ib;
        ib_size_ += size;
    }
    ~SVDAdapter() {
        delete[] vb_;
        delete[] ib_;
    }
#if 0
    AABB get_aabb() const {
        AABB aabb(vec3(0),vec3(0));
        if(vb_size_ > 0) {
            aabb.min_ = vb_[0].pos;
            aabb.max_ = vb_[0].pos;
            for(size_t i=1; i<vb_size_; ++i) {
                aabb.min_ = min(vb_[i].pos, aabb.min_);
                aabb.max_ = max(vb_[i].pos, aabb.max_);
            }
        }
        return aabb;
    }
#endif

    void set_offset(size_t offset) { offset_ = offset; }
    void p(unsigned int i, vec3 p) { vb_[i+offset_].pos = p; }
    void n(unsigned int i, vec3 n) { vb_[i+offset_].normal = n; }
    void uv(unsigned int i, vec2 uv) { vb_[i+offset_].uv = uv; }
    void i(unsigned int i, ib_type idx) { ib_[i] = idx; }
};


// counter-clockwise order
template<typename MeshBuffer>
void generate_cube(MeshBuffer& mb, const vec3 scale, const vec3 offset)
{
    mb.allocate_vb(36);
    vec3 xyz = vec3(1,1,1)*scale + offset;
    vec3 _yz = vec3(-1,1,1)*scale + offset;
    vec3 x_z = vec3(1,-1,1)*scale + offset;
    vec3 xy_ = vec3(1,1,-1)*scale + offset;
    vec3 x__ = vec3(1,-1,-1)*scale + offset;
    vec3 _y_ = vec3(-1,1,-1)*scale + offset;
    vec3 __z = vec3(-1,-1,1)*scale + offset;
    vec3 ___ = vec3(-1,-1,-1)*scale + offset;

    vec3 nx = vec3(1,0,0);
    vec3 ny = vec3(0,1,0);
    vec3 nz = vec3(0,0,1);

    vec2 uv = vec2(1,1);
    vec2 u_ = vec2(1,0);
    vec2 _v = vec2(0,1);
    vec2 __ = vec2(0,0);

    // front faces

	// face v0-v1-v2
    mb.p(0, xyz); mb.uv(0, uv); mb.n(0, nz);
    mb.p(1, _yz); mb.uv(1, _v); mb.n(1, nz);
    mb.p(2, __z); mb.uv(2, __); mb.n(2, nz);
	// face v2-v3-v0
    mb.p(3, __z); mb.uv(3, __); mb.n(3, nz);
    mb.p(4, x_z); mb.uv(4, u_); mb.n(4, nz);
    mb.p(5, xyz), mb.uv(5, uv); mb.n(5, nz);

	// right faces
    
	// face v0-v3-v4
    mb.p(6, xyz), mb.uv(6, _v), mb.n(6, nx);
    mb.p(7, x_z), mb.uv(7, __), mb.n(7, nx);
    mb.p(8, x__), mb.uv(8, u_), mb.n(8, nx);
	// face v4-v5-v0
    mb.p(9, x__), mb.uv(9, u_), mb.n(9, nx);
    mb.p(10, xy_), mb.uv(10, uv), mb.n(10, nx);
    mb.p(11, xyz), mb.uv(11, _v), mb.n(11, nx);

	// top faces
    
	// face v0-v5-v6
    mb.p(12, xyz), mb.uv(12, u_), mb.n(12, ny);
    mb.p(13, xy_), mb.uv(13, uv), mb.n(13, ny);
    mb.p(14, _y_), mb.uv(14, _v), mb.n(14, ny);
	// face v6-v1-v0
    mb.p(15, _y_), mb.uv(15, _v), mb.n(15, ny);
    mb.p(16, _yz), mb.uv(16, __), mb.n(16, ny);
    mb.p(17, xyz), mb.uv(17, u_), mb.n(17, ny);

	// left faces
   
	// face  v1-v6-v7
    mb.p(18, _yz), mb.uv(18, uv), mb.n(18, -nx);
    mb.p(19, _y_), mb.uv(19, _v), mb.n(19, -nx);
    mb.p(20, ___), mb.uv(20, __), mb.n(20, -nx);
	// face v7-v2-v1
    mb.p(21, ___), mb.uv(21, __), mb.n(21, -nx);
    mb.p(22, __z), mb.uv(22, u_), mb.n(22, -nx);
    mb.p(23, _yz), mb.uv(23, uv), mb.n(23, -nx);

	// bottom faces
   
	// face v7-v4-v3
    mb.p(24, ___), mb.uv(24, __), mb.n(24, -ny);
    mb.p(25, x__), mb.uv(25, u_), mb.n(25, -ny);
    mb.p(26, x_z), mb.uv(26, uv), mb.n(26, -ny);
	// face v3-v2-v7
    mb.p(27, x_z), mb.uv(27, uv), mb.n(27, -ny);
    mb.p(28, __z), mb.uv(28, _v), mb.n(28, -ny);
    mb.p(29, ___), mb.uv(29, __), mb.n(29, -ny);

	// back faces
    
	// face v4-v7-v6
    mb.p(30, x__), mb.uv(30, __), mb.n(30, -nz);
    mb.p(31, ___), mb.uv(31, u_), mb.n(31, -nz);
    mb.p(32, _y_), mb.uv(32, uv), mb.n(32, -nz);
	// face v6-v5-v4
    mb.p(33, _y_), mb.uv(33, uv), mb.n(33, -nz);
    mb.p(34, xy_), mb.uv(34, _v), mb.n(34, -nz);
    mb.p(35, x__), mb.uv(35, __), mb.n(35, -nz);
}

