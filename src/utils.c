// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "jdsimple.h"

static int8_t irq_disabled;

void target_enable_irq() {
    irq_disabled--;
    if (irq_disabled <= 0) {
        irq_disabled = 0;
        __enable_irq();
        // set_log_pin2(0);
    }
}

void target_disable_irq() {
    irq_disabled++;
    if (irq_disabled == 1) {
        // set_log_pin2(1);
        __disable_irq();
    }
}

/**
 * Performs an in buffer reverse of a given char array.
 *
 * @param s the string to reverse.
 *
 * @return DEVICE_OK, or DEVICE_INVALID_PARAMETER.
 */
int string_reverse(char *s) {
    // sanity check...
    if (s == NULL)
        return -1;

    char *j;
    int c;

    j = s + strlen(s) - 1;

    while (s < j) {
        c = *s;
        *s++ = *j;
        *j-- = c;
    }

    return 0;
}

/**
 * Converts a given integer into a string representation.
 *
 * @param n The number to convert.
 *
 * @param s A pointer to the buffer where the resulting string will be stored.
 *
 * @return DEVICE_OK, or DEVICE_INVALID_PARAMETER.
 */
int itoa(int n, char *s) {
    int i = 0;
    int positive = (n >= 0);

    if (s == NULL)
        return -1;

    // Record the sign of the number,
    // Ensure our working value is positive.
    unsigned k = positive ? n : -n;

    // Calculate each character, starting with the LSB.
    do {
        s[i++] = (k % 10) + '0';
    } while ((k /= 10) > 0);

    // Add a negative sign as needed
    if (!positive)
        s[i++] = '-';

    // Terminate the string.
    s[i] = '\0';

    // Flip the order.
    string_reverse(s);

    return 0;
}

RAM_FUNC
static void do_wait_us(int n) {
    __asm__ __volatile__(".syntax unified\n"
                         "1:              \n"
                         "   subs %0, #1   \n" // subtract 1 from %0 (n)
                         "   bne 1b       \n"  // if result is not 0 jump to 1
                         : "+r"(n)             // '%0' is n variable with RW constraints
                         :                     // no input
                         :                     // no clobber
    );
}

void wait_us(int n) {
#ifdef STM32G0
    // 64MHz, this is 3 cycles
    n = n * 64 / 3;
#elif defined(STM32F0)
    // 48MHz, and the loop is 4 cycles
    n = n * 48 / 4;
#else
#error "define clock rate"
#endif
    do_wait_us(n);
}

// https://tools.ietf.org/html/draft-eastlake-fnv-14#section-3
uint32_t hash_fnv1a(const void *data, unsigned len) {
    const uint8_t *d = (const uint8_t *)data;
    uint32_t h = 0x811c9dc5;
    while (len--)
        h = (h * 0x1000193) ^ *d++;
    return h;
}

uint32_t device_id_hash() {
    uint32_t *uid = (uint32_t *)UID_BASE;
    return hash_fnv1a(uid, 12);
}

uint64_t device_id() {
    static uint64_t cache;
    if (!cache) {
        // serial: 0x330073 0x31365012 0x20313643
        // serial: 0x71007A 0x31365012 0x20313643
        // DMESG("serial: %x %x %x", ((uint32_t *)UID_BASE)[0], ((uint32_t *)UID_BASE)[1],
        //      ((uint32_t *)UID_BASE)[2]);

        // clear "universal" bit
        uint32_t w0 = ((uint32_t *)UID_BASE)[0] & ~0x02000000;
        uint32_t w1 = device_id_hash();
        cache = (uint64_t)w0 << 32 | w1;
    }
    return cache;
}

// need to seed from somewhere if no DevID, eg http://robseward.com/misc/RNG2/

static uint32_t seed;
void seed_random(uint32_t s) {
    seed = (seed * 0x1000193) ^ s;
    DMESG("random seed %x", seed);
}

uint32_t random() {
    if (seed == 0)
        seed_random(device_id_hash());

    // xorshift algorithm
    uint32_t x = seed;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    seed = x;
    return x;
}

// return v +/- 25% or so
uint32_t random_around(uint32_t v) {
    uint32_t mask = 0xfffffff;
    while (mask > v)
        mask >>= 1;
    return (v - (mask >> 1)) + (random() & mask);
}

// faster versions of memcpy() and memset()
void *memcpy(void *dst, const void *src, size_t sz) {
    if (sz >= 4 && !((uintptr_t)dst & 3) && !((uintptr_t)src & 3)) {
        size_t cnt = sz >> 2;
        uint32_t *d = (uint32_t *)dst;
        const uint32_t *s = (const uint32_t *)src;
        while (cnt--) {
            *d++ = *s++;
        }
        sz &= 3;
        dst = d;
        src = s;
    }

    uint8_t *dd = (uint8_t *)dst;
    uint8_t *ss = (uint8_t *)src;

    while (sz--) {
        *dd++ = *ss++;
    }

    return dst;
}

void *memset(void *dst, int v, size_t sz) {
    if (sz >= 4 && !((uintptr_t)dst & 3)) {
        size_t cnt = sz >> 2;
        uint32_t vv = 0x01010101 * v;
        uint32_t *d = (uint32_t *)dst;
        while (cnt--) {
            *d++ = vv;
        }
        sz &= 3;
        dst = d;
    }

    uint8_t *dd = (uint8_t *)dst;

    while (sz--) {
        *dd++ = v;
    }

    return dst;
}
