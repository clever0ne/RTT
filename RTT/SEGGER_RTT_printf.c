/*********************************************************************
*                    SEGGER Microcontroller GmbH                     *
*                        The Embedded Experts                        *
**********************************************************************
*                                                                    *
*            (c) 1995 - 2021 SEGGER Microcontroller GmbH             *
*                                                                    *
*       www.segger.com     Support: support@segger.com               *
*                                                                    *
**********************************************************************
*                                                                    *
*       SEGGER RTT * Real Time Transfer for embedded targets         *
*                                                                    *
**********************************************************************
*                                                                    *
* All rights reserved.                                               *
*                                                                    *
* SEGGER strongly recommends to not make any changes                 *
* to or modify the source code of this software in order to stay     *
* compatible with the RTT protocol and J-Link.                       *
*                                                                    *
* Redistribution and use in source and binary forms, with or         *
* without modification, are permitted provided that the following    *
* condition is met:                                                  *
*                                                                    *
* o Redistributions of source code must retain the above copyright   *
*   notice, this condition and the following disclaimer.             *
*                                                                    *
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND             *
* CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,        *
* INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF           *
* MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE           *
* DISCLAIMED. IN NO EVENT SHALL SEGGER Microcontroller BE LIABLE FOR *
* ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR           *
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT  *
* OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;    *
* OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF      *
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT          *
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE  *
* USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH   *
* DAMAGE.                                                            *
*                                                                    *
**********************************************************************
*                                                                    *
*       RTT version: 8.56a                                           *
*                                                                    *
**********************************************************************

---------------------------END-OF-HEADER------------------------------
File    : SEGGER_RTT_printf.c
Purpose : Replacement for printf to write formatted data via RTT
Revision: $Rev: 17697 $
----------------------------------------------------------------------
*/
#include "SEGGER_RTT.h"
#include "SEGGER_RTT_Conf.h"

/*********************************************************************
*
*       Defines, configurable
*
**********************************************************************
*/

#ifndef SEGGER_RTT_PRINTF_BUFFER_SIZE
  #define SEGGER_RTT_PRINTF_BUFFER_SIZE (64)
#endif

#include <stdlib.h>
#include <stdarg.h>


#define FORMAT_FLAG_LEFT_JUSTIFY   (1u << 0)
#define FORMAT_FLAG_PAD_ZERO       (1u << 1)
#define FORMAT_FLAG_PRINT_SIGN     (1u << 2)
#define FORMAT_FLAG_ALTERNATE      (1u << 3)

/*********************************************************************
*
*       Types
*
**********************************************************************
*/

typedef struct {
  char*     pBuffer;
  unsigned  BufferSize;
  unsigned  Cnt;

  int   ReturnValue;

  unsigned RTTBufferIndex;
} SEGGER_RTT_PRINTF_DESC;

/*********************************************************************
*
*       Function prototypes
*
**********************************************************************
*/

/*********************************************************************
*
*       Static code
*
**********************************************************************
*/
/*********************************************************************
*
*       _StoreChar
*/
static void _StoreChar(SEGGER_RTT_PRINTF_DESC * p, char c) {
  unsigned Cnt;

  Cnt = p->Cnt;
  if ((Cnt + 1u) <= p->BufferSize) {
    *(p->pBuffer + Cnt) = c;
    p->Cnt = Cnt + 1u;
    p->ReturnValue++;
  }
  //
  // Write part of string, when the buffer is full
  //
  if (p->Cnt == p->BufferSize) {
    if (SEGGER_RTT_Write(p->RTTBufferIndex, p->pBuffer, p->Cnt) != p->Cnt) {
      p->ReturnValue = -1;
    } else {
      p->Cnt = 0u;
    }
  }
}

/*********************************************************************
*
*       _PrintUnsigned
*/
static void _PrintUnsigned(SEGGER_RTT_PRINTF_DESC * pBufferDesc, unsigned v, unsigned Base, int NumDigits, int FieldWidth, unsigned FormatFlags) {
  static const char _aV2C[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
  unsigned Div;
  unsigned Digit;
  unsigned Number;
  int Width;
  char c;

  /*
   * cppreference: If both the converted value and the precision are `0` the conversion results in no characters.
   */
  if (NumDigits == 0 && v == 0u) {
    return;
  }

  /*
   * cppreference: Precision specifies the minimum number of digits to appear. The default precision is `1`.
   */
  if (NumDigits < 0) {
    NumDigits = 1;
  }

  Number = v;
  Digit = 1u;
  //
  // Get actual field width
  //
  Width = 1;
  while (Number >= Base) {
    Number = (Number / Base);
    Width++;
  }
  if (NumDigits > Width) {
    Width = NumDigits;
  }
  //
  // Print leading chars if necessary
  //
  if ((FormatFlags & FORMAT_FLAG_LEFT_JUSTIFY) == 0u) {
    if (FieldWidth != 0) {
      if ((FormatFlags & FORMAT_FLAG_PAD_ZERO) == FORMAT_FLAG_PAD_ZERO) {
        c = '0';
      } else {
        c = ' ';
      }
      while ((FieldWidth != 0) && (Width < FieldWidth)) {
        FieldWidth--;
        _StoreChar(pBufferDesc, c);
        if (pBufferDesc->ReturnValue < 0) {
          break;
        }
      }
    }
  }
  if (pBufferDesc->ReturnValue >= 0) {
    //
    // Compute Digit.
    // Loop until Digit has the value of the highest digit required.
    // Example: If the output is 345 (Base 10), loop 2 times until Digit is 100.
    //
    while (1) {
      if (NumDigits > 1) {       // User specified a min number of digits to print? => Make sure we loop at least that often, before checking anything else (> 1 check avoids problems with NumDigits being signed / unsigned)
        NumDigits--;
      } else {
        Div = v / Digit;
        if (Div < Base) {        // Is our divider big enough to extract the highest digit from value? => Done
          break;
        }
      }
      Digit *= Base;
    }
    //
    // Output digits
    //
    do {
      Div = v / Digit;
      v -= Div * Digit;
      _StoreChar(pBufferDesc, _aV2C[Div]);
      if (pBufferDesc->ReturnValue < 0) {
        break;
      }
      Digit /= Base;
    } while (Digit);
    //
    // Print trailing spaces if necessary
    //
    if ((FormatFlags & FORMAT_FLAG_LEFT_JUSTIFY) == FORMAT_FLAG_LEFT_JUSTIFY) {
      if (FieldWidth != 0) {
        while ((FieldWidth != 0) && (Width < FieldWidth)) {
          FieldWidth--;
          _StoreChar(pBufferDesc, ' ');
          if (pBufferDesc->ReturnValue < 0) {
            break;
          }
        }
      }
    }
  }
}

/*********************************************************************
*
*       _PrintInt
*/
static void _PrintInt(SEGGER_RTT_PRINTF_DESC * pBufferDesc, int v, unsigned Base, int NumDigits, int FieldWidth, unsigned FormatFlags) {
  int Width;
  unsigned Number;

  /*
   * cppreference: If both the converted value and the precision are `0` the conversion results in no characters.
   */
  if (NumDigits == 0 && v == 0) {
    return;
  }

  /*
   * cppreference: Precision specifies the minimum number of digits to appear. The default precision is `1`.
   */
  if (NumDigits < 0) {
    NumDigits = 1;
  }

  Number = (v < 0) ? -v : v;

  //
  // Get actual field width
  //
  Width = 1;
  while (Number >= Base) {
    Number = (Number / Base);
    Width++;
  }
  if (NumDigits > Width) {
    Width = NumDigits;
  }
  if ((FieldWidth > 0) && ((v < 0) || ((FormatFlags & FORMAT_FLAG_PRINT_SIGN) == FORMAT_FLAG_PRINT_SIGN))) {
    FieldWidth--;
  }

  //
  // Print leading spaces if necessary
  //
  if (((FormatFlags & FORMAT_FLAG_PAD_ZERO) == 0u) && ((FormatFlags & FORMAT_FLAG_LEFT_JUSTIFY) == 0u)) {
    if (FieldWidth != 0) {
      while ((FieldWidth != 0) && (Width < FieldWidth)) {
        FieldWidth--;
        _StoreChar(pBufferDesc, ' ');
        if (pBufferDesc->ReturnValue < 0) {
          break;
        }
      }
    }
  }
  //
  // Print sign if necessary
  //
  if (pBufferDesc->ReturnValue >= 0) {
    if (v < 0) {
      v = -v;
      _StoreChar(pBufferDesc, '-');
    } else if ((FormatFlags & FORMAT_FLAG_PRINT_SIGN) == FORMAT_FLAG_PRINT_SIGN) {
      _StoreChar(pBufferDesc, '+');
    }
    /*
     * Leading zeros will be printed by _PrintUnsigned
     */
    if (pBufferDesc->ReturnValue >= 0) {
      //
      // Print number without sign
      //
      _PrintUnsigned(pBufferDesc, (unsigned)v, Base, NumDigits, FieldWidth, FormatFlags);
    }
  }
}

/*********************************************************************
*
*       Public code
*
**********************************************************************
*/
/*********************************************************************
*
*       SEGGER_RTT_vprintf
*
*  Function description
*    Stores a formatted string in SEGGER RTT control block.
*    This data is read by the host.
*
*  Parameters
*    BufferIndex  Index of "Up"-buffer to be used. (e.g. 0 for "Terminal")
*    sFormat      Pointer to format string
*    pParamList   Pointer to the list of arguments for the format string
*
*  Return values
*    >= 0:  Number of bytes which have been stored in the "Up"-buffer.
*     < 0:  Error
*/
int SEGGER_RTT_vprintf(unsigned BufferIndex, const char * sFormat, va_list * pParamList) {
  char c;
  SEGGER_RTT_PRINTF_DESC BufferDesc;
  int v;
  int Precision;
  unsigned FormatFlags;
  int FieldWidth;
  char acBuffer[SEGGER_RTT_PRINTF_BUFFER_SIZE];

  BufferDesc.pBuffer        = acBuffer;
  BufferDesc.BufferSize     = SEGGER_RTT_PRINTF_BUFFER_SIZE;
  BufferDesc.Cnt            = 0u;
  BufferDesc.RTTBufferIndex = BufferIndex;
  BufferDesc.ReturnValue    = 0;

  do {
    c = *sFormat;
    sFormat++;
    if (c == 0u) {
      break;
    }
    if (c == '%') {
      //
      // Filter out flags
      //
      FormatFlags = 0u;
      v = 1;
      do {
        c = *sFormat;
        switch (c) {
        case '-': FormatFlags |= FORMAT_FLAG_LEFT_JUSTIFY; sFormat++; break;
        case '0': FormatFlags |= FORMAT_FLAG_PAD_ZERO;     sFormat++; break;
        case '+': FormatFlags |= FORMAT_FLAG_PRINT_SIGN;   sFormat++; break;
        case '#': FormatFlags |= FORMAT_FLAG_ALTERNATE;    sFormat++; break;
        default:  v = 0; break;
        }
      } while (v);
      //
      // filter out field with
      //
      FieldWidth = 0;
      /*
       * cppreference: In the case when `*` is used, the width is specified by an additional argument of type `int`,
       * which appears before the argument to be converted and the argument supplying precision if one is supplied.
       */
      if (*sFormat == '*') {
        sFormat++;
        FieldWidth = va_arg(*pParamList, int);
        /*
         * cppreference: If the value of the argument is negative, it results with the `-` flag specified and positive field width.
         */
        if (FieldWidth < 0) {
          FieldWidth = -FieldWidth;
          FormatFlags |= FORMAT_FLAG_LEFT_JUSTIFY;
        }
      } else {
        do {
          c = *sFormat;
          if ((c < '0') || (c > '9')) {
            break;
          }
          sFormat++;
          FieldWidth = FieldWidth * 10 + ((int)c - '0');
        } while (1);
      }
      //
      // Filter out precision (number of digits to display)
      //
      Precision = -1;
      c = *sFormat;
      if (c == '.') {
        sFormat++;
        /*
         * cppreference: In the case when `*` is used, the precision is specified by an additional argument of type `int`,
         * which appears before the argument to be converted, but after the argument supplying minimum field width if one is supplied.
         */
        if (*sFormat == '*') {
          sFormat++;
          Precision = va_arg(*pParamList, int);
          /* 
           * cppreference: If the value of this argument is negative, it is ignored.
           */
          if (Precision < 0) {
            Precision = -1;
          }
        } else {
          /*
           * cppreference: If neither a number nor `*` is used, the precision is taken as zero.
           */
          Precision = 0;
          do {
            c = *sFormat;
            if ((c < '0') || (c > '9')) {
              break;
            }
            sFormat++;
            Precision = Precision * 10 + ((int)c - '0');
          } while (1);
        }
      }
      //
      // Filter out length modifier
      //
      c = *sFormat;
      do {
        if ((c == 'l') || (c == 'h')) {
          sFormat++;
          c = *sFormat;
        } else {
          break;
        }
      } while (1);
      //
      // Handle specifiers
      //
      switch (c) {
      case 'c': {
        char c0;
        v = va_arg(*pParamList, int);
        c0 = (char)v;
        _StoreChar(&BufferDesc, c0);
        break;
      }
      case 'd':
      case 'i':
        v = va_arg(*pParamList, int);
        _PrintInt(&BufferDesc, v, 10u, Precision, FieldWidth, FormatFlags);
        break;
      case 'u':
        v = va_arg(*pParamList, int);
        _PrintUnsigned(&BufferDesc, (unsigned)v, 10u, Precision, FieldWidth, FormatFlags);
        break;
      case 'o':
        v = va_arg(*pParamList, int);
        _PrintUnsigned(&BufferDesc, (unsigned)v, 8u, Precision, FieldWidth, FormatFlags);
        break;
      case 'x':
      case 'X':
        v = va_arg(*pParamList, int);
        _PrintUnsigned(&BufferDesc, (unsigned)v, 16u, Precision, FieldWidth, FormatFlags);
        break;
      case 's':
        {
          const char * s = va_arg(*pParamList, const char *);
          if (s == NULL) {
            s = "(NULL)";   // Print (NULL) instead of crashing or breaking, as it is more informative to the user.
            Precision = -1; // Make sure (NULL) is printed, even when precision was set.
          }
          do {
            c = *s;
            s++;
            if (c == '\0') {
              break;
            }
            if (Precision == 0) {
              break;
            }
            _StoreChar(&BufferDesc, c);
            Precision--;
          } while (BufferDesc.ReturnValue >= 0);
        }
        break;
      case 'p':
        v = va_arg(*pParamList, int);
        _PrintUnsigned(&BufferDesc, (unsigned)v, 16u, 8, 8, 0u);
        break;
      case '%':
        _StoreChar(&BufferDesc, '%');
        break;
      default:
        break;
      }
      sFormat++;
    } else {
      _StoreChar(&BufferDesc, c);
    }
  } while (BufferDesc.ReturnValue >= 0);

  if (BufferDesc.ReturnValue > 0) {
    //
    // Write remaining data, if any
    //
    if (BufferDesc.Cnt != 0u) {
      SEGGER_RTT_Write(BufferIndex, acBuffer, BufferDesc.Cnt);
    }
    BufferDesc.ReturnValue += (int)BufferDesc.Cnt;
  }
  return BufferDesc.ReturnValue;
}

/*********************************************************************
*
*       SEGGER_RTT_printf
*
*  Function description
*    Stores a formatted string in SEGGER RTT control block.
*    This data is read by the host.
*
*  Parameters
*    BufferIndex  Index of "Up"-buffer to be used. (e.g. 0 for "Terminal")
*    sFormat      Pointer to format string, followed by the arguments for conversion
*
*  Return values
*    >= 0:  Number of bytes which have been stored in the "Up"-buffer.
*     < 0:  Error
*
*  Notes
*    (1) Conversion specifications have following syntax:
*          %[flags][FieldWidth][.Precision]ConversionSpecifier
*    (2) Supported flags:
*          -: Left justify within the field width
*          +: Always print sign extension for signed conversions
*          0: Pad with 0 instead of spaces. Ignored when using '-'-flag or precision
*    (3) Supported conversion specifiers:
*          c: Print the argument as one char
*          d: Print the argument as a signed integer
*          i: Print the argument as a signed integer
*          u: Print the argument as an unsigned integer
*          o: Print the argument as an octal integer
*          x: Print the argument as a hexadecimal integer
*          X: Print the argument as a hexadecimal integer
*          s: Print the string pointed to by the argument
*          p: Print the argument as an 8-digit hexadecimal integer. (Argument shall be a pointer to void.)
*/
int SEGGER_RTT_printf(unsigned BufferIndex, const char * sFormat, ...) {
  int r;
  va_list ParamList;

  va_start(ParamList, sFormat);
  r = SEGGER_RTT_vprintf(BufferIndex, sFormat, &ParamList);
  va_end(ParamList);
  return r;
}
/*************************** End of file ****************************/
