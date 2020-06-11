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

// Standard C++ headers
#include <algorithm>
#include <functional>
#include <mutex>
#include <thread> // we run our testing threads using C++11 threads

// Google Test Framework (& related) C++ headers
#include <absl/synchronization/notification.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

// Standard C headers
#include <threads.h> // included for C11 thrd_start_t
#include <unistd.h>  // read(2) / write(2) prototypes

extern "C" {
#include "bob.h"
#include "common.h"
}

using namespace std;

// for mocking a "free function" (i.e. one in the default namespace
class ReadWriteInterface {
public:
  virtual ~ReadWriteInterface() {}
  virtual ssize_t read(int fd, void *buf, size_t count) = 0;
  virtual ssize_t write(int fd, const void *buf, size_t count) = 0;
};

class ReadWriteMock : public ReadWriteInterface {
public:
  MOCK_METHOD(ssize_t, read, (int, void *, size_t));
  MOCK_METHOD(ssize_t, write, (int, const void *, size_t));
};

// instance pointer, refreshed before each test is run
static ReadWriteMock *inst;
extern "C" {
// dependency injection for read(2) / write(2) "free functions"
// allows us to intercept I/O to/from Bob
ssize_t read(int fd, void *buf, size_t count) {
  return inst->read(fd, buf, count);
}
ssize_t write(int fd, const void *buf, size_t count) {
  return inst->write(fd, buf, count);
}
}

/*
 * Test fixture for testing Bob
 */
class Bob : public ::testing::Test {
public:
  Bob() = default;

protected:
  ReadWriteMock rwMock;

  void SetUp() override {
    // update the instance pointer prior to running tests
    inst = &rwMock;
  }

  void TearDown() override {}
};

// C++11 std::thread compatibity layer for C11 thread functions
static void c11ThreadTrampoline(::thrd_start_t fun, void *arg) {
  int r = fun(arg);
  (void)r;
}

/*
 * In this test, we override the default MOCK_METHOD() implementations of
 * read(2) and write(2) using EXPECT_CALL() followed by WillRepeatedly().
 *
 * In this case, we use a lambda function for each call to read(2) and write(2).
 *
 * Our test does not time-out because done.Notify() is called when Alice
 * receives the final token "dog".
 */
TEST_F(Bob, TheQuickBrownFox_WillRepeatedly) {

  using testing::_;
  mutex mu;
  size_t i = 0;
  size_t j = 1;
  const int mockFd = 42;

  absl::Notification done;

  EXPECT_CALL(rwMock, write(_, _, _))
      .WillRepeatedly(::testing::Invoke(
          [&](int fd, const void *buffer, size_t len) -> ssize_t {
            // lock for i, j, and readBuf
            lock_guard<mutex> lock(mu);

            EXPECT_EQ(fd, mockFd);
            if (mockFd != fd) {
              errno = EINVAL;
              return -1;
            }

            EXPECT_LT(i, N);
            if (i >= N) {
              errno = EINVAL;
              return -1;
            }

            string expected_string(s[i], s[i] + l[i]);
            string actual_string(static_cast<const char *>(buffer),
                                 static_cast<const char *>(buffer) + len);
            EXPECT_EQ(actual_string, expected_string);
            if (actual_string != expected_string) {
              errno = EINVAL;
              return -1;
            }

            cout << actual_string << ' ';

            // set up the next call to write(2) from Bob
            i += 2;

            if (i >= N) {
              cout << endl;
              done.Notify();
            }

            return len;
          }));

  EXPECT_CALL(rwMock, read(_, _, _))
      .WillRepeatedly(
          ::testing::Invoke([&](int fd, void *buffer, size_t len) -> ssize_t {
            // lock for i, j, and readBuf
            lock_guard<mutex> lock(mu);

            EXPECT_EQ(fd, mockFd);
            if (mockFd != fd) {
              errno = EINVAL;
              return -1;
            }

            EXPECT_LT(j, N);
            if (j >= N) {
              errno = EINVAL;
              return -1;
            }

            size_t copyLen = min(::strlen(s[j]), len);
            for (size_t k = 0; k < copyLen; ++k) {
              ((uint8_t *)buffer)[k] = s[j][k];
            }

            // set up the next expeted message from Bob
            j += 2;

            return copyLen;
          }));

  thread bobThread =
      thread(c11ThreadTrampoline, (::thrd_start_t)&bob, (void *)&mockFd);
  bobThread.detach();

  ASSERT_TRUE(done.WaitForNotificationWithTimeout(absl::Milliseconds(1000)));
}

/*
 * In this test, we override the default MOCK_METHOD() implementations of
 * read(2) and write(2) using EXPECT_CALL() followed by chained calls to
 * WillOnce().
 *
 * It's possible to Invoke() a separately defined function or really use any
 * code that matches the signatures of read(2) and write(2).
 *
 * In this case, we use a lambda that calls another lambda for the first call to
 * write(2), and in the rest of the cases, we simply bind function calls.
 *
 * Note the use of placeholder variables for (fd, buffer, len), while the final
 * variable specifies an index into the external arrays s[] and l[], which
 * contain each token and string length of the phrase "The quick brown fox jumps
 * over the lazy dog".
 *
 * Our test does not time-out because done.Notify() is called when Alice
 * receives the final token "dog".
 */
TEST_F(Bob, TheQuickBrownFox_WillOnce) {

  using ::testing::_;
  using namespace std::placeholders;
  mutex mu;
  const int mockFd = 42;

  absl::Notification done;

  auto writeCallback = [&](int fd, const void *buffer, size_t len,
                           size_t i) -> ssize_t {
    lock_guard<mutex> lock(mu);

    EXPECT_EQ(fd, mockFd);
    if (mockFd != fd) {
      errno = EINVAL;
      return -1;
    }

    EXPECT_LT(i, N);
    if (i >= N) {
      errno = EINVAL;
      return -1;
    }

    string expected_string(s[i], s[i] + l[i]);
    string actual_string(static_cast<const char *>(buffer),
                         static_cast<const char *>(buffer) + len);
    EXPECT_EQ(actual_string, expected_string);
    if (actual_string != expected_string) {
      errno = EINVAL;
      return -1;
    }

    cout << actual_string << ' ';

    // set up the next call to write(2) from Bob
    i += 2;

    if (i >= N) {
      cout << endl;
      done.Notify();
    }

    return len;
  };

  auto readCallback = [&](int fd, void *buffer, size_t len,
                          size_t j) -> ssize_t {
    // lock for i, j, and readBuf
    lock_guard<mutex> lock(mu);

    EXPECT_EQ(fd, mockFd);
    if (mockFd != fd) {
      errno = EINVAL;
      return -1;
    }

    EXPECT_LT(j, N);
    if (j >= N) {
      errno = EINVAL;
      return -1;
    }

    size_t copyLen = min(::strlen(s[j]), len);
    for (size_t k = 0; k < copyLen; ++k) {
      ((uint8_t *)buffer)[k] = s[j][k];
    }

    // set up the next expeted message from Bob
    j += 2;

    return copyLen;
  };

  EXPECT_CALL(rwMock, write(_, _, _))
      .WillOnce(::testing::Invoke(
          [&](int fd, const void *buffer, size_t len) -> ssize_t {
            /* This can be a completely unique function body and this is here
             * just to illustrate that. In this particular case, each of the
             * callbacks has a very similar function and only differs by the
             * final argument to writeCallback() which is an index to the global
             * s[] array.
             *
             * Below, we simply use the std::bind() shorthand to bind the
             * argument to Invoke() to writeCallback(), using placeholder
             * variables _1, _2, and _3, along with the array index for s[]
             * and l[].
             */
            return writeCallback(fd, buffer, len, 0);
          }))
      .WillOnce(::testing::Invoke(bind(writeCallback, _1, _2, _3, 2)))
      .WillOnce(::testing::Invoke(bind(writeCallback, _1, _2, _3, 4)))
      .WillOnce(::testing::Invoke(bind(writeCallback, _1, _2, _3, 6)))
      .WillOnce(::testing::Invoke(bind(writeCallback, _1, _2, _3, 8)));

  EXPECT_CALL(rwMock, read(_, _, _))
      .WillOnce(::testing::Invoke(bind(readCallback, _1, _2, _3, 1)))
      .WillOnce(::testing::Invoke(bind(readCallback, _1, _2, _3, 3)))
      .WillOnce(::testing::Invoke(bind(readCallback, _1, _2, _3, 5)))
      .WillOnce(::testing::Invoke(bind(readCallback, _1, _2, _3, 7)));

  thread bobThread =
      thread(c11ThreadTrampoline, (::thrd_start_t)&bob, (void *)&mockFd);
  bobThread.detach();

  ASSERT_TRUE(done.WaitForNotificationWithTimeout(absl::Milliseconds(1000)));
}

/*
 * In this test, we override the default MOCK_METHOD() implementations of
 * read(2) and write(2) to make them block indefinitely, which one sure way to
 * induce a timeout.
 *
 * Our test times-out because done.Notify() is not called.
 */
TEST_F(Bob, TheQuickBrownFox_timeout) {

  using ::testing::_;
  using namespace std::placeholders;

  const int mockFd = 42;

  absl::Notification done;

  ON_CALL(rwMock, write(_, _, _))
      .WillByDefault(::testing::Invoke(
          [](int fd, const void *buffer, size_t len) -> ssize_t {
            (void)fd;
            (void)buffer;
            (void)len;
            // loop forever, causing a timeout
            for (;;)
              ;
            return 0;
          }));

  ON_CALL(rwMock, read(_, _, _))
      .WillByDefault(::testing::Invoke(
          [](int fd, const void *buffer, size_t len) -> ssize_t {
            (void)fd;
            (void)buffer;
            (void)len;
            // loop forever, causing a timeout
            for (;;)
              ;
            return 0;
          }));

  thread bobThread =
      thread(c11ThreadTrampoline, (::thrd_start_t)&bob, (void *)&mockFd);
  bobThread.detach();

  bool timeoutOccurred =
      !done.WaitForNotificationWithTimeout(absl::Milliseconds(1000));

  if (timeoutOccurred) {
    cout << "A timeout occurred" << endl;
  }

  ASSERT_TRUE(timeoutOccurred);
}

/*
 * In this test, we do rely on the default MOCK_METHOD() implementations for
 * read(2) and write(2) which simply return 0.
 *
 * Our test times-out because done.Notify() is not called.
 */
TEST_F(Bob, TheQuickBrownFox_defaultImplTimeout) {

  int mockFd = 42;
  absl::Notification done;

  thread bobThread =
      thread(c11ThreadTrampoline, (::thrd_start_t)&bob, (void *)&mockFd);
  bobThread.detach();

  bool timeoutOccurred =
      !done.WaitForNotificationWithTimeout(absl::Milliseconds(1000));

  if (timeoutOccurred) {
    cout << "A timeout occurred" << endl;
  }

  ASSERT_TRUE(timeoutOccurred);
}
