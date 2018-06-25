#define CATCH_CONFIG_MAIN
#include <catch.hpp>
#include <cstddef>
#include <type_traits>

template< typename T >
bool is_aligned(T* p)
{
    return !(reinterpret_cast<uintptr_t>(p) % std::alignment_of<T>::value);
}

TEST_CASE("Demonstrate padding automatiacally inserted by the compiler to guarentee alignement","[alignment.test_compiler_padding]")
{
    GIVEN( "A structure with un-ordered data member" )
    {
        struct X
        {
            int8_t a;
            int64_t b;
            int8_t c;
            int16_t d;
            int64_t e;
            float f;
        };

        WHEN( "looking at the layout of X" )
        {
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

            THEN( "expect the compiler to insert padding between elements to meet the alignment requirements of members" ) {
                REQUIRE(offsetof(X, a) == 0);
                REQUIRE(offsetof(X, b) == 8);
                REQUIRE(offsetof(X, c) == 16);
                REQUIRE(offsetof(X, d) == 18);
                REQUIRE(offsetof(X, e) == 24);
                REQUIRE(offsetof(X, f) == 32);

                // Looking at the layout of X we can see the compiler generated something akind to Y.
                REQUIRE(offsetof(X, a) == offsetof(Y, a));
                REQUIRE(offsetof(X, b) == offsetof(Y, b));
                REQUIRE(offsetof(X, c) == offsetof(Y, c));
                REQUIRE(offsetof(X, d) == offsetof(Y, d));
                REQUIRE(offsetof(X, e) == offsetof(Y, e));
                REQUIRE(offsetof(X, f) == offsetof(Y, f));
                REQUIRE(sizeof(X) == sizeof(Y));
            }
        }
        WHEN( "working with an array of X" )
        {
            X array[2];

            THEN( "expect the compiler to insert 4-bytes of padding between at the end of X to ensure correct alignment in arrays" )
            {
                REQUIRE(sizeof(X) == 40);
                REQUIRE(is_aligned(&array[1]));
                REQUIRE(std::alignment_of<X>::value == 8);
            }
        }
        WHEN( "reordering the elements of X from biggest to smallest" )
        {
            struct Z
            {
                int64_t b;
                int64_t e;
                float f;
                int16_t d;
                int8_t a;
                int8_t c;
            };

            THEN( "expect the compiler to insert no padding thus reducing the overall size of the structure" ) {
                REQUIRE(offsetof(Z, b) == 0);
                REQUIRE(offsetof(Z, e) == 8);
                REQUIRE(offsetof(Z, f) == 16);
                REQUIRE(offsetof(Z, d) == 20);
                REQUIRE(offsetof(Z, a) == 22);
                REQUIRE(offsetof(Z, c) == 23);
                REQUIRE(sizeof(Z) == 24);
            }
        }
    }
}

