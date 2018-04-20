[![Build Status](https://travis-ci.org/frankleonrose/Respire.svg?branch=master)](https://travis-ci.org/frankleonrose/Respire)

# Respire

Respire is an experimental framework used to represent application state and modes of operation in an embedded system.

Is it possible that this is *not* an interesting, composable, general purpose system? Yes. But you have to try things to know.

## Discussion

### Design Goals
- Concise declarative expression of modes of operation from which the system can generate functional diagrams, test code, and simulation code, as well as runtime code.
- Modularized focus on input, state update, and output
- Explicit support for regular periodic activities and issues surrounding them
- Implicit support for a wide range of common behaviors, like sleep and changing power modes.
- Performance (though, TBH, I haven't written any performance tests, so, really? But it works well enough in real hardware thus far.)

### Modes
Modes are the units of behavior in Respire.
- Modes are in either an Active state or an Inactive state. Inactive modes may be Inspired and become Active. Active modes may expire or terminate, after which they are Inactive.
- Modes are contained within a parent node.
- Modes can be active only when their parent is also active.
- Modes may have multiple parents, in which case they are considered contained by each of the parents with respect to activation.
- Modes are inspired when **all** of
  - The mode is inactive
  - The parent is active
  - The mode's required predicate is true
  - The mode's repeat limit has not been reached
  - The parent's child activations limit has not been reached
  - The parent's child simultaneous limit has not been reached
  - Any of the following is true
    - The parent was just inspired
    - The parent which is periodic was just triggered
    - The mode's inspiration predicate is true
    - The mode's predecessor just expired
    - The mode is the parent's idle mode and all its peers are inactive
- Modes expire when and one or more of the following is true:
  - The parent expires
  - The mode's required predicate is false
  - The mode contains no active modes AND none of the following is true (keepalive criteria):
    - The mode's minimum active time is not yet reached
    - The mode has an outstanding invocation (function execution)
    - The mode is periodic and has not reached its childActivationLimit
    - The mode is still waiting for the invoke delay to pass before executing a function.

#### Properties
Every Mode has a set of properties that define its behavior in a system.
- **name**: The name of the mode used in debugging. Not significant internally.
- **repeatLimit**: How many times may the mode be inspired
- **minDuration**: Once inspired, the mode will remain active for at least this length of time.
- **maxDuration**: Once inspired, the mode will remain active for no longer than this amount of time.
- **minGapDuration**: Once inspired (and expired), the mode will not be inspired again until this gap duration has passed. Useful for making something happen just once a day, for instance.
- **perTimes** & **perUnit**: Periodic modes will inspire their children perTimes per perUnit. Like, 10 times per Hour.
- **childActivationLimit**: For each inspiration, how many child inspirations will the mode perform. Useful for limiting a periodic mode.
- **childSimultaneousLimit**: How many children of this mode may be activated at once. Useful to guarantee that only one thing happens of a set. Children are inspired in the order that they are added to the parent mode, which means it is possible to set a priority for which children will be inspired.
- **requiredPred**:
- **inspirationPred**: A predicate that will inspire a mode (given appropriate conditions) based on some state.
- **invokeDelay**: Once inspired, the function invoked by the mode will not execute until the invoke delay has passed.
- **invokeFunction**: The function that will be invoked at inspiration (or invoke delay later). Modes remain active until the application indicates that the function has completed (`RespireContext.complete()`). Functions are expected to by asynchronous - it is not expected that `complete()` will be called within the scope of the function, though that is allowed. In most cases stuff happens and at a later time you call `complete()`.
- **storageTag**: The name used to distinguish the mode when storing parameter values. Currently max 5 characters so that a field name of 8 characters can be constructed of the form: RM----F- (R is the Respire prefix, M---- is the storage tag, and F- is the field tag.)

#### Relationships
- **idleMode**: When all other children of a mode are inactive, this mode is inspired. It is terminated when any one of its siblings is activated. An example would be to activate a sleep function. When nothing else is happening, the system should go to sleep.
- **followMode**: The current mode will be inspired when the mode it is following expires. This is how sequences are constructed.
- **children**: Modes contain child modes. The result is a directed acyclic graph growing from a single root mode. (Since modes can be shared by multiple parents, it is *not* a tree.)

### Naming
I couldn't think of a better lung or breathing related term than "Mode". Is there one? "Gasps"? "Alveoli"? Suggestions welcome.

### Implementation
- `XyzAppState` must subclass `RespireState<XyzAppState>`. (Is this considered ugly? I don't know. It's just an easy way to make the hooks work and have all the necessary types available.)
- `Mode`s are parameterized on a particular application's `XyzAppState` class.
- In order to pass the current and last state values to reacting functions, Respire copies the entire state all the time. I dug for some evil root by pre-optimizing those copies by making a system that separates `Mode` configuration from `ModeState`. On `attach`, `Mode`s allocate a `ModeState` struct for themselves within the `XyzAppState` object. What's the performance hit of keeping `Mode`s as a single thing versus doing this? I don't know. And that's where the evil is. An alternative would be to make a system where each value within the state object maintains its own current and last values, obviating copies of the whole state. TODO: Compare performance of these three implementations.

### TODO
So much to do. Respire is an experiment and there are so many things that need to be improved and that I want to try.
- Enqueue updates. Right now there are problems if you update the application state while it is processing an update.
- Performance testing of various state copy models. (As discussed above.)
- Parallel `Mode`s - Each mode can have a parallel limit saying how many of it can be active at one time.
- `Mode` implied state. Specifying dependent state for a Mode should be easier. For example, if you want a `flag` to be set when either `Mode A` or `Mode B` is active, there should be a way to express that `Mode A` implies `flag`.
- Prefabricated input/output adapters. Tying, for instance, a boolean in state to an output pin should be less than a line.
- Multi-threaded operation. We have plenty of multi-core µcus and Respire has something to offer in terms of coordinating them and making optimal use of the resources available.
- Code generation. I'd like to make the expression of Modes more concise and then generate whatever language structure is necessary for a particular target.

### Motivation
Fear. It was mostly fear. I work on projects at [The Things Network New York](https://thethings.nyc) and each one is some combination of sensors and radios running at minimal power. There's generally too much going on and I feared that in an event or timer based system the logic about what should be happening when would be spread around the code and there would be unpleasant surprises.

And hope. There was definitly hope. The hope is that we can develop better abstractions to work with that empower software developers and enable systems to be aware of themselves. How will systems ever be able to negotiate amongst themselves about which can and should perform some operation if code is still represented at the level of `for` loops?

### Influences
Among the influences I can trace are:
 - [React](https://reactjs.org/) - Using React, I can think about less at one time because the framework takes care of things, like any good framework. The fact that the display code I write is part of a pure one-way flow means I don't have to think about loops and feedback when I'm thinking about display. I wanted Respire to offer the same freedom to focus on doing just one thing at a time.
 - [Clojure Atoms](https://clojure.org/reference/atoms) - The predicates and action functions that take new and old state are remeniscent of atom watchers from Clojure. Perhaps down the road we will figure out how to use structure sharing immutable collections in embedded systems.

## Using Respire in an Arduino project
### 1. Add the library
Using [PlatformIO](https://platformio.org/), just add it to your `lib_deps` list in `platformio.ini`.

(It will also work with the Arduino IDE. Just put it where you put libraries.)

### 2. Write the code
 - 2.1. Define your state class with setters that call `onUpdate()` to advance the system
```c++
#include <respire.h>

class AppState : public RespireState<AppState> {
  bool _onOffFlag = false;

  public:
  bool _led = false;

  bool getOnOffFlag() const { return _onOffFlag; }
  void setOnOffFlag(const bool flag) {
    // Setters have to call onUpdate()
    if (_onOffFlag == flag) {
      return; // Short circuit no change
    }
    AppState oldState(*this);
    _onOffFlag = flag;
    onUpdate(oldState);
  }
}
```
 - 2.2. Declare global state and system variables
```c++
Clock gClock;
Executor<AppState> gExecutor;
AppState gState;
RespireContext<AppState> gRespire(gState, ModeBlink, &gClock, &gExecutor);
```
 - 2.3. Define an invocation function (this one is synchronous - it calls `complete()` within its own scope.)
```c++
void toggleLED(const AppState &state, const AppState &oldState, Mode<AppState> *triggeringMode) {
  digitalWrite(LED_BUILTIN, state._led);
  gRespire.complete(triggeringMode, [](AppState &state){
    // Use optional mutating state function to combine mode completion and state update in one step
    state._led = !state._led;
  });
}
```
 - 2.4. Define a mode using the `Builder` helper class.
```c++
Mode<AppState> ModeBlink(Mode<AppState>::Builder("Blink")
      .invokeFn(toggleLED));
      .periodic(3600, TimeUnitHour)
      .requiredPred([](const AppState &state) -> bool {
        return state.getOnOffFlag();
      }));
```
 - 2.5. Hook Respire into your `setup()` and `loop()` functions.
```c++
void setup() {
    gRespire.init();
    // Initialize state values as necessary. No actions get performed until begin()
    gRespire.begin();
}

void loop() {
  gRespire.loop();
}
```
## Running tests

Respire uses PlatformIO as a build tool.

1. Clone the repository to a local directory: `git clone https://github.com/frankleonrose/Respire.git`
1. Use [PlatformIO](https://platformio.org/) (`pio`) to run unit tests locally (no device required): `pio test -e native_test`

## License
Source code for Respire is released under the MIT License,
which can be found in the [LICENSE](LICENSE) file.

## Copyright
Copyright (c) 2018 Frank Leon Rose
