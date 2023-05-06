//-----------------------------------------------------------------------------
// The confidential and proprietary information contained in this file may
// only be used by a person authorised under and to the extent permitted
// by a subsisting licensing agreement from EMBEDEEP Corporation.
//
//    (C) COPYRIGHT 2018-2022  EMBEDEEP Corporation.
//        ALL RIGHTS RESERVED
//
// This entire notice must be reproduced on all copies of this file
// and copies of this file may only be made by a person if such person is
// permitted to do so under the terms of a subsisting license agreement
// from EMBEDEEP Corporation.
//
//      Main version        : V0.0.1
//
//      Revision            : 2022-10-08
//
//-----------------------------------------------------------------------------

#ifndef SRC_EEPTPU_NMAT_H_
#define SRC_EEPTPU_NMAT_H_



#include <stdlib.h>
#include <string.h>

namespace ncnn {

// the alignment of all the allocated buffers
#define MALLOC_ALIGN    16

// Aligns a pointer to the specified number of bytes
// ptr Aligned pointer
// n Alignment size that must be a power of two
template<typename _Tp> static inline _Tp* alignPtr(_Tp* ptr, int n=(int)sizeof(_Tp))
{
    return (_Tp*)(((size_t)ptr + n-1) & -n);
}

// exchange-add operation for atomic operations on reference counters
#if defined __INTEL_COMPILER && !(defined WIN32 || defined _WIN32)
// atomic increment on the linux version of the Intel(tm) compiler
#  define NCNN_XADD(addr, delta) (int)_InterlockedExchangeAdd(const_cast<void*>(reinterpret_cast<volatile void*>(addr)), delta)
#elif defined __GNUC__
#  if defined __clang__ && __clang_major__ >= 3 && !defined __ANDROID__ && !defined __EMSCRIPTEN__ && !defined(__CUDACC__)
#    ifdef __ATOMIC_ACQ_REL
#      define NCNN_XADD(addr, delta) __c11_atomic_fetch_add((_Atomic(int)*)(addr), delta, __ATOMIC_ACQ_REL)
#    else
#      define NCNN_XADD(addr, delta) __atomic_fetch_add((_Atomic(int)*)(addr), delta, 4)
#    endif
#  else
#    if defined __ATOMIC_ACQ_REL && !defined __clang__
// version for gcc >= 4.7
#      define NCNN_XADD(addr, delta) (int)__atomic_fetch_add((unsigned*)(addr), (unsigned)(delta), __ATOMIC_ACQ_REL)
#    else
#      define NCNN_XADD(addr, delta) (int)__sync_fetch_and_add((unsigned*)(addr), (unsigned)(delta))
#    endif
#  endif
#elif defined _MSC_VER && !defined RC_INVOKED
#  include <intrin.h>
#  define NCNN_XADD(addr, delta) (int)_InterlockedExchangeAdd((long volatile*)addr, delta)
#else
// thread-unsafe branch
static inline int NCNN_XADD(int* addr, int delta) { int tmp = *addr; *addr += delta; return tmp; }
#endif


// Aligns a buffer size to the specified number of bytes
// The function returns the minimum number that is greater or equal to sz and is divisible by n
// sz Buffer size to align
// n Alignment size that must be a power of two
static inline unsigned int alignSize(unsigned int sz, int n)
{
    return (sz + n-1) & -n;
}

static inline void* fastMalloc(unsigned int size)
{
    unsigned char* udata = (unsigned char*)malloc(size + sizeof(void*) + MALLOC_ALIGN);
    if (!udata)
        return 0;
    unsigned char** adata = alignPtr((unsigned char**)udata + 1, MALLOC_ALIGN);
    adata[-1] = udata;
    return adata;
}


static inline void fastFree(void* ptr)
{
    if (ptr)
    {
        unsigned char* udata = ((unsigned char**)ptr)[-1];
        free(udata);
    }
}

class Allocator
{
public:
    virtual ~Allocator() = 0;
    virtual void* fastMalloc(size_t size) = 0;
    virtual void fastFree(void* ptr) = 0;
};

// the three dimension matrix
class Mat
{
public:
    // empty
    Mat();
    // vec
    Mat(int w, size_t elemsize = 4u, Allocator* allocator = 0);
    // image
    Mat(int w, int h, size_t elemsize = 4u, Allocator* allocator = 0);
    // dim
    Mat(int w, int h, int c, size_t elemsize = 4u, Allocator* allocator = 0);
    // copy
    Mat(const Mat& m);
    // external vec
    Mat(int w, void* data, size_t elemsize = 4u, Allocator* allocator = 0);
    // external image
    Mat(int w, int h, void* data, size_t elemsize = 4u, Allocator* allocator = 0);
    // external dim
    Mat(int w, int h, int c, void* data, size_t elemsize = 4u, Allocator* allocator = 0);
    // release
    ~Mat();
    // assign
    Mat& operator=(const Mat& m);

    // deep copy
    Mat clone(Allocator* allocator = 0) const;

    // allocate vec
    void create(int w, size_t elemsize = 4u, Allocator* allocator = 0);
    // allocate image
    void create(int w, int h, size_t elemsize = 4u, Allocator* allocator = 0);
    // allocate dim
    void create(int w, int h, int c, size_t elemsize = 4u, Allocator* allocator = 0);
    // refcount++
    void addref();
    // refcount--
    void release();

    bool empty() const;
    size_t total() const;

    // data reference
    Mat channel(int c);
    const Mat channel(int c) const;
    float* row(int y);
    const float* row(int y) const;
    template<typename T> T* row(int y);
    template<typename T> const T* row(int y) const;

    // range reference
    Mat channel_range(int c, int channels);
    const Mat channel_range(int c, int channels) const;
    Mat row_range(int y, int rows);
    const Mat row_range(int y, int rows) const;
    Mat range(int x, int n);
    const Mat range(int x, int n) const;

    // access raw data
    template<typename T> operator T*();
    template<typename T> operator const T*() const;

    // convenient access float vec element
    float& operator[](int i);
    const float& operator[](int i) const;

    // pointer to the data
    void* data;

    // pointer to the reference counter
    // when points to user-allocated data, the pointer is NULL
    int* refcount;

    // element size in bytes
    // 4 = float32/int32
    // 2 = float16
    // 1 = int8/uint8
    // 0 = empty
    size_t elemsize;

    // the allocator
    Allocator* allocator;

    // the dimensionality
    int dims;

    int w;
    int h;
    int c;

    size_t cstep;
};


inline Mat::Mat()
    : data(0), refcount(0), elemsize(0), allocator(0), dims(0), w(0), h(0), c(0), cstep(0)
{
}

inline Mat::Mat(int _w, size_t _elemsize, Allocator* allocator)
    : data(0), refcount(0), dims(0)
{
    create(_w, _elemsize, allocator);
}

inline Mat::Mat(int _w, int _h, size_t _elemsize, Allocator* allocator)
    : data(0), refcount(0), dims(0)
{
    create(_w, _h, _elemsize, allocator);
}

inline Mat::Mat(int _w, int _h, int _c, size_t _elemsize, Allocator* allocator)
    : data(0), refcount(0), dims(0)
{
    create(_w, _h, _c, _elemsize, allocator);
}

inline Mat::Mat(const Mat& m)
    : data(m.data), refcount(m.refcount), elemsize(m.elemsize), allocator(m.allocator), dims(m.dims)
{
    if (refcount)
        NCNN_XADD(refcount, 1);

    w = m.w;
    h = m.h;
    c = m.c;

    cstep = m.cstep;
}

inline Mat::Mat(int _w, void* _data, size_t _elemsize, Allocator* _allocator)
    : data(_data), refcount(0), elemsize(_elemsize), allocator(_allocator), dims(1)
{
    w = _w;
    h = 1;
    c = 1;

    cstep = w;
}

inline Mat::Mat(int _w, int _h, void* _data, size_t _elemsize, Allocator* _allocator)
    : data(_data), refcount(0), elemsize(_elemsize), allocator(_allocator), dims(2)
{
    w = _w;
    h = _h;
    c = 1;

    cstep = w * h;
}

inline Mat::Mat(int _w, int _h, int _c, void* _data, size_t _elemsize, Allocator* _allocator)
    : data(_data), refcount(0), elemsize(_elemsize), allocator(_allocator), dims(3)
{
    w = _w;
    h = _h;
    c = _c;

    cstep = alignSize(w * h * elemsize, 16) / elemsize;
}

inline Mat::~Mat()
{
    release();
}

inline Mat& Mat::operator=(const Mat& m)
{
    if (this == &m)
        return *this;

    if (m.refcount)
        NCNN_XADD(m.refcount, 1);

    release();

    data = m.data;
    refcount = m.refcount;
    elemsize = m.elemsize;
    allocator = m.allocator;

    dims = m.dims;
    w = m.w;
    h = m.h;
    c = m.c;

    cstep = m.cstep;

    return *this;
}

inline Mat Mat::clone(Allocator* allocator) const
{
    if (empty())
        return Mat();

    Mat m;
    if (dims == 1)
        m.create(w, elemsize, allocator);
    else if (dims == 2)
        m.create(w, h, elemsize, allocator);
    else if (dims == 3)
        m.create(w, h, c, elemsize, allocator);

    if (total() > 0)
    {
        memcpy(m.data, data, total() * elemsize);
    }

    return m;
}

inline void Mat::create(int _w, size_t _elemsize, Allocator* _allocator)
{
    if (dims == 1 && w == _w && elemsize == _elemsize && allocator == _allocator)
        return;

    release();

    elemsize = _elemsize;
    allocator = _allocator;

    dims = 1;
    w = _w;
    h = 1;
    c = 1;

    cstep = w;

    if (total() > 0)
    {
        size_t totalsize = alignSize(total() * elemsize, 4);
        if (allocator)
            data = allocator->fastMalloc(totalsize + (int)sizeof(*refcount));
        else
            data = fastMalloc(totalsize + (int)sizeof(*refcount));
        refcount = (int*)(((unsigned char*)data) + totalsize);
        *refcount = 1;
    }
}

inline void Mat::create(int _w, int _h, size_t _elemsize, Allocator* _allocator)
{
    if (dims == 2 && w == _w && h == _h && elemsize == _elemsize && allocator == _allocator)
        return;

    release();

    elemsize = _elemsize;
    allocator = _allocator;

    dims = 2;
    w = _w;
    h = _h;
    c = 1;

    cstep = w * h;

    if (total() > 0)
    {
        size_t totalsize = alignSize(total() * elemsize, 4);
        if (allocator)
            data = allocator->fastMalloc(totalsize + (int)sizeof(*refcount));
        else
            data = fastMalloc(totalsize + (int)sizeof(*refcount));
        refcount = (int*)(((unsigned char*)data) + totalsize);
        *refcount = 1;
    }
}

inline void Mat::create(int _w, int _h, int _c, size_t _elemsize, Allocator* _allocator)
{
    if (dims == 3 && w == _w && h == _h && c == _c && elemsize == _elemsize && allocator == _allocator)
        return;

    release();

    elemsize = _elemsize;
    allocator = _allocator;

    dims = 3;
    w = _w;
    h = _h;
    c = _c;

    cstep = alignSize(w * h * elemsize, 16) / elemsize;

    if (total() > 0)
    {
        size_t totalsize = alignSize(total() * elemsize, 4);
        if (allocator)
            data = allocator->fastMalloc(totalsize + (int)sizeof(*refcount));
        else
            data = fastMalloc(totalsize + (int)sizeof(*refcount));
        refcount = (int*)(((unsigned char*)data) + totalsize);
        *refcount = 1;
    }
}

inline void Mat::addref()
{
    if (refcount)
        NCNN_XADD(refcount, 1);
}

inline void Mat::release()
{
    if (refcount && NCNN_XADD(refcount, -1) == 1)
    {
        if (allocator)
            allocator->fastFree(data);
        else
            fastFree(data);
    }

    data = 0;

    elemsize = 0;

    dims = 0;
    w = 0;
    h = 0;
    c = 0;

    cstep = 0;

    refcount = 0;
}

inline bool Mat::empty() const
{
    return data == 0 || total() == 0;
}

inline size_t Mat::total() const
{
    return cstep * c;
}

inline Mat Mat::channel(int c)
{
    return Mat(w, h, (unsigned char*)data + cstep * c * elemsize, elemsize, allocator);
}

inline const Mat Mat::channel(int c) const
{
    return Mat(w, h, (unsigned char*)data + cstep * c * elemsize, elemsize, allocator);
}

inline float* Mat::row(int y)
{
    return (float*)data + w * y;
}

inline const float* Mat::row(int y) const
{
    return (const float*)data + w * y;
}

template <typename T>
inline T* Mat::row(int y)
{
    return (T*)data + w * y;
}

template <typename T>
inline const T* Mat::row(int y) const
{
    return (const T*)data + w * y;
}

inline Mat Mat::channel_range(int _c, int channels)
{
    return Mat(w, h, channels, (unsigned char*)data + cstep * _c * elemsize, elemsize, allocator);
}

inline const Mat Mat::channel_range(int _c, int channels) const
{
    return Mat(w, h, channels, (unsigned char*)data + cstep * _c * elemsize, elemsize, allocator);
}

inline Mat Mat::row_range(int y, int rows)
{
    return Mat(w, rows, (unsigned char*)data + w * y * elemsize, elemsize, allocator);
}

inline const Mat Mat::row_range(int y, int rows) const
{
    return Mat(w, rows, (unsigned char*)data + w * y * elemsize, elemsize, allocator);
}

inline Mat Mat::range(int x, int n)
{
    return Mat(n, (unsigned char*)data + x * elemsize, elemsize, allocator);
}

inline const Mat Mat::range(int x, int n) const
{
    return Mat(n, (unsigned char*)data + x * elemsize, elemsize, allocator);
}

template <typename T>
inline Mat::operator T*()
{
    return (T*)data;
}

template <typename T>
inline Mat::operator const T*() const
{
    return (const T*)data;
}

inline float& Mat::operator[](int i)
{
    return ((float*)data)[i];
}

inline const float& Mat::operator[](int i) const
{
    return ((const float*)data)[i];
}

} // namespace ncnn



#endif /* SRC_EEPTPU_NMAT_H_ */
