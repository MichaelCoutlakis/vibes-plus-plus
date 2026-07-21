# `thread`

**Header:** `<vpp/thread.hpp>`

## Overview

Two cooperating utilities for parallel work:

- `thread_pool` — a fixed-size pool of worker threads that run posted tasks.
- `ordered_pipeline<Result>` — runs tasks in parallel on a `thread_pool` but
  emits their results to a sink in the order the tasks were *pushed*
  (a resequencer). You push callables to it, it parallelises the work, and it
  hands results to your sink serially and in submission order.

Both live in namespace `vpp`.

## `thread_pool`

```cpp
class thread_pool {
public:
    explicit thread_pool(std::size_t n = std::thread::hardware_concurrency());
    ~thread_pool();                               // finishes all queued work, then joins

    thread_pool(const thread_pool&) = delete;
    thread_pool& operator=(const thread_pool&) = delete;

    std::size_t size() const noexcept;

    template <typename F> void post(F task);       // task: () -> void
    template <typename F> void post_to_all(F init); // run once per worker; blocks
};
```

- **`post`** enqueues a nullary callable, served FIFO by the workers. The task
  type is a move-only internal wrapper, so move-only captures (e.g. a
  `std::unique_ptr`) are accepted — unlike `std::function`.
- **`post_to_all`** runs `init` exactly once on every worker thread and blocks
  until all have done so. Intended for per-thread setup: `thread_local` state,
  CPU affinity, RNG seeding, etc.
- **Destructor** finishes everything already queued before joining; nothing
  posted before destruction is dropped.
- **Exceptions:** an exception escaping a posted task is caught and swallowed by
  the worker (propagating it would call `std::terminate`). Capture outcomes
  inside the task if you need them; `ordered_pipeline` does this for you.
- `n == 0` is clamped to a single worker.

## `ordered_pipeline<Result>`

```cpp
template <typename Result>
class ordered_pipeline {
public:
    template <typename Sink>                        // Sink: (Result) -> void
    ordered_pipeline(thread_pool& pool, Sink sink,
                     std::optional<std::size_t> max_pending = std::nullopt);
    ~ordered_pipeline();                            // drains all outstanding tasks

    ordered_pipeline(const ordered_pipeline&) = delete;
    ordered_pipeline& operator=(const ordered_pipeline&) = delete;

    template <typename F> void push(F task);        // task: () -> Result
    void poll();                                    // emit ready prefix, non-blocking
    void flush();                                   // block until all emitted
};
```

### Semantics

- **`push(task)`** submits a `() -> Result` callable to the pool. Each push is
  assigned a monotonic sequence number. Whichever worker runs the task deposits
  its result into an internal buffer keyed by that sequence; **workers never
  touch the sink**.
- **Emission** (calling the sink) is driven only by `push`, `poll` and `flush`.
  They drain the buffer's *ready prefix* — the longest run of completed results
  starting at the next-to-emit sequence — and call the sink strictly in
  submission order. Consequently the sink is only ever invoked from the thread
  that calls those methods, never concurrently, and never from a pool worker.
- **`poll`** emits whatever is ready now without waiting for outstanding tasks.
  It may be called from a consumer thread distinct from the producer.
- **`flush`** blocks until every task pushed so far has completed and been
  emitted.
- **Destructor** drains all outstanding tasks (it must, since each in-flight
  task references the pipeline), emitting them but swallowing exceptions. Call
  `flush()` first if you need to observe a late task's exception.

### Ordering under out-of-order completion

If task 0 is slow, tasks 1..N complete first and their results wait in the
buffer. Nothing is emitted until task 0 finishes, at which point the whole ready
prefix is emitted in one burst — always in push order.

### Exceptions

If a task throws, the exception is captured and re-thrown from a draining call
(`push`/`poll`/`flush`) at the point that task's result would have been emitted
— i.e. in submission order. A later draining call resumes with the following
results, so the stream is not poisoned.

### Backpressure (`max_pending`)

The buffer is **unbounded by default**, which maximises throughput: a slow early
task never stalls the workers, since later tasks keep running and their results
simply accumulate until the slow one unblocks a burst of ordered emission.

Passing `max_pending` bounds the number of submitted-but-not-yet-emitted tasks.
`push` then blocks once that many are outstanding, trading peak throughput for a
memory ceiling. (Because emission is producer-driven, `push` drains as it waits,
so the bound cannot deadlock.)

### Constraints

- The sink **must not** call back into the pipeline (`push`/`poll`/`flush`);
  that would deadlock. (In this v1 the sink runs under the internal lock.)
- The referenced `thread_pool` must outlive the pipeline.

## Example

```cpp
std::string output;
vpp::thread_pool pool(2);
vpp::ordered_pipeline<std::string> pipeline(
    pool, [&](std::string s) { output += s; });

using namespace std::chrono_literals;
pipeline.push([]{ std::this_thread::sleep_for(100ms); return std::string("the answer "); });
pipeline.push([]{ std::this_thread::sleep_for(1ms);   return std::string("is 42");       });
pipeline.flush();
// output == "the answer is 42"  (push order, despite the second task finishing first)
```

See [examples/thread.cpp](../examples/thread.cpp) and
[tests/test_thread.cpp](../tests/test_thread.cpp).
