#include <Arduino.h>
#include <unity.h>
#include <vector>
#include <Logging.h>

#include "respire.h"

#define UNIT_TEST
#ifdef UNIT_TEST

void printFn(const char c) {
  printf("%c", c);
}

// void setUp(void) {
// // set stuff up here
// }

// void tearDown(void) {
// // clean stuff up here
// }

class TestClock : public Clock {
  uint32_t _millis = 100000;

  public:
  virtual uint32_t millis() {
    return _millis;
  }

  void advanceSeconds(uint32_t s) {
    _millis += 1000 * s;
  }
};

class AppState : public RespireState<AppState> {
  public:
  virtual void onChange(const AppState &oldState, Executor<AppState> *executor) {}
  virtual void dump(const Mode<AppState> &mainMode) const {}
};

#define FNAME(fn) {fn, #fn}
static struct {
  Mode<AppState>::ActionFn fn;
  const char *name;
} _names[] = {
};

class TestExecutor : public Executor<AppState> {
  std::vector<Mode<AppState>::ActionFn> _expected;
  std::vector<Mode<AppState>::ActionFn> _called;
  public:
  TestExecutor(Mode<AppState>::ActionFn expected, ...) {
    va_list args;
    va_start(args, expected);
    while (expected!=NULL) {
      Log.Debug(F("Expect: %p %s\n"), expected, name(expected));
      _expected.push_back(expected);
      expected = va_arg(args, Mode<AppState>::ActionFn);
    }
    va_end(args);
  }

  TestExecutor(Mode<AppState>::ActionFn expected, va_list &args) {
    while (expected!=NULL) {
      Log.Debug("Expect: %p %s\n", expected, name(expected));
      _expected.push_back(expected);
      expected = va_arg(args, Mode<AppState>::ActionFn);
    }
  }

  virtual void exec(Mode<AppState>::ActionFn listener, const AppState &state, const AppState &oldState, Mode<AppState> *trigger) {
    Log.Debug("Exec: %p %s\n", listener, name(listener));
    _called.push_back(listener);
  }

  bool check() {
    // TEST_ASSERT_EQUAL(_expected.size(), _called.size());
    if (_expected != _called) {
      Log.Debug("Checking expected: ");
      for (auto pfn = _expected.begin(); pfn!=_expected.end(); ++pfn) {
        Log.Debug_("%s, ", name(*pfn));
      }
      Log.Debug("\n......vs executed: ");
      for (auto pfn = _called.begin(); pfn!=_called.end(); ++pfn) {
        Log.Debug_("%s, ", name(*pfn));
      }
      Log.Debug("\n");
    }
    return _expected == _called;
  }

  const char *name(Mode<AppState>::ActionFn fn) {
    for (uint16_t i=0; i<ELEMENTS(_names); ++i) {
      if (fn==_names[i].fn) {
        return _names[i].name;
      }
    }
    return "unknown";
  }
};

void test_storing_last_trigger(void) {
  TestClock clock;
  TestExecutor expectedOps(NULL);
  Mode<AppState> ModePeriodic(Mode<AppState>::Builder("periodic"));
  AppState state;
  RespireContext<AppState> respire(state, ModePeriodic, &clock, &expectedOps);

  respire.init();
  respire.begin();

  TEST_ASSERT(expectedOps.check());
}

void test_storing_cumulative_wait(void) {
  TestClock clock;
  TestExecutor expectedOps(NULL);
  Mode<AppState> ModePeriodic(Mode<AppState>::Builder("periodic"));
  AppState state;
  RespireContext<AppState> respire(state, ModePeriodic, &clock, &expectedOps);

  respire.init();
  respire.begin();

  TEST_ASSERT(expectedOps.check());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    LogPrinter printer(printFn);
    Log.Init(LOGLEVEL, printer);

    RUN_TEST(test_storing_last_trigger);
    RUN_TEST(test_storing_cumulative_wait);
    UNITY_END();

    return 0;
}

void invokeFunctionExample(const AppState &state, const AppState &oldState, Mode<AppState> *triggeringMode) {
}

#endif
