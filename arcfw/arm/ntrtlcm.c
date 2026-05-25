//
// RTL routines the boot-time Configuration Manager (arcfw/ported/config/*) calls
// that are inline x86 _asm or table-driven in the real NTRTL and so are not in
// our minimal shim layer. Scope was measured against the 13 bconfig files:
// 7 UNICODE_STRING/ANSI string routines + 4 RTL bitmap routines. Prototypes are
// in cmshim.h. Registry key/value names are ASCII, so the Unicode case folding
// and ANSI->Unicode widening here are ASCII-correct (the real NTRTL uses the
// NLS tables, which the loader does not have at this point).
//
#include "ntos.h"
#include "cmshim.h"
#include "string.h"

#define STATUS_BUFFER_TOO_SMALL ((NTSTATUS)0xC0000023L)

static ULONG WideLen(PCWSTR s)
{
    ULONG n = 0;
    while (s[n] != 0) {
        n++;
    }
    return n;
}

WCHAR
RtlUpcaseUnicodeChar(IN WCHAR c)
{
    if (c >= L'a' && c <= L'z') {
        return (WCHAR)(c - (L'a' - L'A'));
    }
    return c;
}

VOID
RtlInitUnicodeString(OUT PUNICODE_STRING d, IN PCWSTR s)
{
    d->Buffer = (PWSTR)s;
    if (s == NULL) {
        d->Length = 0;
        d->MaximumLength = 0;
        return;
    }
    d->Length = (USHORT)(WideLen(s) * sizeof(WCHAR));
    d->MaximumLength = (USHORT)(d->Length + sizeof(WCHAR));
}

NTSTATUS
RtlAppendUnicodeToString(IN OUT PUNICODE_STRING d, IN PCWSTR s)
{
    USHORT bytes;

    if (s == NULL) {
        return STATUS_SUCCESS;
    }
    bytes = (USHORT)(WideLen(s) * sizeof(WCHAR));
    if (d->Length + bytes > d->MaximumLength) {
        return STATUS_BUFFER_TOO_SMALL;
    }
    memcpy((PUCHAR)d->Buffer + d->Length, s, bytes);
    d->Length = (USHORT)(d->Length + bytes);
    if (d->Length + (USHORT)sizeof(WCHAR) <= d->MaximumLength) {
        d->Buffer[d->Length / sizeof(WCHAR)] = 0;
    }
    return STATUS_SUCCESS;
}

NTSTATUS
RtlAppendUnicodeStringToString(IN OUT PUNICODE_STRING d, IN PUNICODE_STRING s)
{
    if (s->Length == 0) {
        return STATUS_SUCCESS;
    }
    if (d->Length + s->Length > d->MaximumLength) {
        return STATUS_BUFFER_TOO_SMALL;
    }
    memcpy((PUCHAR)d->Buffer + d->Length, s->Buffer, s->Length);
    d->Length = (USHORT)(d->Length + s->Length);
    if (d->Length + (USHORT)sizeof(WCHAR) <= d->MaximumLength) {
        d->Buffer[d->Length / sizeof(WCHAR)] = 0;
    }
    return STATUS_SUCCESS;
}

BOOLEAN
RtlEqualUnicodeString(IN PUNICODE_STRING s1, IN PUNICODE_STRING s2, IN BOOLEAN ci)
{
    USHORT i, n;

    if (s1->Length != s2->Length) {
        return FALSE;
    }
    n = (USHORT)(s1->Length / sizeof(WCHAR));
    for (i = 0; i < n; i++) {
        WCHAR a = s1->Buffer[i];
        WCHAR b = s2->Buffer[i];
        if (ci) {
            a = RtlUpcaseUnicodeChar(a);
            b = RtlUpcaseUnicodeChar(b);
        }
        if (a != b) {
            return FALSE;
        }
    }
    return TRUE;
}

LONG
RtlCompareUnicodeString(IN PUNICODE_STRING s1, IN PUNICODE_STRING s2, IN BOOLEAN ci)
{
    USHORT i, n;
    USHORT n1 = (USHORT)(s1->Length / sizeof(WCHAR));
    USHORT n2 = (USHORT)(s2->Length / sizeof(WCHAR));

    n = (n1 < n2) ? n1 : n2;
    for (i = 0; i < n; i++) {
        WCHAR a = s1->Buffer[i];
        WCHAR b = s2->Buffer[i];
        if (ci) {
            a = RtlUpcaseUnicodeChar(a);
            b = RtlUpcaseUnicodeChar(b);
        }
        if (a != b) {
            return (LONG)a - (LONG)b;
        }
    }
    return (LONG)n1 - (LONG)n2;
}

NTSTATUS
RtlAnsiStringToUnicodeString(IN OUT PUNICODE_STRING d, IN PANSI_STRING s, IN BOOLEAN alloc)
{
    USHORT i;
    USHORT srcLen = s->Length;                          // ANSI bytes
    USHORT needed = (USHORT)(srcLen * sizeof(WCHAR));

    if (alloc) {
        return STATUS_INVALID_PARAMETER;                // boot subset only ever calls with FALSE
    }
    if (needed + (USHORT)sizeof(WCHAR) > d->MaximumLength) {
        return STATUS_BUFFER_TOO_SMALL;
    }
    for (i = 0; i < srcLen; i++) {
        d->Buffer[i] = (WCHAR)(UCHAR)s->Buffer[i];      // ASCII widen
    }
    d->Buffer[srcLen] = 0;
    d->Length = needed;
    return STATUS_SUCCESS;
}

//
// RTL bitmap (LSB-first within each ULONG, the NTRTL convention). HvInitializeHive
// builds the hive's free/dirty vector with these.
//
VOID
RtlInitializeBitMap(IN PRTL_BITMAP h, IN PULONG buf, IN ULONG size)
{
    h->SizeOfBitMap = size;
    h->Buffer = buf;
}

VOID
RtlClearAllBits(IN PRTL_BITMAP h)
{
    memset(h->Buffer, 0x00, ((h->SizeOfBitMap + 31) / 32) * sizeof(ULONG));
}

VOID
RtlSetAllBits(IN PRTL_BITMAP h)
{
    memset(h->Buffer, 0xFF, ((h->SizeOfBitMap + 31) / 32) * sizeof(ULONG));
}

ULONG
RtlNumberOfSetBits(IN PRTL_BITMAP h)
{
    ULONG i, count = 0;

    for (i = 0; i < h->SizeOfBitMap; i++) {
        if (h->Buffer[i >> 5] & (1u << (i & 31))) {
            count++;
        }
    }
    return count;
}

//
// Bit-scan lookup tables, verbatim from PRIVATE/NTOS/KE/KERNLDAT.C. hivecell.c
// indexes these by a byte to find a set-bit position when bucketing free cells
// by size (CmpFindFirstSetRight/Left). KiFindFirstSetRight[b] = position (0..7)
// of the lowest set bit in b; KiFindFirstSetLeft[b] = position of the highest.
// Normally KE arch data; supplied here for the boot CM.
//
CCHAR KiFindFirstSetRight[256] = {
        0, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
        4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
        5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
        4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
        6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
        4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
        5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
        4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
        7, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
        4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
        5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
        4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
        6, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
        4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
        5, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0,
        4, 0, 1, 0, 2, 0, 1, 0, 3, 0, 1, 0, 2, 0, 1, 0};

CCHAR KiFindFirstSetLeft[256] = {
        0, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3,
        4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7};
