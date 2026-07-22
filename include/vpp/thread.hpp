/******************************************************************************
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 Michael Coutlakis
 *****************************************************************************/
#pragma once
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace vpp
{

namespace detail
{

// A minimal move-only, type-erased nullary callable.
//
// std::function requires the wrapped callable to be copyable, which rejects
// closures that capture move-only state (as ordered_pipeline's task wrappers
// do). unique_task stores any `() -> void` callable behind a single heap
// allocation and never copies it.
class unique_task
{
public:
    unique_task() = default;

    template <
        typename F,
        typename = std::enable_if_t<!std::is_same_v<std::decay_t<F>, unique_task>>>
    unique_task(F &&f) :
        m_impl(std::make_unique<model<std::decay_t<F>>>(std::forward<F>(f)))
    {
    }

    unique_task(unique_task &&) noexcept = default;
    unique_task &operator=(unique_task &&) noexcept = default;
    unique_task(const unique_task &) = delete;
    unique_task &operator=(const unique_task &) = delete;

    explicit operator bool() const noexcept { return m_impl != nullptr; }

    void operator()() { m_impl->call(); }

private:
    struct concept_t
    {
        virtual ~concept_t() = default;
        virtual void call() = 0;
    };

    template <typename F>
    struct model final : concept_t
    {
        F m_f;
        explicit model(F f) :
            m_f(std::move(f))
        {
        }
        void call() override { m_f(); }
    };

    std::unique_ptr<concept_t> m_impl;
};

} // namespace detail

// A fixed-size pool of worker threads that run posted tasks.
//
// Tasks are nullary callables (`() -> void`), served FIFO from a shared queue.
// The destructor finishes all queued work before joining, so nothing posted
// before destruction is dropped.
//
// An exception escaping a posted task is caught and swallowed by the worker
// (letting it propagate would call std::terminate). Callers that need to
// observe task failures must capture the outcome inside the task itself;
// ordered_pipeline does exactly this.
class thread_pool
{
public:
    explicit thread_pool(std::size_t n = std::thread::hardware_concurrency())
    {
        if(n == 0)
            n = 1;
        m_workers.reserve(n);
        for(std::size_t i = 0; i < n; ++i)
            m_workers.emplace_back([this] { worker_loop(); });
    }

    ~thread_pool()
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stopping = true;
        }
        m_cv.notify_all();
        for(auto &w : m_workers)
            if(w.joinable())
                w.join();
    }

    thread_pool(const thread_pool &) = delete;
    thread_pool &operator=(const thread_pool &) = delete;

    std::size_t size() const noexcept { return m_workers.size(); }

    // Enqueue a task for execution on some worker thread.
    template <typename F>
    void post(F task)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_queue.emplace_back(std::move(task));
        }
        m_cv.notify_one();
    }

    // Run `init` exactly once on every worker thread, blocking until all have
    // done so. Intended for per-thread setup such as thread_local state, CPU
    // affinity, or RNG seeding. `init` is invoked with no arguments.
    //
    // Warning: must not be called from a pool worker thread, nor concurrently
    // with itself on another thread -- either deadlocks, since it depends on
    // every worker being free to pick up exactly one of the `n` gate tasks.
    template <typename F>
    void post_to_all(F init)
    {
        // A gate makes each worker run `init` exactly once: a worker that has
        // run it blocks on the gate, so it cannot grab a second copy before its
        // peers have each taken one. The control block is heap-owned by a
        // shared_ptr held in every task, so it outlives this call even though
        // workers may still be exiting the gate after we return.
        struct control
        {
            std::mutex mutex;
            std::condition_variable done_cv;
            std::condition_variable gate_cv;
            std::size_t remaining;
            bool open = false;
        };

        const std::size_t n = m_workers.size();
        auto ctl = std::make_shared<control>();
        ctl->remaining = n;

        for(std::size_t i = 0; i < n; ++i)
        {
            post(
                [ctl, init]() mutable
                {
                    init();
                    {
                        std::lock_guard<std::mutex> lock(ctl->mutex);
                        --ctl->remaining;
                    }
                    ctl->done_cv.notify_one();

                    std::unique_lock<std::mutex> lock(ctl->mutex);
                    ctl->gate_cv.wait(lock, [&] { return ctl->open; });
                });
        }

        std::unique_lock<std::mutex> lock(ctl->mutex);
        ctl->done_cv.wait(lock, [&] { return ctl->remaining == 0; });
        ctl->open = true; // release every worker
        lock.unlock();
        ctl->gate_cv.notify_all();
    }

private:
    void worker_loop()
    {
        for(;;)
        {
            detail::unique_task task;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_cv.wait(lock, [this] { return m_stopping || !m_queue.empty(); });
                if(m_queue.empty()) // implies m_stopping
                    return;
                task = std::move(m_queue.front());
                m_queue.pop_front();
            }
            try
            {
                task();
            }
            catch(...)
            {
                // A posted task must not take down its worker; see class note.
            }
        }
    }

    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::deque<detail::unique_task> m_queue;
    bool m_stopping = false;
    std::vector<std::thread> m_workers;
};
// A minimal expected-lite delivered to the sink (C++17 has no std::expected).
//
// Exactly one of {value, error} is engaged: a successful task yields a value,
// a throwing task yields the captured exception_ptr. The sink must inspect it
// and handle the error branch itself -- errors are data here, not control flow.
template <typename T>
class task_result
{
public:
    task_result(std::optional<T> value, std::exception_ptr error) noexcept :
        m_value(std::move(value)),
        m_error(std::move(error))
    {
    }

    bool has_value() const noexcept { return m_value && !m_error; }
    explicit operator bool() const noexcept { return has_value(); }

    std::exception_ptr error() const noexcept { return m_error; }

    T &operator*() & noexcept { return *m_value; }
    const T &operator*() const & noexcept { return *m_value; }
    T &&operator*() && noexcept { return std::move(*m_value); }
    T *operator->() noexcept { return &*m_value; }
    const T *operator->() const noexcept { return &*m_value; }

    // For sinks that would rather branch on a throw than on has_value().
    T &value_or_throw() &
    {
        if(m_error)
            std::rethrow_exception(m_error);
        return *m_value;
    }

private:
    std::optional<T> m_value;
    std::exception_ptr m_error;
};

// Runs tasks in parallel on a thread_pool and delivers their results to a sink
// strictly in submission order (a resequencer).
//
// Each `push(task)` submits a `() -> Result` callable. The worker that runs it
// computes off-lock, deposits the outcome keyed by submission order, then emits
// the contiguous ready prefix. Emission is therefore driven by the workers, not
// the producer: `push` blocks only when a bound is set and the window is full.
// At most one thread emits at a time (m_emitting), so the sink is never invoked
// concurrently -- but it IS invoked on pool worker threads, with no thread
// affinity. A sink that needs to run on a particular thread must marshal
// itself.
//
// Errors: a throwing task's exception is captured and passed to the sink, in
// order, as task_result::error(). Neither push nor flush ever throws a task's
// exception. The sink receives errors as data and MUST NOT throw; any exception
// escaping the sink is swallowed (it would otherwise strand m_emitting).
//
// The buffer is unbounded by default so a slow early task never stalls workers;
// pass max_pending to bound submitted-but-not-emitted tasks (trades throughput
// for a memory ceiling). The destructor waits for every outstanding task to
// complete and be emitted before destroying members (see m_inflight).
//
// Warning: the sink runs on a pool worker (see above) and must not re-enter the
// pipeline. Calling push() or flush() from the sink deadlocks: the emitting
// worker is parked inside the sink, so no other worker will advance emission.
template <typename Result>
class ordered_pipeline
{
public:
    using sink_type = std::function<void(task_result<Result>)>;

    template <typename Sink>
    ordered_pipeline(
        thread_pool &pool,
        Sink sink,
        std::optional<std::size_t> max_pending = std::nullopt) :
        m_pool(pool),
        m_sink(std::move(sink)),
        m_max_pending(
            max_pending && *max_pending == 0 ? std::optional<std::size_t>(1) : max_pending)
    {
    }

    // Waits for every posted task to fully return. Each posted lambda captured
    // `this`, and a result may be emitted by a *different* worker than the one
    // that computed it, so "all results emitted" is not sufficient to know no
    // worker still touches us -- we must wait on the task count itself. By the
    // time m_inflight hits zero, every deposit, emission and sink call is done.
    ~ordered_pipeline()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_idle_cv.wait(lock, [this] { return m_inflight == 0; });
    }

    ordered_pipeline(const ordered_pipeline &) = delete;
    ordered_pipeline &operator=(const ordered_pipeline &) = delete;

    // Submit a task `() -> Result`. In bounded mode, blocks until a slot frees.
    // Never runs the sink and never throws a task's exception.
    template <typename F>
    void push(F task)
    {
        std::uint64_t seq;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            if(m_max_pending)
                m_space_cv.wait(lock, [this] { return pending_locked() < *m_max_pending; });
            seq = m_push_seq++;
            ensure_slot_locked(seq); // reserve before posting
            ++m_inflight;
        }

        m_pool.post(
            [this, seq, task = std::move(task)]() mutable
            {
                slot outcome;
                try
                {
                    outcome.value.emplace(task()); // compute off-lock
                }
                catch(...)
                {
                    outcome.error = std::current_exception();
                }
                deposit(seq, std::move(outcome));
                emit();

                std::lock_guard<std::mutex> lock(m_mutex);
                if(--m_inflight == 0)
                    m_idle_cv.notify_all();
            });
    }

    // Block until every task pushed before this call has been emitted. Does not
    // throw; errors have already been delivered to the sink in order.
    void flush()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        const std::uint64_t target = m_push_seq;
        m_drained_cv.wait(lock, [&] { return m_emit_seq >= target; });
    }

private:
    struct slot
    {
        std::optional<Result> value;
        std::exception_ptr error;
        bool ready = false;
    };

    std::size_t pending_locked() const { return static_cast<std::size_t>(m_push_seq - m_emit_seq); }

    // m_buffer.front() corresponds to m_emit_seq; reserve up to `seq`.
    void ensure_slot_locked(std::uint64_t seq)
    {
        const std::size_t idx = static_cast<std::size_t>(seq - m_emit_seq);
        if(idx >= m_buffer.size())
            m_buffer.resize(idx + 1);
    }

    void deposit(std::uint64_t seq, slot outcome)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        const std::size_t idx = static_cast<std::size_t>(seq - m_emit_seq);
        outcome.ready = true;
        m_buffer[idx] = std::move(outcome);
        // No notify: this worker emits next, or the active emitter re-checks
        // the head under the lock and picks this slot up.
    }

    // Emit the contiguous ready prefix in submission order. At most one thread
    // emits at a time; the sink runs with the mutex released so workers can keep
    // depositing. The final "front not ready" check and clearing m_emitting are
    // one contiguous critical section, so any concurrent deposit is ordered
    // either before that check (this emitter takes it) or after the unlock (that
    // depositor's own emit() takes it) -- no ready slot is ever stranded.
    void emit()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if(m_emitting)
            return;
        m_emitting = true;

        while(!m_buffer.empty() && m_buffer.front().ready)
        {
            slot s = std::move(m_buffer.front());
            m_buffer.pop_front();
            ++m_emit_seq;
            m_space_cv.notify_all();   // room freed for a bounded producer
            m_drained_cv.notify_all(); // m_emit_seq advanced, for flush()

            lock.unlock();
            try
            {
                m_sink(task_result<Result>(std::move(s.value), std::move(s.error)));
            }
            catch(...)
            {
                // The sink gets errors as data and must not throw; swallowing
                // keeps m_emitting consistent and the pipeline alive.
            }
            lock.lock();
        }

        m_emitting = false;
    }

    thread_pool &m_pool;
    sink_type m_sink;
    std::optional<std::size_t> m_max_pending;

    std::mutex m_mutex;
    std::condition_variable m_space_cv;   // a slot was emitted (room freed)
    std::condition_variable m_drained_cv; // m_emit_seq advanced (for flush)
    std::condition_variable m_idle_cv;    // m_inflight reached zero (for dtor)

    std::deque<slot> m_buffer; // front corresponds to m_emit_seq
    std::uint64_t m_push_seq = 0;
    std::uint64_t m_emit_seq = 0;
    std::size_t m_inflight = 0;
    bool m_emitting = false;
};

} // namespace vpp
