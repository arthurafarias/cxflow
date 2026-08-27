CXFlow is a header-only, GStreamer-inspired dataflow pipeline framework for C++23.
Pipelines are built from **elements** connected by **pads**: a source pushes
`buffer`s downstream, sinks and filters consume and forward them, and
out-of-band control flows through **events** (in-band, pad-to-pad) and
**messages** (posted to a pipeline's **bus**, drained by the application).
Every control-plane property — element state, custom tunables, pad caps —
is a named `variant` value on a common `object` base, and every mutation is
observed through a `threading::signal`, not discovered by polling.

Contributions and design ideas are welcome via the issues channel.

# Table of Contents

- [Getting Started](#getting-started)
- [Core Concepts](#core-concepts)
- [Elements](#elements)
- [Bus & Messaging](#bus--messaging)
- [Testing & Coverage](#testing--coverage)
- [Contributing](#contributing)
- [License](#license)

# Getting Started

CXFlow is header-only, so there's nothing to build or link against — just
point your compiler at `include/`.

The example below wires three elements into `fake_src ! identity ! fake_sink`
and runs it. It is fully event-driven: every reaction — buffer counts, state
transitions, EOS/error — is wired through a signal connected before
`set_state(playing)`, not discovered by polling afterwards, and `main()`
blocks on `SIGINT` (Ctrl-C) rather than on a fixed buffer count:

```c++
#include <atomic>
#include <condition_variable>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <mutex>

#include <cxflow/core/element_factory.hpp>
#include <cxflow/core/message.hpp>
#include <cxflow/core/pad.hpp>
#include <cxflow/core/pipeline.hpp>
#include <cxflow/core/state.hpp>
#include <cxflow/elements/fake_sink.hpp>
#include <cxflow/elements/fake_src.hpp>
#include <cxflow/elements/identity.hpp>

using namespace cxflow;
using namespace cxflow::elements;

namespace {
std::mutex shutdown_mutex;
std::condition_variable shutdown_cv;
std::atomic<bool> interrupted{false};

extern "C" void handle_sigint(int /*signum*/) {
  interrupted.store(true);
  shutdown_cv.notify_all();
}
} // namespace

int main() {
  fake_src::register_type();
  identity::register_type();
  fake_sink::register_type();

  pipeline pipe("fake-pipeline");

  auto src = element_factory::create("fake_src", "src");
  auto id = element_factory::create("identity", "id");
  auto sink = element_factory::create("fake_sink", "sink");

  pipe.add(src);
  pipe.add(id);
  pipe.add(sink);

  if (!src->get_static_pad("src")->link(*id->get_static_pad("sink")) ||
      !id->get_static_pad("src")->link(*sink->get_static_pad("sink"))) {
    std::cerr << "failed to link pipeline\n";
    return 1;
  }

  // Generic property access — no static_cast to fake_src needed.
  src->property_set("num-buffers", std::int64_t{10});
  src->property_set("interval-ms", std::uint64_t{20});

  // Every reaction below is wired before set_state(playing).
  std::atomic<std::uint64_t> buffers_received{0};
  id->get_static_pad("src")->buffer_probe.connect(
      [&](pad &, const buffer &) { ++buffers_received; });

  pipe.on_state_changed([](state old_state, state new_state) {
    std::cout << "pipeline state changed\n";
  });

  pipe.bus().message_posted.connect([](bus &, const message &msg) {
    switch (msg.type) {
    case message_type::eos:
      std::cout << "EOS received\n";
      break;
    case message_type::error:
      std::cerr << "error: " << msg.debug_info << "\n";
      break;
    default:
      break;
    }
  });

  std::signal(SIGINT, handle_sigint);

  pipe.set_state(state::playing);

  std::cout << "running - press Ctrl-C to stop\n";
  {
    std::unique_lock lock(shutdown_mutex);
    shutdown_cv.wait(lock, [] { return interrupted.load(); });
  }

  pipe.set_state(state::null);

  std::cout << "buffers received: " << buffers_received.load() << "\n";

  return 0;
}
```

The full, buildable version lives at
[`examples/fake_pipeline.cpp`](examples/fake_pipeline.cpp). Compile it:

```bash
g++ -std=c++23 -Iinclude -pthread examples/fake_pipeline.cpp -o fake_pipeline
```

Run it and press Ctrl-C once the source finishes (or at any point) to stop:

```
running - press Ctrl-C to stop
EOS received
^Cbuffers received: 10
```

# Core Concepts

Every stateful type in CXFlow — `structure`, `caps`, `pad`, `element`, `bin`,
`bus` — inherits `containers::object`: a named bag of typed `variant`
properties (`property_set()`/`property_get()`) that fires a
`property_changed` signal on every actual change. `variant` is a closed,
discriminated union (`bool`, `int64_t`, `uint64_t`, `double`, `string`, plus
nested list/map alternatives for aggregate values); `variant_map` is the
interface `object` implements around it. Signals themselves
(`threading::signal`) are a small connect/emit primitive with two dispatch
modes today — sequential `emit()` and fire-and-forget `emit_async()` — used
throughout the control plane instead of polling.

An **element** is the unit of work in a pipeline — a source, filter, or sink.
It owns its pads, has a current `state` (itself an `object` property,
observable via `state_changed`/`on_state_changed()`), and can post to a
`bus`. A **pad** is a typed connection point (`direction::src` or
`direction::sink`); linking only succeeds `src → sink`, with neither side
already linked and compatible caps. A pad's `buffer_probe` signal fires on
every buffer pushed through it, for observing the data plane without
touching it.

States are ordered `null < ready < paused < playing`. A **bin** is a composite
element that propagates state changes to its children in data-flow order —
sink-first going up, source-first going down — computed by a topological sort
over the pads' link graph. A **pipeline** is a `bin` that owns the top-level
bus.

# Elements

Three elements ship today, registered by name with `element_factory`:

- **fake_src** — one `src` pad and a dedicated task that pushes buffers on its
  own thread while playing (configurable via the `num-buffers`/`interval-ms`
  properties).
- **identity** — a straight passthrough, one `sink` pad, one `src` pad.
- **fake_sink** — one `sink` pad; counts buffers received and turns an
  incoming EOS event into an EOS message.

Element-specific tunables and results (`num-buffers`, `interval-ms`, a
sink's buffer count, …) are read and written generically through
`property_set()`/`property_get()` — callers never need to `static_cast` to
a concrete element type.

# Bus & Messaging

CXFlow splits control flow into two channels, mirroring GStreamer's
event/message split: **events** travel pad-to-pad in-band (`pad::send_event`
— EOS, flush start/stop); **messages** are posted to a pipeline's bus
(`element::post_message`). An application observes them either by polling
(`bus::pop`, with an optional timeout) or, preferably, by connecting to the
bus's `message_posted` signal — fired in `post()` order, once per message —
so the application's main thread can block on something else (e.g. an OS
interrupt) instead of a poll loop.

# Testing & Coverage

The self-hosted test suite (`include/cxflow/**/testing/*_test.hpp`) builds
into one binary:

```bash
cmake -S . -B build
cmake --build build --target cxflow-tests
build/cxflow-tests
```

An HTML line-coverage report can be generated locally via
[gcovr](https://gcovr.com/) (GCC/Clang only):

```bash
cmake -S . -B build -DMEDIA_STREAMER_ENABLE_COVERAGE=ON
cmake --build build --target coverage
```

# Contributing

We welcome issues and pull requests. Suggestions for optimizations, new
elements, or feature ideas are especially appreciated!

# License

This project is licensed under a proprietary license — see the
[license file](license.md) for details.
