
#define ASSERTS_ON

#ifdef ASSERTS_ON
#include <assert.h>
#define ASSERT(X) assert(X)
#else
#define ASSERT(X)
#endif

