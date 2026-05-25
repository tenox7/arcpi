#ifndef _ARM_WCHAR_H_
#define _ARM_WCHAR_H_

//
// cmp.h pulls in <wchar.h>; the boot CM subset uses no wide-char library
// routines (verified: no wcs*/towupper references in the 13 bconfig files),
// so this is an empty guard. WCHAR itself is the NT 16-bit type from ntdef.h.
//

#endif // _ARM_WCHAR_H_
