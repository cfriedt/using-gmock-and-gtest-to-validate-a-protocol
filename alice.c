/*
 * MIT License
 *
 * Copyright (c) 2020, Friedt Professional Engineering Services, Inc
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "common.h"

int alice(int *fd) {

  char buf[8];
  int r;

  for (size_t i = 0; i < N; ++i) {
    // alice will read even numbered messages and write odd-numbered messages
    if (0 == i % 2) {
      memset(buf, '\0', sizeof(buf));
      r = read(*fd, buf, sizeof(buf));
      if (-1 == r || 0 != strncmp(buf, s[i], min(l[i], sizeof(buf)))) {
        // just give up on failure
        break;
      }
      printf("%s ", s[i]);
    } else {
      r = write(*fd, s[i], l[i]);
      if ((int)l[i] != r) {
        // just give up on failure
        break;
      }
    }
  }

  close(*fd);

  return 0;
}
