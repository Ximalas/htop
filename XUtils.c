/*
htop - StringUtils.c
(C) 2004-2011 Hisham H. Muhammad
Released under the GNU GPLv2+, see the COPYING file
in the source distribution for its full text.
*/

#include "config.h" // IWYU pragma: keep

#include "XUtils.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "CRT.h"


void fail() {
   CRT_done();
   abort();

   _exit(1); // Should never reach here
}

extern FILE* debug_log;

void* xMalloc(size_t size, const char* fun, const char* file, int line) {
   struct timeval t;
   assert(size > 0);
   void* data = malloc(size);
   if (!data) {
      fail();
   }
   gettimeofday(&t, NULL);
   fprintf(debug_log, "%p %ld.%06ld allocated %lu bytes %s():%s:%d\n", data, t.tv_sec, t.tv_usec, size, fun, file, line);
   return data;
}

void* xMallocArray(size_t nmemb, size_t size, const char* fun, const char* file, int line) {
   assert(nmemb > 0);
   assert(size > 0);
   if (SIZE_MAX / nmemb < size) {
      fail();
   }
   return xMalloc(nmemb * size, fun, file, line);
}

void* xCalloc(size_t nmemb, size_t size, const char* fun, const char* file, int line) {
   struct timeval t;
   assert(nmemb > 0);
   assert(size > 0);
   if (SIZE_MAX / nmemb < size) {
      fail();
   }
   void* data = calloc(nmemb, size);
   if (!data) {
      fail();
   }
   gettimeofday(&t, NULL);
   fprintf(debug_log, "%p %ld.%06ld allocated and zeroised %lu bytes %s():%s:%d\n", data, t.tv_sec, t.tv_usec, nmemb * size, fun, file, line);
   return data;
}

void* xRealloc(void* ptr, size_t size, const char* fun, const char* file, int line) {
   struct timeval t;
   assert(size > 0);
   void* data = realloc(ptr, size); // deepcode ignore MemoryLeakOnRealloc: this goes to fail()
   if (!data) {
      xFree(ptr, __func__, __FILE__, __LINE__);
      fail();
   }
   gettimeofday(&t, NULL);
   fprintf(debug_log, "%p %ld.%06ld reallocated %lu bytes %s():%s:%d\n", data, t.tv_sec, t.tv_usec, size, fun, file, line);
   return data;
}

void* xReallocArray(void* ptr, size_t nmemb, size_t size, const char* fun, const char* file, int line) {
   assert(nmemb > 0);
   assert(size > 0);
   if (SIZE_MAX / nmemb < size) {
      fail();
   }
   return xRealloc(ptr, nmemb * size, fun, file, line);
}

void* xReallocArrayZero(void* ptr, size_t prevmemb, size_t newmemb, size_t size, const char* fun, const char* file, int line) {
   assert((ptr == NULL) == (prevmemb == 0));

   if (prevmemb == newmemb) {
      return ptr;
   }

   void* ret = xReallocArray(ptr, newmemb, size, fun, file, line);

   if (newmemb > prevmemb) {
      memset((unsigned char*)ret + prevmemb * size, '\0', (newmemb - prevmemb) * size);
   }

   return ret;
}

void xFree(void* ptr, const char* fun, const char* file, int line) {
   struct timeval t;
   gettimeofday(&t, NULL);
   fprintf(debug_log, "%p %ld.%06ld deallocated %s():%s:%d\n", ptr, t.tv_sec, t.tv_usec, fun, file, line);
   free(ptr); // This should be the only call to vanilla free() in the entire program.
}

inline bool String_contains_i(const char* s1, const char* s2) {
   return strcasestr(s1, s2) != NULL;
}

char* String_cat(const char* s1, const char* s2, const char* fun, const char* file, int line) {
   const size_t l1 = strlen(s1);
   const size_t l2 = strlen(s2);
   char* out = xMalloc(l1 + l2 + 1, fun, file, line);
   memcpy(out, s1, l1);
   memcpy(out + l1, s2, l2);
   out[l1 + l2] = '\0';
   return out;
}

char* String_trim(const char* in, const char* fun, const char* file, int line) {
   while (in[0] == ' ' || in[0] == '\t' || in[0] == '\n') {
      in++;
   }

   size_t len = strlen(in);
   while (len > 0 && (in[len - 1] == ' ' || in[len - 1] == '\t' || in[len - 1] == '\n')) {
      len--;
   }

   return xStrndup(in, len, fun, file, line);
}

char** String_split(const char* s, char sep, size_t* n, const char* fun, const char* file, int line) {
   const unsigned int rate = 10;
   char** out = xCalloc(rate, sizeof(char*), fun, file, line);
   size_t ctr = 0;
   unsigned int blocks = rate;
   const char* where;
   while ((where = strchr(s, sep)) != NULL) {
      size_t size = (size_t)(where - s);
      out[ctr] = xStrndup(s, size, fun, file, line);
      ctr++;
      if (ctr == blocks) {
         blocks += rate;
         out = (char**) xRealloc(out, sizeof(char*) * blocks, fun, file, line);
      }
      s += size + 1;
   }
   if (s[0] != '\0') {
      out[ctr] = xStrdup(s, fun, file, line);
      ctr++;
   }
   out = xRealloc(out, sizeof(char*) * (ctr + 1), fun, file, line);
   out[ctr] = NULL;

   if (n)
      *n = ctr;

   return out;
}

void String_freeArray(char** s, const char* fun, const char* file, int line) {
   if (!s) {
      return;
   }
   for (size_t i = 0; s[i] != NULL; i++) {
      xFree(s[i], fun, file, line);
   }
   xFree(s, fun, file, line);
}

char* String_getToken(const char* line, const unsigned short int numMatch, const char* fun, const char* file, int sline) {
   const size_t len = strlen(line);
   char inWord = 0;
   unsigned short int count = 0;
   char match[50];

   size_t foundCount = 0;

   for (size_t i = 0; i < len; i++) {
      char lastState = inWord;
      inWord = line[i] == ' ' ? 0 : 1;

      if (lastState == 0 && inWord == 1)
         count++;

      if (inWord == 1) {
         if (count == numMatch && line[i] != ' ' && line[i] != '\0' && line[i] != '\n' && line[i] != (char)EOF) {
            match[foundCount] = line[i];
            foundCount++;
         }
      }
   }

   match[foundCount] = '\0';
   return xStrdup(match, fun, file, sline);
}

char* String_readLine(FILE* fd, const char* fun, const char* file, int line) {
   const unsigned int step = 1024;
   unsigned int bufSize = step;
   char* buffer = xMalloc(step + 1, fun, file, line);
   char* at = buffer;
   for (;;) {
      const char* ok = fgets(at, step + 1, fd);
      if (!ok) {
         xFree(buffer, fun, file, line);
         return NULL;
      }
      char* newLine = strrchr(at, '\n');
      if (newLine) {
         *newLine = '\0';
         return buffer;
      } else {
         if (feof(fd)) {
            return buffer;
         }
      }
      bufSize += step;
      buffer = xRealloc(buffer, bufSize + 1, fun, file, line);
      at = buffer + bufSize - step;
   }
}

size_t String_safeStrncpy(char* restrict dest, const char* restrict src, size_t size) {
   assert(size > 0);

   size_t i = 0;
   for (; i < size - 1 && src[i]; i++)
      dest[i] = src[i];

   dest[i] = '\0';

   return i;
}

int xAsprintf(char** strp, const char* fmt, ...) {
   va_list vl;
   va_start(vl, fmt);
   int r = vasprintf(strp, fmt, vl);
   va_end(vl);

   if (r < 0 || !*strp) {
      fail();
   }

   return r;
}

int xSnprintf(char* buf, size_t len, const char* fmt, ...) {
   va_list vl;
   va_start(vl, fmt);
   int n = vsnprintf(buf, len, fmt, vl);
   va_end(vl);

   if (n < 0 || (size_t)n >= len) {
      fail();
   }

   return n;
}

char* xStrdup(const char* str, const char* fun, const char* file, int line) {
   struct timeval t;
   char* data = strdup(str);
   if (!data) {
      fail();
   }
   gettimeofday(&t, NULL);
   fprintf(debug_log, "%p %ld.%06ld duplicated %lu bytes %s():%s:%d\n", data, t.tv_sec, t.tv_usec, strlen(str) + 1, fun, file, line);
   return data;
}

void free_and_xStrdup(char** ptr, const char* str, const char* fun, const char* file, int line) {
   if (*ptr && String_eq(*ptr, str))
      return;

   xFree(*ptr, fun, file, line);
   *ptr = xStrdup(str, fun, file, line);
}

char* xStrndup(const char* str, size_t len, const char* fun, const char* file, int line) {
   struct timeval t;
   char* data = strndup(str, len);
   if (!data) {
      fail();
   }
   gettimeofday(&t, NULL);
   fprintf(debug_log, "%p %ld.%06ld duplicated %lu bytes %s():%s:%d\n", data, t.tv_sec, t.tv_usec, len + 1, fun, file, line);
   return data;
}

static ssize_t readfd_internal(int fd, void* buffer, size_t count) {
   if (!count) {
      close(fd);
      return -EINVAL;
   }

   ssize_t alreadyRead = 0;
   count--; // reserve one for null-terminator

   for (;;) {
      ssize_t res = read(fd, buffer, count);
      if (res == -1) {
         if (errno == EINTR)
            continue;

         close(fd);
         return -errno;
      }

      if (res > 0) {
         buffer = ((char*)buffer) + res;
         count -= (size_t)res;
         alreadyRead += res;
      }

      if (count == 0 || res == 0) {
         close(fd);
         *((char*)buffer) = '\0';
         return alreadyRead;
      }
   }
}

ssize_t xReadfile(const char* pathname, void* buffer, size_t count) {
   int fd = open(pathname, O_RDONLY);
   if (fd < 0)
      return -errno;

   return readfd_internal(fd, buffer, count);
}

ssize_t xReadfileat(openat_arg_t dirfd, const char* pathname, void* buffer, size_t count) {
   int fd = Compat_openat(dirfd, pathname, O_RDONLY);
   if (fd < 0)
      return -errno;

   return readfd_internal(fd, buffer, count);
}
