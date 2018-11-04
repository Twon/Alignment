# What is alignment and why should I care?

Every type in C++ has the property called alignment requirement, which specifies the number of bytes between successive addresses at which objects of this type can be allocated. For instance, if you create a double on either the stack or the heap, it is guaranteed to be aligned to at least the size of a double (a double is 8-bytes so will always be aligned to an 8-byte boundary). The reason for this relates to restrictions placed on alignment by the underlying hardware architecture when handling and accessing data. Certain architectures such as ARM or MIPS will generate a hardware level exception when accessing data which is not aligned with alignment boundaries of at least a type's size. Whereas other architecture such as PowerPC may generate multiple loads for unaligned data which is less efficient than the single load required to load data which is optimally aligned. To ensure consistency across the language C++ make certain guarantees about alignment. This article will examine what these guarantees are, how they have changed with the evolution of the language and what the implications of these are.

## C++ 03
Earlier versions of C++ have made guarantees about alignment requirements for fundamental types. As a result users could relax safely in the knowledge that when building classes composed of fundamental types that all alignment requirements would be respected. However, this had a side effect; for compilers to respect these alignment requirements it is often necessary for compilers to insert invisible padding elements inside of classes [1]. For instance, consider the simple structure:

```cpp
struct X
{
    int8_t a;
    int64_t b;
    int8_t c;
    int16_t d;
    int64_t e;
    float f;
};
```

In order for compilers to respect the alignment requirements of this structure, the compiler inserts invisible elements to pad out the structures data layout which increases its size requirements. The author of this structure may well have assumed the size of this structure will be 24 bytes long, but the layout the compiler has to generate is actually 40 bytes in length and looks more like the following:

```cpp
struct Y
{
    int8_t a;
    int8_t pad_a[7]; // Compiler generated padding
    int64_t b;
    int8_t c;
    int8_t pad_c[1]; // Compiler generated padding
    int16_t d;
    int16_t pad_d[2]; // Compiler generated padding
    int64_t e;
    float f;
    float pad_f[1]; // Compiler generated padding
};
```
This example makes visible what is going on behind the scenes of a compiler to ensure alignment requirements are met. Because a 64-bit integer must be aligned to 8 bytes the compiler has to insert 7 bytes of padding after data member a and b. Similar padding is needed elsewhere to meet the specific alignment requirements of each data member. One point of interest in the insertion of pad_f by the compiler, while this might initially not appear to be needed for single instances of the class it becomes apparent that this must be inserted in order for the compiler to ensure that for an array of type Y, each instance of the Y struct is correctly aligned to meet the requirements of its internal data members. In particular without pad_f then data member b in the second element of an array would be 4 byte aligned, not 8-byte aligned. Of course, it's possible to ensure this padding is not necessary and that the resulting data structure is 24-bytes in length, however, to do so we must take care to manually align each data member as such:

```cpp
struct Z
{
    int64_t b;
    int64_t e;
    float f;
    int16_t d;
    int8_t a;
    int8_t c;
};
```

This, of course, creates other problems. In C++ declaration order of member variables in a structure or class also defines the initialisation order of member variables in a constructor. So now if we try to create a constructor for structure Z which initialises members in alphabetical order we might then be surprised to find out this is not the order they are initialised in.

```cpp
struct Z
{
    int64_t b;
    int64_t e;
    float f;
    int16_t d;
    int8_t a;
    int8_t c;

    // The constructor initialiser list actually initialises members in order b, e, f, e, a, c
    // and not a, b, c, d, e, f as the unsuspecting maintainer of this code might think.
    Z() : a(0), b(0), c(0), d(0), e(0), f(0.0) {}
};
```

Thankfully in this example, that's not a huge problem, but as soon the initialisation order of data members matter this restriction creates problems. For this reason, worrying about data layout is something best left to compilers or library vendors unless explicitly required. Let's consider a case when this is explicitly required.

## Alignment requirements for vectorisation
Modern processors are built with SIMD capabilities. That is the ability to execute instructions on multiple data elements using specialised instructions, which allows for a form of parallelism known as instruction level parallelism.

[[SIMD PICTURE]]

One side effect of using these instructions is that in some case they require using aligned data (resulting in alignment exceptions when not aligned), and when they do work with unaligned data they may generate multiple loads or stores across the CPU bus when they straddle a CPU cache boundary (typically 32 or 64 bytes) resulting in a performance slow down. For direct interaction with these instructions, one must use an extension to the fundamental types provided by the C++ standard, in part because of these instructions a dependent on the underlying architecture of the process. So for instance compilers supporting the x86 or x64 architectures provide support for the use of the __m128 and __m256 types, which at 128-bits and 256-bits respectively are larger are larger than any of the existing fundamental types in the language. However, these still have alignment requirements matching those of the fundamental types of the language, namely that alignment must be greater than or equal to these type's size. That is a __m128 must be aligned to a 128-bit or 16-byte boundary or greater, and a __m256 must be aligned to a 256-bit or 32-byte boundary or greater. This raises the question how did compilers ensure these alignment requirements are guaranteed? Peaking at the definition of this type in the Visual Studio compiler provides some insights, namely compilers support this alignment requirement with a compiler specific extension:

```cpp
/*
* Intel(R) AVX compiler intrinsic functions.
*/
typedef union __declspec(intrin_type) __declspec(align(32)) __m256 {
    float m256_f32[8];
} __m256;
```
Microsoft Visual Studio and the Intel C++ Compiler for Windows support the syntax __declspec(align( # )) where as GCC, Clang and the Intel C++ Compiler for Linux support the syntax __attribute__ ((aligned( # ))).  These compiler extensions tell the compiler to ensure that data is aligned to the boundary specified in the alignment statement, with a few restrictions:
- The number must be a positive integer that is a power of two.
- The number must be less than or equal to 128.
- The number must be known at compile time.
The last requirement, in particular, can create some difficulties when trying to use alignment with Visual Studio versions prior to 2017 update 5, which did not correctly implement the 2 phase template lookup required by the C++ standard when parsing template class. If we templatise a class on an alignment value, we create a situation where the compiler requires knowledge of a dependent type, the template alignment parameter, but does not yet have knowledge of this type. This unsurprisingly results in a compilation error:
```cpp
template <class T>
class __declspec(align(Alignment)) AlignedClass {
private:
    T myData;
};  // 'AlignedClass' : invalid template argument for 'Alignment', expected compile-time constant expression
```
We can, of course, work around this issue in the compiler with some template magic:
```cpp
template<std::size_t A> struct aligned;
template<> struct __declspec(align((1)) aligned<1> { };
template<> struct __declspec(align((2)) aligned<2> { };
template<> struct __declspec(align((4)) aligned<4> { };
template<> struct __declspec(align((8)) aligned<8> { };
template<> struct __declspec(align((16)) aligned<16> { };
template<> struct __declspec(align((32)) aligned<32> { };
template<> struct __declspec(align((64)) aligned<64> { };
template<> struct __declspec(align((128)) aligned<128> { };

template<class T>
class AlignedClass : aligned<32> {
private:
    T myData; 
};
```
But wouldn't this be nice if this just worked?

## Alignment of dynamically allocated memory
This brings us to the next problem, heap-allocated memory. The extensions we have just looked at apply to structures and classes the compiler has control of, namely those declared global, statically or locally on the stack. If however, we need to allocate storage for an aligned type at run-time then the compiler has no control on this:

```cpp
struct __declspec(align((16))) float4 {
    float f[4];
};

float4 *pData = new float4[1000];
```
In the above example not only is an implementation of C++ not required to allocate properly aligned memory for the array, for practical purposes, it is very nearly required to do the allocation incorrectly [2]. Looking at how compilers implement vector deleting destructors we can see that they are required to insert the size of a dynamically allocated array into the beginning of the allocated block of memory so that the array delete knows how many elements in an array must have their destructors called. The upshot of this is array allocations are offset by the size of size_t.

So how can we safely create aligned dynamically allocate memory? Once again its compiler specific implementations to the rescue, except this time we still have some manual work to do our self. Microsoft Visual Studio and the Intel C++ Compiler on Windows provide _aligned_malloc and _aligned_free for the creation of aligned memory, and GCC, Clang and the Intel C++ Compiler on Linux support the use of the memalign and free functions. However both compiler mechanisms for aligned allocation only allocate raw storage, so any use with non-POD types requires manually calling their constructors and destructors to ensure proper initialisation and clean-up:

```cpp
struct __declspec(align((16))) float4 {
public:
   float4() : f() {}
   float f[4];
};

const std::size_t num_elements = 1000;
void *pArrayBuffer = _aligned_malloc(sizeof(float4) * num_elements, 16);
float4 *p = new (pArrayBuffer) float4[num_elements];

// Use the array
// ...

// Manually desctuct elements and free memory
for (std::size_t i = 0; i < num_elements; ++i)
{
    p[i].~float4();
}
_aligned_free(pArrayBuffer);
```

In this example we have modified float4 to have a constructor using the aggregate initialisation syntax of C ++03 to zero every element of the array data member f. However this means that it is not enough to just create a buffer of memory for the array, we must force initialisation of each element in the array by using the placement array new operator which allows calling new on a pre-allocated block of memory. In turn, once we have finished using the array, to ensure proper clean, we must manually loop through the array and call each instance's destructor directly. Once the array in uninitialised we can then free the underlying aligned memory buffer. As you can see this is error-prone and complex, requiring special functions in order to be able to allocate and initialise the memory for the array. There is also another problem, should any exceptions be thrown either from the constructor of float4 or in the usage of the array we will leak resource because we have not made use of the resource acquisition is initialisation idiom.

Let's see what we can do to clean this up a bit. We'll make use of C++03 overloading of the array new and array delete operators at the class level for float4 (please note for simplicities sake I've avoided the issue of throwing bad_alloc if a null pointer is returned from the allocation routine). This automates away some of the complications. We now don't have to use the placement new operator or manually call the destructors for all elements in the array. Moreover, the compiler is smart enough to over-allocate by enough storage to insert
the secret size variable which is required for generation of the vector deleting destructor while still meeting the alignment requirements. And finally, while we were unable to solve the RAII problem which could bite under the presence of exceptions with the standard template libraries auto_ptr, which does not support arrays, we were able to solve this issue with the Boost libraries scoped_array smart pointer class.

```cpp
class __declspec(align((16))) float4 {
public:
    float4() : f() {}
    float f[4];
    void* operator new[](std::size_t size)
    {
        void* mem = _aligned_malloc(size, 16);
        if (!mem)
            throw std::bad_alloc();
        return mem;
    }

    void operator delete[](void* p)
    {
        _aligned_free(p);
    }
};

const std::size_t num_elements = 1000;
boost::scoped_array<float4> p(new float4[num_elements]);

/ / Use the array
// ...

p.reset(); // Delete the array
```

So this is a big improvement over the first iteration but still, it is not exactly easy to use. We had to overload member functions on the class which is acceptable if we are the authors of this class, but if this was a class in an external library we may not have this option available to us. We should also overload the new and delete operators for this class, and properly handle throwing std::bad_alloc exceptions if the allocations throw. Potentially we may also choose to implement the no-throw variants of these functions. But even if we do all of this there is still another problem. Do you see it?

Its the issue of ensuring correct alignment for types requiring alignment during dynamic allocation of a class that encapsulates them. For instance, even if we have overloaded all of the new and delete methods to ensure direct allocations of float4 are aligned, we can't protect against the following:

```cpp
class __declspec(align((16))) float4 {
public:
    float4() : f() {}
    float f[4];

    void* operator new[](std::size_t size)
    {
        void* mem = _aligned_malloc(size, 16);
        if (!mem)
            throw std::bad_alloc();
        return mem;
    }

    void operator delete[](void* p)
    {
        _aligned_free(p);
    }
};

struct line {
    float4 start;
    float4 end;
};

line* pLine = new line;

// Are start and end correctly aligned?
```

Now if we create an instance of a line structure via dynamic allocation all bets are off as to whether the start and end members are correctly aligned. In the best case, you will now have a performance slow down when using these elements which is hard to detect and diagnose the cause. In the worst case, you might have a program that generates an alignment exception when start and end are accessed.

While the examples are somewhat contrived they are representative of real-life issues this author has encountered in writing high-performance software libraries. This had been an area of the language which was sorely neglected. However, this is no longer the case, this area has now improved significantly with C ++11/14/17. The rest of this article will look at developments in the language which significantly improve this area.

## C++ 11
With the major overhaul of the language which was C++ 11, there have been many improvements to the language in regards to alignment. The first is the standardisation of alignment specification from compiler-specific extension to a keyword in the language. The alignas keyword instructs a compiler of a types alignment requirements in a language conformant manner.   Additionally, the language now has the alignof keyword to allow querying of a type's expected alignment:

```cpp
#include <iostream>

class alignas(16) float4 {
    float f[4];
};

int main() {
    std::cout << "Alignment of \n"
        "- char : " << alignof(char) << "\n" // Alignment of 1
        "- int32_t : " << alignof(int32_t) << "\n"  // Alignment of 4
        "- double: " << alignof(double) << "\n" // Alignment of 8
        "- class float4 : " << alignof(float4) << "\n"; // Alignment of 16
}
```

The standard now provides a type, std::max_align_t, whose alignment guaranteed is required to be met by all memory allocation routines (malloc, new, etc). This goes a long way to unifying alignment requirements for compiler controlled allocation within the language. However, it fails to address the issue of dynamic memory allocation. For that we need C++17.

## C++ 17
In the latest C++ standard aligned allocation of memory is now standardised in the language, the function std::aligned_alloc creates aligned memory which can be used in conjunction with std::free. Importantly this removes the need for us to use compiler-specific aligned memory allocation functions and solves the issue of needing to use a specific aligned deallocation method that plagues the Microsoft _aligned_malloc and _aligned_free pair.
But the standard does not stop there providing alternative overloads for new and delete which make use of a type's alignment requirements when allocating. Versions are provided for standard, no throw, placement new and class variants of these operators:

```cpp
void* operator new  (std::size_t count, std::align_val_t al);
void* operator new[](std::size_t count, std::align_val_t al);
void* operator new  (std::size_t count, std::align_val_t al, const std::nothrow_t&);
void* operator new[](std::size_t count, std::align_val_t al, const std::nothrow_t&);
void operator delete  (void* ptr, std::align_val_t al);
void operator delete[](void* ptr, std::align_val_t al);
void operator delete  (void* ptr, std::align_val_t al, const std::nothrow_t& tag);
void operator delete[](void* ptr, std::align_val_t al, const std::nothrow_t& tag);
```

So armed with this knowledge lets revisit our earlier example of using dynamically aligned memory for a type using over alignment (that is specifying alignment requirements greater than those of the fundamental types).

```cpp
#include <memory>

int main() {
    class alignas(16) float4 {
        float f[4];
    };

    std::unique_ptr<float4[]> p(std::make_unique<float4[]>(1000));
    
    // Do stuff with our aligned array
    ... 
}
```

Now we are safely able to specify a types alignment requirements, safe in the knowledge that dynamic memory allocation and deallocation will just "do the right thing". Moreover, this can safely be used in conjunction with the standard library types from C++11 (using the std::make_unique function from C++14) to ensure we use the RAII idiom to guard against any resource leaks and ensure our code is exception safe. But we still have one more use case we want to the language to solve for us, that's alignment of embedded classes in a class without alignment requirements specified.

Thankfully it turns out our friend, the alignas keyword, in conjunction with the existing guarantees in the language around alignment requirements solves this problem for us. The language guarantees that for
any type it must respect the alignment requirements of any member types it owns. This means if we revisit our earlier example, that specifying an alignment requirement on float4 is enough to ensure the alignment requirement is propagated to the line structure. Now when we use dynamic memory allocation on that class it calls the overload of new which take the alignment requirement and ensure that everything is aligned correctly as we would like.

```cpp
#include <iostream>
#include <memory>

int main() {
    class alignas(16) float4 {
         float f[4];
    };

    struct line {
        float4 start;
        float4 end;
    };

    std::cout << "Alignment of - line alignment of 16: " << alignof(line) << "\n";

    // line has std::unique_ptr<line> pLine(std::make_unique<line>());
    // Do stuff with pLine
    ...
 }
```

## Alignment A Solved Problem?
So it now looks like we can do everything we want with alignment through core features of the C++17 language. But there is still one important area missing. That of ensuring the compiler has all of the potential optimisation opportunities available to it to make full use of auto vectorisation. And this is one of the big benefits of the culminating changes to alignment that C++11 and 17 bring, they allow the compiler to safely make assumptions about alignment of a type for all instances of that type regardless of whether it is heap or stack allocated, and then safely make optimisations based on this knowledge. But what about cases where alignment specifications are not part of the type, for instance, consider the following case:

```cpp
void scale(float* x, int size, float factor)
{
    for (int i = 0; i < size; ++i)
        x[i] *= factor;
}

float alignas(64) data[1024];
scale(data, 1024, 2.0)
```

In this case, alignment requirements are not part of the type information for the declaration of the data variable, yet this is perfectly valid in C++11 and beyond. Of course, if all of this happens in the context of a translation unit then the compiler may have enough information to know it can safely assume the for loop is acting on aligned data and make optimisation assumptions based on this. However, if the scale function is defined in a separate translation unit then it cannot know this and must assume the worse, and avoid optimisations based on alignment.

Yet again we hit a case where compilers try to come to the rescue with vendor-specific extension. GCC provides the __builtin_assume_aligned built-in function to tell the compiler when it can safely assume data is aligned.

```cpp
void scale(float* x, int size, float factor)
{
    float* ax = (float*)__builtin_assume_aligned(x, 64);
    for (int i = 0; i < size; ++i)
        ax[i] *= factor;
}
```

The Clang compiler provides an alternative mechanism to achieve the same mean by way of an attribute, __attribute__((assume_aligned(alignment))), which can be coerced into behaving like the built-in method in GCC.

```cpp
template <std::size_t alignment>
float* assume_aligned(float* p) __attribute__((assume_aligned(alignment)));

template <std::size_t>
float* assume_aligned(float* p) { return p; }

void scale(float* x, int size, float factor)
{
    float* ax = (float*)assume_aligned<64>(x);
    for (int i = 0; i < size; ++i)
        ax[i] *= factor;
}
```

Finally, the Intel C++ Compiler also throws its hat into the ring with yet another alternative syntax, this time called __assume_aligned(ptr, alignment):

```cpp
void scale(float* x, int size, float factor)
{
    __assume_aligned(x, 64);
    for (int i = 0; i < size; ++i)
        ax[i] *= factor;
}
```

So we still have some ambiguity within the language currently. However, proposals for the C++ 20 standard have suggested adding a new attribute to unify this feature in the language. If this is accepted then in future versions of the language you will be able to use the following syntax [3]:

```cpp
void add_64aligned([[assume_aligned(64)]] float* x, [assume_aligned(64)]] float* y, int
size)
{
    for (int i = 0; i < size; ++i)
        x[i] += y[i];
}
```

## Conclusions
The C++ language has taken significant steps in recent revisions to the language to simplify the alignment of data, adding core features to the language to hide the complexity. This means for users this area of the language just works, without having to manually intervene to get the desired behaviour as has historically been the case [5]. This, however, does not affect the care we must take around compiler generated padding caused by the layout of types we as programmers write, here we require an understanding of alignment requirements within the language and a careful eye.

## References
[[1] Christer Ericson. Memory Optimization. Games Developers Conference 2003](http://realtimecollisiondetection.net/pubs/GDC03_Ericson_Memory_Optimization.ppt)  
[[2] Clark Nelson. Dynamic memory allocation for over-aligned data. P0035R4](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0035r4.html)
[[3] Timur Doumler & Chandler Carruth. The assume aligned attribute. ISOCPP SG14 - Low Latency/Game Dev/Financial/Trading/ Simulation/Embedded Devices](https://wg21.link/p0886)  
[[4] Timur Doumler & Chandler Carruth. std::assume_aligned.  P1007R0](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p1007r0.pdf)  
[[5] Tony Albrecht. Pitfalls Of Object Oriented Programming. Sony Computer Entertainment Europe, Research & Development Division](https://web.archive.org/web/20150212013524/http://research.scee.net/files/presentations/gcapaustralia09/Pitfalls_of_Object_Oriented_Programming_GCAP_09.pdf)  