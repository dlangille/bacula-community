/*
 * Copyright Patrick Powell 1995
 *
 * This code is based on code written by Patrick Powell
 * (papowell@astart.com) It may be used for any purpose as long
 * as this notice remains intact on all source code distributions.
 *
 * Adapted for Bacula -- note there were lots of bugs in
 *     the original code: %lld and %s were seriously broken, and
 *     with FP turned off %f seg faults.
 *   Kern Sibbald, November MMV
 *
 *   Version $Id$
 */


#include "bacula.h"
#define FP_OUTPUT 1 /* Bacula uses floating point */

/* 
    Temp only for me -- NOT YET READY FOR USE -- seems to work fine
    on Linux, but doesn't build correctly on Win32
 */
#ifdef USE_BSNPRINTF

#ifdef HAVE_LONG_DOUBLE
#define LDOUBLE long double
#else
#define LDOUBLE double
#endif

int bvsnprintf(char *buffer, int32_t maxlen, const char *format, va_list args);
static int32_t fmtstr(char *buffer, int32_t currlen, int32_t maxlen,
                   char *value, int flags, int min, int max);
static int32_t fmtint(char *buffer, int32_t currlen, int32_t maxlen,
                   int64_t value, int base, int min, int max, int flags);

#ifdef FP_OUTPUT
# ifdef HAVE_FCVTL
#  define fcvt fcvtl
# endif
static int32_t fmtfp(char *buffer, int32_t currlen, int32_t maxlen,
                  LDOUBLE fvalue, int min, int max, int flags);
#else
#define fmtfp(b, c, m, f, min, max, fl) currlen
#endif

#define outch(c) {int len=currlen; if (currlen++ < maxlen) { buffer[len] = (c);}}


/* format read states */
#define DP_S_DEFAULT 0
#define DP_S_FLAGS   1
#define DP_S_MIN     2
#define DP_S_DOT     3
#define DP_S_MAX     4
#define DP_S_MOD     5
#define DP_S_CONV    6
#define DP_S_DONE    7

/* format flags - Bits */
#define DP_F_MINUS      (1 << 0)
#define DP_F_PLUS       (1 << 1)
#define DP_F_SPACE      (1 << 2)
#define DP_F_NUM        (1 << 3)
#define DP_F_ZERO       (1 << 4)
#define DP_F_UP         (1 << 5)
#define DP_F_UNSIGNED   (1 << 6)
#define DP_F_DOT        (1 << 7)

/* Conversion Flags */
#define DP_C_INT16   1
#define DP_C_INT32    2
#define DP_C_LDOUBLE 3
#define DP_C_INT64   4

#define char_to_int(p) ((p)- '0')
#define MAX(p,q) (((p) >= (q)) ? (p) : (q))

/*
  You might ask why does Bacula have it's own printf routine? Well,
  There are two reasons: 1. Here (as opposed to library routines), we
  define %d and %ld to be 32 bit; %lld and %q to be 64 bit.  2. We 
  disable %n for security reasons.                
 */

int bsnprintf(char *str, int32_t size, const char *fmt,  ...)
{
   va_list   arg_ptr;
   int len;

   va_start(arg_ptr, fmt);
   len = bvsnprintf(str, size, fmt, arg_ptr);
   va_end(arg_ptr);
   return len;
}


int bvsnprintf(char *buffer, int32_t maxlen, const char *format, va_list args)
{
   char ch;
   int64_t value;
   char *strvalue;
   int min;
   int max;
   int state;
   int flags;
   int cflags;
   int32_t currlen;
   int base;
   int junk;
#ifdef FP_OUTPUT
   LDOUBLE fvalue;
#endif

   state = DP_S_DEFAULT;
   currlen = flags = cflags = min = 0;
   max = -1;
   ch = *format++;
   *buffer = 0;

   while (state != DP_S_DONE) {
      if ((ch == '\0') || (currlen >= maxlen))
         state = DP_S_DONE;

      switch (state) {
      case DP_S_DEFAULT:
         if (ch == '%') {
            state = DP_S_FLAGS;
         } else {
            outch(ch);
         }
         ch = *format++;
         break;
      case DP_S_FLAGS:
         switch (ch) {
         case '-':
            flags |= DP_F_MINUS;
            ch = *format++;
            break;
         case '+':
            flags |= DP_F_PLUS;
            ch = *format++;
            break;
         case ' ':
            flags |= DP_F_SPACE;
            ch = *format++;
            break;
         case '#':
            flags |= DP_F_NUM;
            ch = *format++;
            break;
         case '0':
            flags |= DP_F_ZERO;
            ch = *format++;
            break;
         default:
            state = DP_S_MIN;
            break;
         }
         break;
      case DP_S_MIN:
         if (isdigit((unsigned char)ch)) {
            min = 10 * min + char_to_int(ch);
            ch = *format++;
         } else if (ch == '*') {
#ifdef SECURITY_PROBLEM
            min = va_arg(args, int);
#else
            junk = va_arg(args, int);
#endif
            ch = *format++;
            state = DP_S_DOT;
         } else
            state = DP_S_DOT;
         break;
      case DP_S_DOT:
         if (ch == '.') {
            state = DP_S_MAX;
            flags |= DP_F_DOT;
            ch = *format++;
         } else
            state = DP_S_MOD;
         break;
      case DP_S_MAX:
         if (isdigit((unsigned char)ch)) {
            if (max < 0)
               max = 0;
            max = 10 * max + char_to_int(ch);
            ch = *format++;
         } else if (ch == '*') {
#ifdef SECURITY_PROBLEM
            max = va_arg(args, int);
#else
            junk = va_arg(args, int);
#endif
            ch = *format++;
            state = DP_S_MOD;
         } else
            state = DP_S_MOD;
         break;
      case DP_S_MOD:
         switch (ch) {
         case 'h':
            cflags = DP_C_INT16;
            ch = *format++;
            break;
         case 'l':
            cflags = DP_C_INT32;
            ch = *format++;
            if (ch == 'l') {       /* It's a long long */
               cflags = DP_C_INT64;
               ch = *format++;
            }
            break;
         case 'L':
            cflags = DP_C_LDOUBLE;
            ch = *format++;
            break;
         default:
            break;
         }
         state = DP_S_CONV;
         break;
      case DP_S_CONV:
         switch (ch) {
         case 'd':
         case 'i':
            if (cflags == DP_C_INT16) {
               value = va_arg(args, int32_t);
            } else if (cflags == DP_C_INT32) {
               value = va_arg(args, int32_t);
            } else if (cflags == DP_C_INT64) {
               value = va_arg(args, int64_t);
            } else {
               value = va_arg(args, int);
            }
            currlen = fmtint(buffer, currlen, maxlen, value, 10, min, max, flags);
            break;
         case 'X':
         case 'x':
         case 'o':
         case 'u':
            if (ch == 'o') {
               base = 8;
            } else if (ch == 'x') {
               base = 16;
            } else if (ch == 'X') {
               base = 16;
               flags |= DP_F_UP;
            } else {
               base = 10;
            }
            flags |= DP_F_UNSIGNED;
            if (cflags == DP_C_INT16) {
               value = va_arg(args, uint32_t);
            } else if (cflags == DP_C_INT32) {
               value = (long)va_arg(args, uint32_t);
            } else if (cflags == DP_C_INT64) {
               value = (int64_t) va_arg(args, uint64_t);
            } else {
               value = (long)va_arg(args, unsigned int);
            }
            currlen = fmtint(buffer, currlen, maxlen, value, base, min, max, flags);
            break;
         case 'f':
            if (cflags == DP_C_LDOUBLE) {
               fvalue = va_arg(args, LDOUBLE);
            } else {
               fvalue = va_arg(args, double);
            }
            currlen = fmtfp(buffer, currlen, maxlen, fvalue, min, max, flags);
            break;
         case 'E':
            flags |= DP_F_UP;
         case 'e':
            if (cflags == DP_C_LDOUBLE) {
               fvalue = va_arg(args, LDOUBLE);
            } else {
               fvalue = va_arg(args, double);
            }
            currlen = fmtfp(buffer, currlen, maxlen, fvalue, min, max, flags);
            break;
         case 'G':
            flags |= DP_F_UP;
         case 'g':
            if (cflags == DP_C_LDOUBLE) {
               fvalue = va_arg(args, LDOUBLE);
            } else {
               fvalue = va_arg(args, double);
            }
            currlen = fmtfp(buffer, currlen, maxlen, fvalue, min, max, flags);
            break;
         case 'c':
            outch(va_arg(args, int));
            break;
         case 's':
            strvalue = va_arg(args, char *);
            currlen = fmtstr(buffer, currlen, maxlen, strvalue, flags, min, max);
            break;
         case 'p':
            strvalue = va_arg(args, char *);
            currlen = fmtint(buffer, currlen, maxlen, (long)strvalue, 16, min, max, flags);
            break;
         case 'n':
            if (cflags == DP_C_INT16) {
               int16_t *num;
               num = va_arg(args, int16_t *);
#ifdef SECURITY_PROBLEM
               *num = currlen;
#endif
            } else if (cflags == DP_C_INT32) {
               int32_t *num;
               num = va_arg(args, int32_t *);
#ifdef SECURITY_PROBLEM
               *num = (int32_t)currlen;
#endif
            } else if (cflags == DP_C_INT64) {
               int64_t *num;
               num = va_arg(args, int64_t *);
#ifdef SECURITY_PROBLEM
               *num = (int64_t)currlen;
#endif
            } else {
               int32_t *num;
               num = va_arg(args, int32_t *);
#ifdef SECURITY_PROBLEM
               *num = (int32_t)currlen;
#endif
            }
            break;
         case '%':
            outch(ch);
            break;
         case 'w':
            /* not supported yet, treat as next char */
            ch = *format++;
            break;
         default:
            /* Unknown, skip */
            break;
         }
         ch = *format++;
         state = DP_S_DEFAULT;
         flags = cflags = min = 0;
         max = -1;
         break;
      case DP_S_DONE:
         break;
      default:
         /* hmm? */
         break;                    /* some picky compilers need this */
      }
   }
   if (currlen < maxlen - 1) {
      buffer[currlen] = '\0';
   } else {
      buffer[maxlen - 1] = '\0';
   }
   return currlen;
}

static int32_t fmtstr(char *buffer, int32_t currlen, int32_t maxlen,
                   char *value, int flags, int min, int max)
{
   int padlen, strln;              /* amount to pad */
   int cnt = 0;

   if (value == 0) {
      value = "<NULL>";
   }

   if (flags & DP_F_DOT && max < 0) {   /* Max not specified */
      max = 0;
   } else if (max < 0) {
      max = maxlen;
   }
   strln = strlen(value);
   if (strln > max) {
      strln = max;                /* truncate to max */
   }
   padlen = min - strln;
   if (padlen < 0) {
      padlen = 0;
   }
   if (flags & DP_F_MINUS) {
      padlen = -padlen;            /* Left Justify */
   }

   while (padlen > 0) {
      outch(' ');
      --padlen;
   }
   while (*value && (cnt < max)) {
      outch(*value++);
      ++cnt;
   }
   while (padlen < 0) {
      outch(' ');
      ++padlen;
   }
   return currlen;
}

/* Have to handle DP_F_NUM (ie 0x and 0 alternates) */

static int32_t fmtint(char *buffer, int32_t currlen, int32_t maxlen,
                   int64_t value, int base, int min, int max, int flags)
{
   int signvalue = 0;
   uint64_t uvalue;
   char convert[20];
   int place = 0;
   int spadlen = 0;                /* amount to space pad */
   int zpadlen = 0;                /* amount to zero pad */
   int caps = 0;

   if (max < 0) {
      max = 0;
   }

   uvalue = value;

   if (!(flags & DP_F_UNSIGNED)) {
      if (value < 0) {
         signvalue = '-';
         uvalue = -value;
      } else if (flags & DP_F_PLUS) {  /* Do a sign (+/i) */
         signvalue = '+';
      } else if (flags & DP_F_SPACE) {
         signvalue = ' ';
      }
   }

   if (flags & DP_F_UP) {
      caps = 1;                    /* Should characters be upper case? */
   }

   do {
      convert[place++] = (caps ? "0123456789ABCDEF" : "0123456789abcdef")
         [uvalue % (unsigned)base];
      uvalue = (uvalue / (unsigned)base);
   } while (uvalue && (place < 20));
   if (place == 20) {
      place--;
   }
   convert[place] = 0;

   zpadlen = max - place;
   spadlen = min - MAX(max, place) - (signvalue ? 1 : 0);
   if (zpadlen < 0)
      zpadlen = 0;
   if (spadlen < 0)
      spadlen = 0;
   if (flags & DP_F_ZERO) {
      zpadlen = MAX(zpadlen, spadlen);
      spadlen = 0;
   }
   if (flags & DP_F_MINUS)
      spadlen = -spadlen;          /* Left Justifty */

#ifdef DEBUG_SNPRINTF
   printf("zpad: %d, spad: %d, min: %d, max: %d, place: %d\n",
          zpadlen, spadlen, min, max, place);
#endif

   /* Spaces */
   while (spadlen > 0) {
      outch(' ');
      --spadlen;
   }

   /* Sign */
   if (signvalue) {
      outch(signvalue);
   }

   /* Zeros */
   if (zpadlen > 0) {
      while (zpadlen > 0) {
         outch('0');
         --zpadlen;
      }
   }

   /* Digits */
   while (place > 0) {
      outch(convert[--place]);
   }

   /* Left Justified spaces */
   while (spadlen < 0) {
      outch(' ');
      ++spadlen;
   }
   return currlen;
}

#ifdef FP_OUTPUT

static LDOUBLE abs_val(LDOUBLE value)
{
   LDOUBLE result = value;

   if (value < 0)
      result = -value;

   return result;
}

static LDOUBLE pow10(int exp)
{
   LDOUBLE result = 1;

   while (exp) {
      result *= 10;
      exp--;
   }

   return result;
}

static long round(LDOUBLE value)
{
   long intpart;

   intpart = (long)value;
   value = value - intpart;
   if (value >= 0.5)
      intpart++;

   return intpart;
}

static int32_t fmtfp(char *buffer, int32_t currlen, int32_t maxlen,
                  LDOUBLE fvalue, int min, int max, int flags)
{
   int signvalue = 0;
   LDOUBLE ufvalue;
#ifndef HAVE_FCVT
   char iconvert[20];
   char fconvert[20];
#else
   char iconvert[311];
   char fconvert[311];
   char *result;
   int dec_pt, sig;
   int r_length;
   extern char *fcvt(double value, int ndigit, int *decpt, int *sign);
#endif
   int iplace = 0;
   int fplace = 0;
   int padlen = 0;                 /* amount to pad */
   int zpadlen = 0;
   int caps = 0;
   int64_t intpart;
   int64_t fracpart;

   /* 
    * AIX manpage says the default is 0, but Solaris says the default
    * is 6, and sprintf on AIX defaults to 6
    */
   if (max < 0)
      max = 6;

   ufvalue = abs_val(fvalue);

   if (fvalue < 0)
      signvalue = '-';
   else if (flags & DP_F_PLUS)     /* Do a sign (+/i) */
      signvalue = '+';
   else if (flags & DP_F_SPACE)
      signvalue = ' ';

#if 0
   if (flags & DP_F_UP)
      caps = 1;                    /* Should characters be upper case? */
#endif

#ifndef HAVE_FCVT
   intpart = (long)ufvalue;

   /* 
    * Sorry, we only support 9 digits past the decimal because of our 
    * conversion method
    */
   if (max > 9)
      max = 9;

   /* We "cheat" by converting the fractional part to integer by
    * multiplying by a factor of 10
    */
   fracpart = round((pow10(max)) * (ufvalue - intpart));

   if (fracpart >= pow10(max)) {
      intpart++;
      fracpart -= (int64_t)pow10(max);
   }
#ifdef DEBUG_SNPRINTF
   printf("fmtfp: %g %d.%d min=%d max=%d\n",
          (double)fvalue, intpart, fracpart, min, max);
#endif

   /* Convert integer part */
   do {
      iconvert[iplace++] =
         (caps ? "0123456789ABCDEF" : "0123456789abcdef")[intpart % 10];
      intpart = (intpart / 10);
   } while (intpart && (iplace < 20));
   if (iplace == 20)
      iplace--;
   iconvert[iplace] = 0;

   /* Convert fractional part */
   do {
      fconvert[fplace++] =
         (caps ? "0123456789ABCDEF" : "0123456789abcdef")[fracpart % 10];
      fracpart = (fracpart / 10);
   } while (fracpart && (fplace < 20));
   if (fplace == 20)
      fplace--;
   fconvert[fplace] = 0;
#else                              /* use fcvt() */
   if (max > 310)
      max = 310;
# ifdef HAVE_FCVTL
   result = fcvtl(ufvalue, max, &dec_pt, &sig);
# else
   result = fcvt(ufvalue, max, &dec_pt, &sig);
# endif

   r_length = strlen(result);

   /*
    * Fix broken fcvt implementation returns..
    */

   if (r_length == 0) {
      result[0] = '0';
      result[1] = '\0';
      r_length = 1;
   }

   if (r_length < dec_pt)
      dec_pt = r_length;

   if (dec_pt <= 0) {
      iplace = 1;
      iconvert[0] = '0';
      iconvert[1] = '\0';

      fplace = 0;

      while (r_length)
         fconvert[fplace++] = result[--r_length];

      while ((dec_pt < 0) && (fplace < max)) {
         fconvert[fplace++] = '0';
         dec_pt++;
      }
   } else {
      int c;

      iplace = 0;
      for (c = dec_pt; c; iconvert[iplace++] = result[--c]);
      iconvert[iplace] = '\0';

      result += dec_pt;
      fplace = 0;

      for (c = (r_length - dec_pt); c; fconvert[fplace++] = result[--c]);
   }
#endif  /* HAVE_FCVT */

   /* -1 for decimal point, another -1 if we are printing a sign */
   padlen = min - iplace - max - 1 - ((signvalue) ? 1 : 0);
   zpadlen = max - fplace;
   if (zpadlen < 0) {
      zpadlen = 0;
   }
   if (padlen < 0) {
      padlen = 0;
   }
   if (flags & DP_F_MINUS) {
      padlen = -padlen;            /* Left Justifty */
   }

   if ((flags & DP_F_ZERO) && (padlen > 0)) {
      if (signvalue) {
         outch(signvalue);
         --padlen;
         signvalue = 0;
      }
      while (padlen > 0) {
         outch('0');
         --padlen;
      }
   }
   while (padlen > 0) {
      outch(' ');
      --padlen;
   }
   if (signvalue) {
      outch(signvalue);
   }

   while (iplace > 0) {
      outch(iconvert[--iplace]);
   }


#ifdef DEBUG_SNPRINTF
   printf("fmtfp: fplace=%d zpadlen=%d\n", fplace, zpadlen);
#endif

   /*
    * Decimal point.  This should probably use locale to find the correct
    * char to print out.
    */
   if (max > 0) {
      outch('.');
      while (fplace > 0) {
         outch(fconvert[--fplace]);
      }
   }

   while (zpadlen > 0) {
      outch('0');
      --zpadlen;
   }

   while (padlen < 0) {
      outch(' ');
      ++padlen;
   }
   return currlen;
}
#endif  /* FP_OUTPUT */


#ifdef TEST_PROGRAM

#ifndef LONG_STRING
#define LONG_STRING 1024
#endif
int main(void)
{
   char buf1[LONG_STRING];
   char buf2[LONG_STRING];

#ifdef FP_OUTPUT
   char *fp_fmt[] = {
      "%-1.5f",
      "%1.5f",
      "%123.9f",
      "%10.5f",
      "% 10.5f",
      "%+22.9f",
      "%+4.9f",
      "%01.3f",
      "%4f",
      "%3.1f",
      "%3.2f",
      "%.0f",
      "%.1f",
      NULL
   };
   double fp_nums[] = { -1.5, 134.21, 91340.2, 341.1234, 0203.9, 0.96, 0.996,
      0.9996, 1.996, 4.136, 6442452944.1234, 0
   };
#endif
   char *int_fmt[] = {
      "%-1.5d",
      "%1.5d",
      "%123.9d",
      "%5.5d",
      "%10.5d",
      "% 10.5d",
      "%+22.33d",
      "%01.3d",
      "%4d",
      "%-1.5ld",
      "%1.5ld",
      "%123.9ld",
      "%5.5ld",
      "%10.5ld",
      "% 10.5ld",
      "%+22.33ld",
      "%01.3ld",
      "%4ld",
      NULL
   };
   long int_nums[] = { -1, 134, 91340, 341, 0203, 0 };

   char *ll_fmt[] = {
      "%-1.8lld",
      "%1.8lld",
      "%123.9lld",
      "%5.8lld",
      "%10.5lld",
      "% 10.8lld",
      "%+22.33lld",
      "%01.3lld",
      "%4lld",
      NULL
   };
   int64_t ll_nums[] = { -1976, 789134567890LL, 91340, 34123, 0203, 0 };

   char *s_fmt[] = {
      "%-1.8s",
      "%1.8s",
      "%123.9s",
      "%5.8s",
      "%10.5s",
      "% 10.3s",
      "%+22.1s",
      "%01.3s",
      "%s",
      "%10s",
      "%3s",
      "%3.0s",
      "%3.s",
      NULL
   };
   char *s_nums[] = { "abc", "def", "ghi", "123", "4567", "a", "bb", "ccccccc", NULL};


   int x, y;
   int fail = 0;
   int num = 0;

   printf("Testing snprintf format codes against system sprintf...\n");

#ifdef FP_OUTPUT
   for (x = 0; fp_fmt[x] != NULL; x++)
      for (y = 0; fp_nums[y] != 0; y++) {
         bsnprintf(buf1, sizeof(buf1), fp_fmt[x], fp_nums[y]);
         sprintf(buf2, fp_fmt[x], fp_nums[y]);
         if (strcmp(buf1, buf2)) {
            printf
               ("snprintf doesn't match Format: %s\n\tsnprintf = %s\n\tsprintf  = %s\n",
                fp_fmt[x], buf1, buf2);
            fail++;
         }
         num++;
      }
#endif

   for (x = 0; int_fmt[x] != NULL; x++)
      for (y = 0; int_nums[y] != 0; y++) {
         int pcount, bcount;
         bcount = bsnprintf(buf1, sizeof(buf1), int_fmt[x], int_nums[y]);
         printf("%s\n", buf1);
         pcount = sprintf(buf2, int_fmt[x], int_nums[y]);
         if (bcount != pcount) {
            printf("bsnprintf count %d doesn't match sprintf count %d\n",
               bcount, pcount);
         }
         if (strcmp(buf1, buf2)) {
            printf
               ("bsnprintf doesn't match Format: %s\n\tsnprintf = %s\n\tsprintf  = %s\n",
                int_fmt[x], buf1, buf2);
            fail++;
         }
         num++;
      }

   for (x = 0; ll_fmt[x] != NULL; x++) {
      for (y = 0; ll_nums[y] != 0; y++) {
         int pcount, bcount;
         bcount = bsnprintf(buf1, sizeof(buf1), ll_fmt[x], ll_nums[y]);
         printf("%s\n", buf1);
         pcount = sprintf(buf2, ll_fmt[x], ll_nums[y]);
         if (bcount != pcount) {
            printf("bsnprintf count %d doesn't match sprintf count %d\n",
               bcount, pcount);
         }
         if (strcmp(buf1, buf2)) {
            printf
               ("bsnprintf doesn't match Format: %s\n\tsnprintf = %s\n\tsprintf  = %s\n",
                ll_fmt[x], buf1, buf2);
            fail++;
         }
         num++;
      }
   }

   for (x = 0; s_fmt[x] != NULL; x++) {
      for (y = 0; s_nums[y] != 0; y++) {
         int pcount, bcount;
         bcount = bsnprintf(buf1, sizeof(buf1), s_fmt[x], s_nums[y]);
         printf("%s\n", buf1);
         pcount = sprintf(buf2, s_fmt[x], s_nums[y]);
         if (bcount != pcount) {
            printf("bsnprintf count %d doesn't match sprintf count %d\n",
               bcount, pcount);
         }
         if (strcmp(buf1, buf2)) {
            printf
               ("bsnprintf doesn't match Format: %s\n\tsnprintf = %s\n\tsprintf  = %s\n",
                s_fmt[x], buf1, buf2);
            fail++;
         }
         num++;
      }
   }


   printf("%d tests failed out of %d.\n", fail, num);
}
#endif /* TEST_PROGRAM */

#endif /* USE_BSNPRINTF */
