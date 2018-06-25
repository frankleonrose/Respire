#include <Arduino.h>
#include <unity.h>
#include <map>
#include <vector>
#include <Logging.h>

#include "respire.h"

#include "respire.cpp"

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
  bool value_bool = true;
  int  value_int = 0;

  public:
  virtual void onChange(const AppState &oldState, Executor<AppState> *executor) {}
  virtual void dump(const Mode<AppState> &mainMode) const {}

  int valueInt() {
    return value_int;
  }

  void valueInt(int v) {
    AppState oldState(*this);
    value_int = v;
    onUpdate(oldState);
  }

  bool valueBool() {
    return value_bool;
  }

  void valueBool(bool v) {
    AppState oldState(*this);
    value_bool = v;
    onUpdate(oldState);
  }
};

void testFunction(const AppState &state, const AppState &oldState, Mode<AppState> *triggeringMode) {
}

#define FNAME(fn) {fn, #fn}
static struct {
  Mode<AppState>::ActionFn fn;
  const char *name;
} _names[] = {
  FNAME(testFunction),
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

class TestStore : public RespireStore {
  std::map<std::string, uint32_t> _intMap;

  public:

  virtual bool load(const char *name, uint8_t *bytes, const uint16_t size) {
    return false;
  }
  virtual bool load(const char *name, uint32_t *value) {
    if (_intMap.find(std::string(name))==_intMap.end()) {
      return false;
    }
    *value = _intMap.at(std::string(name));
    Log.Debug("load \"%8s\": %d\n", name, *value);
    return true;
  }

  virtual bool store(const char *name, const uint8_t *bytes, const uint16_t size) {
    Log.Debug("store \"%8s\": %*m\n", name, size, bytes);
    return true;
  }
  virtual bool store(const char *name, const uint32_t value) {
    Log.Debug("store \"%8s\": %d\n", name, value);
    _intMap[std::string(name)] = value;
    return true;
  }

  bool confirm(const char *name, const uint32_t value) const {
    return _intMap.at(std::string(name)) == value;
  }
};

void test_state_copy_ctor(void) {
  AppState state;
  state.valueInt(5);
  state.valueBool(true);

  AppState newState(state);
  TEST_ASSERT_EQUAL(state.valueInt(), newState.valueInt());
  TEST_ASSERT_EQUAL(state.valueBool(), newState.valueBool());
}

void test_parent_terminated_when_limited_idle(void) {
  TestStore store;
  TestClock clock;
  TestExecutor expectedOps(testFunction, NULL);
  Mode<AppState> ModeIdle(Mode<AppState>::Builder("idle")
    .invokeFn(testFunction)
    .repeatLimit(2));
  Mode<AppState> ModeTerminated(Mode<AppState>::Builder("to be terminated")
    .addChild(&ModeIdle)
    .idleMode(&ModeIdle));
  Mode<AppState> ModeRoot(Mode<AppState>::Builder("root")
    .addChild(&ModeTerminated));
  AppState state;
  RespireContext<AppState> respire(state, ModeRoot, &clock, &expectedOps);

  respire.init();
  respire.begin();
  ModeRoot.dump(state);

  clock.epochRtc(500000000);

  clock.advanceSeconds(1);
  respire.loop();
  TEST_ASSERT(expectedOps.check());

  respire.complete(&ModeIdle); // Indicate invoked function has completed

  TestExecutor expectedOneOps(testFunction, NULL);
  respire.setExecutor(&expectedOneOps);

  clock.advanceSeconds(1);
  respire.loop();

  TEST_ASSERT(expectedOneOps.check()); // Nothing more should have happened

  respire.complete(&ModeIdle); // Indicate invoked function has completed

  TEST_ASSERT_FALSE(ModeTerminated.isActive(state));
  TEST_ASSERT_FALSE(ModeIdle.isActive(state));
}

void test_storing_last_trigger(void) {
  TestStore store;
  TestClock clock;
  TestExecutor expectedOps(testFunction, NULL);
  Mode<AppState> ModeFunction(Mode<AppState>::Builder("function")
    .invokeFn(testFunction));
  Mode<AppState> ModePeriodic(Mode<AppState>::Builder("periodic")
    .storageTag("mytag")
    .periodic(60, TimeUnitHour)
    .addChild(&ModeFunction));
  AppState state;
  RespireContext<AppState> respire(state, ModePeriodic, &clock, &expectedOps);

  respire.init();
  respire.begin();

  clock.epochRtc(500000000);

  for (int i=0; i<90; ++i) {
    clock.advanceSeconds(1);
    respire.loop();
  }
  TEST_ASSERT(expectedOps.check());
  respire.checkpoint(state, store);

  TEST_ASSERT(store.confirm("RmytagLT", 500000060));

  for (int i=0; i<45; ++i) {
    clock.advanceSeconds(1);
    respire.loop();
  }
  respire.checkpoint(state, store);

  TEST_ASSERT(store.confirm("RmytagLT", 500000120));
}

void test_storing_cumulative_wait(void) {
  TestStore store;
  TestClock clock;
  TestExecutor expectedOps(testFunction, NULL);
  Mode<AppState> ModeFunction(Mode<AppState>::Builder("function")
    .invokeFn(testFunction));
  Mode<AppState> ModePeriodic(Mode<AppState>::Builder("periodic")
    .storageTag("mytag")
    .periodic(60, TimeUnitHour)
    .addChild(&ModeFunction));
  AppState state;
  RespireContext<AppState> respire(state, ModePeriodic, &clock, &expectedOps);

  respire.init();
  respire.begin();

  clock.epochRtc(500000000);

  for (int i=0; i<90; ++i) {
    clock.advanceSeconds(1);
    respire.loop();
  }
  TEST_ASSERT(expectedOps.check());
  respire.checkpoint(state, store);

  TEST_ASSERT(store.confirm("RmytagCW", 30));

  for (int i=0; i<45; ++i) {
    clock.advanceSeconds(1);
    respire.loop();
  }
  respire.checkpoint(state, store);

  TEST_ASSERT(store.confirm("RmytagCW", 15));
}

void test_restoring_cumulative_wait(void) {
  TestStore store;
  TestClock clock;
  TestExecutor expectedOps(testFunction, testFunction, NULL);
  Mode<AppState> ModeFunction(Mode<AppState>::Builder("function")
    .invokeFn(testFunction));
  Mode<AppState> ModePeriodic(Mode<AppState>::Builder("periodic")
    .storageTag("mytag")
    .periodic(60, TimeUnitHour)
    .addChild(&ModeFunction));
  AppState state;
  RespireContext<AppState> respire(state, ModePeriodic, &clock, &expectedOps);

  respire.init();
  respire.begin();

  clock.epochRtc(500000000);

  for (int i=0; i<75; ++i) {
    clock.advanceSeconds(1);
    respire.loop();
    if (ModeFunction.isActive(state)) {
      respire.complete(ModeFunction);
    }
  }
  TEST_ASSERT(expectedOps.check());
  respire.checkpoint(state, store);

  TEST_ASSERT(store.confirm("RmytagCW", 15));

  for (int i=0; i<45; ++i) {
    clock.advanceSeconds(1);
    respire.loop();
    if (ModeFunction.isActive(state)) {
      respire.complete(ModeFunction);
    }
  }
  // Don't checkpoint, so CW is still 15

  TestExecutor expectedNoOps(NULL);
  RespireContext<AppState> restored(state, ModePeriodic, &clock, &expectedNoOps);

  restored.init(0, &store);
  restored.begin();
  // ModePeriodic.dump(state);

  // random(period = 60000) returns 30000ms in test, so we know next trigger is 15 seconds from now
  for (int i=0; i<10; ++i) {
    clock.advanceSeconds(1);
    restored.loop();
    if (ModeFunction.isActive(state)) {
      restored.complete(ModeFunction);
    }
  }
  TEST_ASSERT(expectedNoOps.check());
  restored.checkpoint(state, store);

  TEST_ASSERT(store.confirm("RmytagCW", 25));

  TestExecutor expectedOneOps(testFunction, NULL);
  restored.setExecutor(&expectedOneOps);

  Log.Debug("Expecting to trigger testFunction\n");

  for (int i=0; i<10; ++i) {
    clock.advanceSeconds(1);
    restored.loop();
    if (ModeFunction.isActive(state)) {
      restored.complete(ModeFunction);
    }
    // ModePeriodic.dump(state);
  }
  TEST_ASSERT(expectedOneOps.check());
  restored.checkpoint(state, store);

  TEST_ASSERT(store.confirm("RmytagCW", 5));
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    LogPrinter printer(printFn);
    Log.Init(LOGLEVEL, printer);

    RUN_TEST(test_state_copy_ctor);

    RUN_TEST(test_parent_terminated_when_limited_idle);

    RUN_TEST(test_storing_last_trigger);
    RUN_TEST(test_storing_cumulative_wait);
    RUN_TEST(test_restoring_cumulative_wait);
    UNITY_END();

    return 0;
}

void invokeFunctionExample(const AppState &state, const AppState &oldState, Mode<AppState> *triggeringMode) {
}

#endif
