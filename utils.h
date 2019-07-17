#pragma once
#include <stdio.h>
#include <assert.h> 

#if defined(NDEBUG) && defined(__GNUC__)
#define U_ASSERT_ONLY __attribute__((unused))
#else
#define U_ASSERT_ONLY
#endif

#if defined(NDEBUG)
#define BAIL fprintf(stderr, "Failure at %u %s\n", __LINE__, __FILE__); exit(-1);
#else
#define BAIL fprintf(stderr, "Failure at %u %s\n", __LINE__, __FILE__); assert(false);
#endif

#define ASSERT_BAIL(result) \
  if (!result) { BAIL }


#define BAIL_IF_NOT(result,expected) \
  if (expected != (result)) { BAIL }

#define BAIL_IF(result,expected) \
  if (expected == (result)) { BAIL }

template <typename T> __forceinline T AlignUpWithMask(T value, size_t mask)
{
	return (T)(((size_t)value + mask) & ~mask);
}

template <typename T> __forceinline T AlignUp(T value, size_t alignment)
{
	return AlignUpWithMask(value, alignment - 1);
}