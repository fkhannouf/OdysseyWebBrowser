// Copyright (c) 2005, 2007, Google Inc. All rights reserved.

/*
 * Copyright (C) 2005-2009, 2011, 2015 Apple Inc. All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "FastMalloc.h"

#include "CheckedArithmetic.h"
#include "CurrentTime.h"
#include <limits>
#include <string.h>
#include <wtf/DataLog.h>

#if OS(WINDOWS)
#include <windows.h>
#elif OS(AROS)
#include <aros/debug.h>
#include <proto/exec.h>
#undef Allocate
#undef Deallocate
#else
#include <pthread.h>
#endif

#if OS(DARWIN)
#include <mach/mach_init.h>
#include <malloc/malloc.h>
#endif

namespace WTF {

void* fastZeroedMalloc(size_t n) 
{
    void* result = fastMalloc(n);
    if(result)
        memset(result, 0, n);
    return result;
}

char* fastStrDup(const char* src)
{
    size_t len = strlen(src) + 1;
    char* dup = static_cast<char*>(fastMalloc(len));
    memcpy(dup, src, len);
    return dup;
}

TryMallocReturnValue tryFastZeroedMalloc(size_t n) 
{
    void* result;
    if (!tryFastMalloc(n).getValue(result))
        return 0;
    memset(result, 0, n);
    return result;
}

} // namespace WTF

#if defined(USE_SYSTEM_MALLOC) && USE_SYSTEM_MALLOC

#if OS(WINDOWS)
#include <malloc.h>
#elif OS(MORPHOS)
#include <clib/debug_protos.h>
extern int morphos_crash(size_t);
#endif

namespace WTF {

static size_t g_limit = 0;
static size_t g_memory_allocated = 0;
MemoryNotification* g_memoryNotification = 0;

void setMemoryNotificationCallback(MemoryNotification* memoryNotification)
{
    if (g_memoryNotification == NULL)
	g_memoryNotification = memoryNotification;
}

void setMemoryLimit(int limit)
{
    if (limit < 0)
	g_limit = 0;
    else
	g_limit = limit;
}

size_t fastMallocGoodSize(size_t bytes)
{
#if OS(DARWIN)
    return malloc_good_size(bytes);
#else
    return bytes;
#endif
}

#if OS(WINDOWS)

void* fastAlignedMalloc(size_t alignment, size_t size) 
{
    return _aligned_malloc(size, alignment);
}

void fastAlignedFree(void* p) 
{
    _aligned_free(p);
}

#else

void* fastAlignedMalloc(size_t alignment, size_t size) 
{
    void* p = nullptr;
    posix_memalign(&p, alignment, size);
    return p;
}

void fastAlignedFree(void* p) 
{
    free(p);
}

#endif // OS(WINDOWS)

//#if OS(MORPHOS)
//    g_memory_allocated += n + Internal::ValidationBufferSize;
//    //kprintf("+ g_memory_allocated %d (+%d)\n", g_memory_allocated, n);
//
//    if (g_limit != 0 && (g_memory_allocated > g_limit)) {
//	if (g_memoryNotification)
//	    g_memoryNotification->call();
//   }
//#endif

TryMallocReturnValue tryFastMalloc(size_t n) 
{
    return malloc(n ? n : 2);
}

void* fastMalloc(size_t n)
{
retry:
    void* result = malloc(n ? n : 2);
    if (!result)
    {
        kprintf("fastMalloc: Failed to allocate %lu bytes. Happy crash sponsored by WebKit will follow.\n", n ? n : 2); 
        if(morphos_crash(n ? n : 2)) 
            goto retry;
    }

    return result;
}

TryMallocReturnValue tryFastCalloc(size_t n_elements, size_t element_size)
{
    return calloc(n_elements ? n_elements : 1, element_size ? element_size : 2);
}

void* fastCalloc(size_t n_elements, size_t element_size)
{
retry:
    void* result = calloc(n_elements ? n_elements : 1, element_size ? element_size : 2);
    if (!result) 
    {
        kprintf("fastCalloc: Failed to allocate %lu x %lu bytes. Happy crash sponsored by WebKit will follow.\n", n_elements ? n_elements : 1, element_size ? element_size : 2);
        if(morphos_crash((n_elements ? n_elements : 1)*(element_size ? element_size : 2))) 
            goto retry;
    }

    return result;
}

//#if OS(MORPHOS)
//    g_memory_allocated -= (fastMallocSize(p) + Internal::ValidationBufferSize);
//    //kprintf("- g_memory_allocated %d (-%d)\n", g_memory_allocated, fastMallocSize(p));
//#endif    

void fastFree(void* p)
{
    free(p);
}

void* fastRealloc(void* p, size_t n)
{
retry:
    void* result = realloc(p, n ? n : 2);
    if (!result)
    {
        kprintf("fastRealloc: Failed to allocate %lu bytes. Happy crash sponsored by WebKit will follow.\n", n ? n : 2);
        if(morphos_crash(n ? n : 2))
            goto retry;
    }
    return result;
}

void releaseFastMallocFreeMemory() { }
void releaseFastMallocFreeMemoryForThisThread() { }
    
FastMallocStatistics fastMallocStatistics()
{
    FastMallocStatistics statistics = { 0, 0, 0 };
    return statistics;
}

size_t fastMallocSize(const void* p)
{
#if OS(DARWIN)
    return malloc_size(p);
#elif OS(WINDOWS)
    return _msize(const_cast<void*>(p));
#else
    UNUSED_PARAM(p);
    return 1;
#endif
}

} // namespace WTF

#else // defined(USE_SYSTEM_MALLOC) && USE_SYSTEM_MALLOC

#include <bmalloc/bmalloc.h>

namespace WTF {

void* fastMalloc(size_t size)
{
    return bmalloc::api::malloc(size);
}

void* fastCalloc(size_t numElements, size_t elementSize)
{
    Checked<size_t> checkedSize = elementSize;
    checkedSize *= numElements;
    void* result = fastZeroedMalloc(checkedSize.unsafeGet());
    if (!result)
        CRASH();
    return result;
}

void* fastRealloc(void* object, size_t size)
{
    return bmalloc::api::realloc(object, size);
}

void fastFree(void* object)
{
    bmalloc::api::free(object);
}

size_t fastMallocSize(const void*)
{
    // FIXME: This is incorrect; best fix is probably to remove this function.
    // Caller currently are all using this for assertion, not to actually check
    // the size of the allocation, so maybe we can come up with something for that.
    return 1;
}

size_t fastMallocGoodSize(size_t size)
{
    // FIXME: This is non-helpful; fastMallocGoodSize will be removed soon.
    return size;
}

void* fastAlignedMalloc(size_t alignment, size_t size) 
{
    return bmalloc::api::memalign(alignment, size);
}

void fastAlignedFree(void* p) 
{
    bmalloc::api::free(p);
}

TryMallocReturnValue tryFastMalloc(size_t size)
{
    return bmalloc::api::tryMalloc(size);
}
    
TryMallocReturnValue tryFastCalloc(size_t numElements, size_t elementSize)
{
    Checked<size_t, RecordOverflow> checkedSize = elementSize;
    checkedSize *= numElements;
    if (checkedSize.hasOverflowed())
        return nullptr;
    return tryFastZeroedMalloc(checkedSize.unsafeGet());
}
    
void releaseFastMallocFreeMemoryForThisThread()
{
    bmalloc::api::scavengeThisThread();
}

void releaseFastMallocFreeMemory()
{
    bmalloc::api::scavenge();
}

FastMallocStatistics fastMallocStatistics()
{
    // FIXME: This is incorrect; needs an implementation or to be removed.
    FastMallocStatistics statistics = { 0, 0, 0 };
    return statistics;
}

} // namespace WTF

#endif // defined(USE_SYSTEM_MALLOC) && USE_SYSTEM_MALLOC