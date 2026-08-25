<picture>
  <source srcset="docs.src/resources/logo-composed-black.png" media="(prefers-color-scheme: dark)">
  <img src="docs.src/resources/logo-composed-light.png" alt="CXFlow" height="72">
</picture>

CXFlow is a header-only, GStreamer-inspired dataflow pipeline framework for C++23.
Pipelines are built from **elements** connected by **pads**: a source pushes
`buffer`s downstream, sinks and filters consume and forward them, and
out-of-band control flows through **events** (in-band, pad-to-pad) and
**messages** (posted to a pipeline's **bus**, drained by the application).

The project is in an early conceptual stage — the current pass proves the core
loop end-to-end (pad linking, task-driven push scheduling, state propagation,
and the bus/event/message split) with a minimal set of elements. Many features
are still under exploration. Contributions and design ideas are welcome via
the issues channel.

# Table of Contents

- [Getting Started](#getting-started)
- [Core Concepts](#core-concepts)
- [Elements](#elements)
- [Bus & Messaging](#bus--messaging)
- [Documentation](#documentation)
- [Contributing](#contributing)
- [License](#license)

# Getting Started

CXFlow is header-only, so there's nothing to build or link against — just
point your compiler at `include/`.

The example below wires three elements into `fake_src ! identity ! fake_sink`,
runs it to end-of-stream, and reports how many buffers the sink saw:

```c++
#include <chrono>
#include <iostream>

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

int main() {
  fake_src::register_type();
  identity::register_type();
  fake_sink::register_type();

  pipeline pipe("fake-pipeline");

  auto src = element_factory::create("fake_src", "src");
  auto id = element_factory::create("identity", "id");
  auto sink = element_factory::create("fake_sink", "sink");

  auto *src_impl = static_cast<fake_src *>(src.get());
  src_impl->set_num_buffers(10);
  src_impl->set_interval(std::chrono::milliseconds(20));

  pipe.add(src);
  pipe.add(id);
  pipe.add(sink);

  if (!src->get_static_pad("src")->link(*id->get_static_pad("sink")) ||
      !id->get_static_pad("src")->link(*sink->get_static_pad("sink"))) {
    std::cerr << "failed to link pipeline\n";
    return 1;
  }

  pipe.set_state(state::playing);

  bool running = true;
  while (running) {
    auto msg = pipe.bus().pop(std::chrono::milliseconds(500));
    if (!msg.has_value()) {
      continue;
    }

    switch (msg->type) {
    case message_type::eos:
      std::cout << "EOS received\n";
      running = false;
      break;
    case message_type::error:
      std::cerr << "error: " << msg->debug_info << "\n";
      running = false;
      break;
    default:
      break;
    }
  }

  pipe.set_state(state::null);

  auto *sink_impl = static_cast<fake_sink *>(sink.get());
  std::cout << "buffers received: " << sink_impl->buffers_received() << "\n";

  return 0;
}
```

Compile it:

```bash
g++ -std=c++23 -Iinclude -pthread examples/fake_pipeline.cpp -o fake_pipeline
```

Running it produces:

```
EOS received
buffers received: 10
```

# Core Concepts

An **element** is the unit of work in a pipeline — a source, filter, or sink.
It owns its pads, has a current `state`, and can post to a `bus`. A **pad** is
a typed connection point (`direction::src` or `direction::sink`); linking only
succeeds `src → sink`, with neither side already linked and compatible caps.

States are ordered `null < ready < paused < playing`. A **bin** is a composite
element that propagates state changes to its children in data-flow order —
sink-first going up, source-first going down — computed by a topological sort
over the pads' link graph. A **pipeline** is a `bin` that owns the top-level
bus.

# Elements

Three elements ship today, registered by name with `element_factory`:

- **fake_src** — one `src` pad and a dedicated task that pushes buffers on its
  own thread while playing (configurable count/interval).
- **identity** — a straight passthrough, one `sink` pad, one `src` pad.
- **fake_sink** — one `sink` pad; counts buffers received and turns an
  incoming EOS event into an EOS message.

# Bus & Messaging

CXFlow splits control flow into two channels, mirroring GStreamer's
event/message split: **events** travel pad-to-pad in-band (`pad::send_event`
— EOS, flush start/stop); **messages** are posted to a pipeline's bus
(`element::post_message`) and drained asynchronously by the application's own
loop (`bus::pop`).

# Documentation

The full documentation site lives under [`docs.src/`](docs.src) (Hugo
source) and builds to [`docs/`](docs). To preview it locally:

```bash
cd docs.src && hugo server
```

# Contributing

We welcome issues and pull requests. Suggestions for optimizations, new
elements, or feature ideas are especially appreciated!

# License

This project is licensed under a proprietary license — see the
[license file](license.md) for details.
