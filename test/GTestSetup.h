#ifndef GTestSetup_H
#define GTestSetup_H

#include <gtest/gtest.h>

namespace testing { namespace internal {
  enum GTestColor {COLOR_DEFAULT, COLOR_RED, COLOR_GREEN, COLOR_YELLOW};
  extern void ColoredPrintf(GTestColor color, const char* fmt, ...);
}}

struct LimitedEventListener : public testing::TestEventListener {
  static int maxmsg;
  LimitedEventListener (testing::TestEventListener* defaultListener_=nullptr)
    : defaultListener(defaultListener_) {}

  void OnTestStart(const testing::TestInfo& test_info) override {
    nfail = 0;
    testing::internal::ColoredPrintf(testing::internal::COLOR_GREEN,  "[ RUN      ] ");
    printf("%s.%s", test_info.test_case_name(), test_info.name());
    if (maxmsg>=0) printf(" (stopping after %d failures)", maxmsg);
    printf("\n");
    fflush(stdout);
  }

  // Called after a failed assertion or a SUCCESS().
  void OnTestPartResult(const testing::TestPartResult& test_part_result) override {
    if (test_part_result.nonfatally_failed()) {
      if (maxmsg >= 0 && nfail >= maxmsg) {
        if (nfail == maxmsg) {
          testing::internal::ColoredPrintf(testing::internal::COLOR_RED, "[  FAILED  ] ");
          printf ("stop printing after %d messages\n", nfail);
          fflush(stdout);
        }
        nfail++;
        throw std::runtime_error("too many non-fatal failures");
        return;
      }
    }
    nfail++;
    if (defaultListener) defaultListener->OnTestPartResult (test_part_result);
  }

  // The following methods call the default TestEventListener to print their stuff.
  void OnTestProgramStart          (const testing::UnitTest& unit_test)                override { if (defaultListener) defaultListener->OnTestProgramStart          (unit_test           ); }
  void OnTestIterationStart        (const testing::UnitTest& unit_test, int iteration) override { if (defaultListener) defaultListener->OnTestIterationStart        (unit_test, iteration); }
  void OnEnvironmentsSetUpStart    (const testing::UnitTest& unit_test)                override { if (defaultListener) defaultListener->OnEnvironmentsSetUpStart    (unit_test           ); }
  void OnEnvironmentsSetUpEnd      (const testing::UnitTest& unit_test)                override { if (defaultListener) defaultListener->OnEnvironmentsSetUpEnd      (unit_test           ); }
  void OnTestCaseStart             (const testing::TestCase& test_case)                override { if (defaultListener) defaultListener->OnTestCaseStart             (test_case           ); }
//void OnTestStart                 (const testing::TestInfo& test_info)                override { if (defaultListener) defaultListener->OnTestStart                 (test_info           ); }
//void OnTestPartResult            (const testing::TestPartResult& result)             override { if (defaultListener) defaultListener->OnTestPartResult            (result              ); }
  void OnTestEnd                   (const testing::TestInfo& test_info)                override { if (defaultListener) defaultListener->OnTestEnd                   (test_info           ); }
  void OnTestCaseEnd               (const testing::TestCase& test_case)                override { if (defaultListener) defaultListener->OnTestCaseEnd               (test_case           ); }
  void OnEnvironmentsTearDownStart (const testing::UnitTest& unit_test)                override { if (defaultListener) defaultListener->OnEnvironmentsTearDownStart (unit_test           ); }
  void OnEnvironmentsTearDownEnd   (const testing::UnitTest& unit_test)                override { if (defaultListener) defaultListener->OnEnvironmentsTearDownEnd   (unit_test           ); }
  void OnTestIterationEnd          (const testing::UnitTest& unit_test, int iteration) override { if (defaultListener) defaultListener->OnTestIterationEnd          (unit_test, iteration); }
  void OnTestProgramEnd            (const testing::UnitTest& unit_test)                override { if (defaultListener) defaultListener->OnTestProgramEnd            (unit_test           ); }

private:
  int nfail = 0;
  std::unique_ptr<testing::TestEventListener> defaultListener;
};


class MyEnvironment : public ::testing::Environment {
protected:
  void SetUp() override {
    printf ("replace default_result_printer with our LimitedEventListener\n");
    fflush(stdout);
    ::testing::TestEventListeners& listeners = testing::UnitTest::GetInstance()->listeners();
    ::testing::TestEventListener* default_listener = listeners.Release (listeners.default_result_printer());
    listeners.Append (new LimitedEventListener (default_listener));
  }
};

const ::testing::Environment* _AddMyEnvironment = ::testing::AddGlobalTestEnvironment(new MyEnvironment);

#endif /* GTestSetup_H */
