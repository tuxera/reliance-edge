/*             ----> DO NOT REMOVE THE FOLLOWING NOTICE <----

                  Copyright (c) 2014-2025 Tuxera US Inc.
                      All Rights Reserved Worldwide.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; use version 2 of the License.

    This program is distributed in the hope that it will be useful,
    but "AS-IS," WITHOUT ANY WARRANTY; without even the implied warranty
    of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, see <https://www.gnu.org/licenses/>.
*/
/*  Businesses and individuals that for commercial or other reasons cannot
    comply with the terms of the GPLv2 license must obtain a commercial
    license before incorporating Reliance Edge into proprietary software
    for distribution in any form.

    Visit https://www.tuxera.com/products/tuxera-edge-fs/ for more information.
*/
/** @file
    @brief Reliance Edge utilities only needed for tests.
*/
#ifndef REDTESTUTILS_H
#define REDTESTUTILS_H


#define ISDIGIT(c)  (((c) >= '0') && ((c) <= '9'))


void RedRandSeed(uint64_t ullSeed);
uint64_t RedRand64(uint64_t *pullSeed);
uint32_t RedRand32(uint32_t *pulSeed);

char *RedRatio(char *pBuffer, uint32_t ulBufferLen, uint64_t ullDividend, uint64_t ullDivisor, uint32_t ulDecPlaces);
uint64_t RedMulDiv64(uint64_t ullBase, uint32_t ulMultiplier, uint64_t ullDivisor);
uint64_t RedUint64DivMod32(uint64_t ullDividend, uint32_t ulDivisor, uint32_t *pulRemainder);
uint64_t RedUint64DivMod64(uint64_t ullDividend, uint64_t ullDivisor, uint64_t *pullRemainder);

int32_t RedAtoI(const char *pszNum);
const char *RedHtoUL(const char *pszNum, uint32_t *pulNum);
const char *RedHtoULL(const char *pszNum, uint64_t *pullNum);
const char *RedNtoUL(const char *pszNum, uint32_t *pulNum);
const char *RedNtoULL(const char *pszNum, uint64_t *pullNum);
const char *RedSizeToUL(const char *pszNum, uint32_t *pulResult);

#include <stdarg.h>

#if REDCONF_OUTPUT == 1
void RedPrintf(const char *pszFormat, ...);
void RedVPrintf(const char *pszFormat, va_list arglist);
#endif
int32_t RedSNPrintf(char *pcBuffer, uint32_t ulBufferLen, const char *pszFormat, ...);
int32_t RedVSNPrintf(char *pcBuffer, uint32_t ulBufferLen, const char *pszFormat, va_list arglist);


#endif /* REDTESTUTILS_H */
