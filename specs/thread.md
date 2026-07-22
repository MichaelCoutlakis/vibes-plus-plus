# `thread`

**Header:** `<vpp/thread.hpp>`

## Overview

Two cooperating utilities for parallel work:

- `thread_pool` — a fixed-size pool of worker threads that run posted tasks.
- `ordered_pipeline<Result>` — runs tasks in parallel on a `thread_pool` but
  delivers their results to a sink in the order the tasks were *pushed*
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

    template <typename F> void post(F task);        // task: () -> void
    template <typename F> void post_to_all(F init);  // run once per worker; blocks
};
```

- **`post`** enqueues a nullary callable, served FIFO by the workers. The task
  type is a move-only internal wrapper, so move-only captures (e.g. a
  `std::unique_ptr`) are accepted — unlike `std::function`.
- **`post_to_all`** runs `init` exactly once on every worker thread and blocks
  until all have done so. Intended for per-thread setup: `thread_local` state,
  CPU affinity, RNG seeding, etc. It must **not** be called from a pool worker
  thread, nor concurrently with itself — either deadlocks, since it relies on
  every worker being free to pick up exactly one of its gate tasks.
- **Destructor** finishes everything already queued before joining; nothing
  posted before destruction is dropped.
- **Exceptions:** an exception escaping a posted task is caught and swallowed by
  the worker (propagating it would call `std::terminate`). Capture outcomes
  inside the task if you need them; `ordered_pipeline` does this for you.
- `n == 0` is clamped to a single worker.

## `task_result<T>`

The value handed to an `ordered_pipeline` sink. C++17 has no `std::expected`, so
this is a minimal stand-in: exactly one of `{value, error}` is engaged.

```cpp
template <typename T>
class task_result {
public:
    bool has_value() const noexcept;      // true if the task returned a value
    explicit operator bool() const noexcept;   // == has_value()

    std::exception_ptr error() const noexcept;  // the captured exception, or null

    T& operator*();                       // the value; UB if !has_value()
    T* operator->();
    T& value_or_throw();                  // the value, or rethrow the exception
    // ... const / rvalue overloads as expected
};
```

- A successful task yields a **value**; a throwing task yields the captured
  `exception_ptr`. Errors are **data**, not control flow — the sink inspects the
  result and handles the error branch itself.
- `operator*` / `operator->` assume success; dereferencing an error result is
  undefined. Branch on `has_value()` (or `operator bool`) first, or use
  `value_or_throw()` if you would rather turn the error back into a throw.

## `ordered_pipeline<Result>`

```cpp
template <typename Result>
class ordered_pipeline {
public:
    using sink_type = std::function<void(task_result<Result>)>;

    template <typename Sink>                        // Sink: (task_result<Result>) -> void
    ordered_pipeline(thread_pool& pool, Sink sink,
                     std::optional<std::size_t> max_pending = std::nullopt);
    ~ordered_pipeline();                            // drains all outstanding tasks

    ordered_pipeline(const ordered_pipeline&) = delete;
    ordered_pipeline& operator=(const ordered_pipeline&) = delete;

    template <typename F> void push(F task);        // task: () -> Result
    void flush();                                   // block until all emitted
};
```

### Semantics

- **`push(task)`** submits a `() -> Result` callable to the pool. Each push is
  assigned a monotonic sequence number. The worker that runs the task computes
  the result off-lock, deposits it into an internal buffer keyed by that
  sequence, then **emits the ready prefix itself** (see below). `push` is safe
  to call from multiple producer threads.
- **Emission is worker-driven.** After depositing, a worker emits the buffer's
  *ready prefix* — the longest run of completed results starting at the
  next-to-emit sequence — calling the sink strictly in submission order. At most
  one worker emits at a time, so **the sink is never invoked concurrently**;
  but it is invoked **on a pool worker thread, with no thread affinity**, and
  with no internal lock held. A sink that must run on a particular thread has to
  marshal itself there.
- **`flush`** blocks until every task pushed before the call has completed and
  been emitted. It does not throw — a failing task has already been delivered to
  the sink as an error `task_result`, in order.
- **Destructor** drains all outstanding tasks (it must, since each in-flight
  task references the pipeline) before destroying members.

### Ordering under out-of-order completion

If task 0 is slow, tasks 1..N complete first and their results wait in the
buffer. Nothing is emitted until task 0 finishes, at which point the worker that
completes it emits the whole ready prefix in one burst — always in push order.
(That burst runs on the single worker that unblocked it, while the others are
free to keep computing.)

### Exceptions

A throwing task's exception is captured and delivered to the sink, in submission
order, as `task_result::error()` — neither `push` nor `flush` ever throws it.
The stream is not poisoned: emission continues past the failed task with the
following results.

The sink itself **must not throw**: it receives errors as data. Any exception
that does escape the sink is swallowed (letting it propagate would strand the
emitter and stall the pipeline).

### Backpressure (`max_pending`)

The buffer is **unbounded by default**, which maximises throughput: a slow early
task never stalls the workers, since later tasks keep running and their results
simply accumulate until the slow one unblocks a burst of ordered emission.

Passing `max_pending` bounds the number of submitted-but-not-yet-emitted tasks.
`push` then blocks once that many are outstanding, trading peak throughput for a
memory ceiling. Because emission is worker-driven, the workers keep draining the
buffer while `push` waits, so the bound cannot deadlock. (`max_pending == 0` is
clamped to 1.)

### Constraints

- The sink **must not** call back into the pipeline (`push`/`flush`) — the
  emitting worker is parked inside the sink, so no other worker will advance
  emission and the call deadlocks.
- The referenced `thread_pool` must outlive the pipeline.

## Example

```cpp
std::string output;
vpp::thread_pool pool(2);
vpp::ordered_pipeline<std::string> pipeline(
    pool,
    [&](vpp::task_result<std::string> r) {
        if(r)
            output += *r;
        // else: handle r.error() here -- never throw back into the worker.
    });

using namespace std::chrono_literals;
pipeline.push([]{ std::this_thread::sleep_for(100ms); return std::string("the answer "); });
pipeline.push([]{ std::this_thread::sleep_for(1ms);   return std::string("is 42");       });
pipeline.flush();
// output == "the answer is 42"  (push order, despite the second task finishing first)
```

See [examples/thread.cpp](../examples/thread.cpp) and
[tests/test_thread.cpp](../tests/test_thread.cpp).
