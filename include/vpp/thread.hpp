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
        explicit model(F f) : m_f(std::move(f)) {}
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

// Runs tasks in parallel on a thread_pool while emitting their results to a
// sink in the order the tasks were pushed (a resequencer).
//
// Each `push(task)` submits a `() -> Result` callable to the pool. Whichever
// worker runs it deposits the result into an internal buffer keyed by the
// task's submission order; workers never touch the sink. Emission is driven
// only by `push`, `poll` and `flush`, which drain the buffer's ready prefix and
// call the sink strictly in submission order. Consequently the sink is only
// ever invoked from the thread that calls those methods, never concurrently,
// and never from a pool worker.
//
// Exceptions: if a task throws, the exception is captured and re-thrown from a
// draining call (`push`/`poll`/`flush`) at the point that task's result would
// have been emitted -- i.e. in submission order. A later draining call resumes
// with the following results. (During backpressure in bounded mode, a prior
// task's exception may surface from `push` before the new task is enqueued.)
//
// The buffer is unbounded by default so that a slow early task never stalls the
// workers: later tasks keep running and their results accumulate until the slow
// task completes and unblocks a burst of ordered emission. Pass `max_pending`
// to bound the number of submitted-but-not-yet-emitted tasks; `push` then
// blocks once that many are outstanding, trading peak throughput for a memory
// ceiling.
//
// The sink must not call back into the pipeline (push/poll/flush); that would
// deadlock. The destructor drains all outstanding tasks (emitting them, but
// swallowing any task/sink exceptions); call flush() beforehand if you need to
// observe those exceptions.
template <typename Result>
class ordered_pipeline
{
public:
    template <typename Sink>
    ordered_pipeline(
        thread_pool &pool,
        Sink sink,
        std::optional<std::size_t> max_pending = std::nullopt) :
        m_pool(pool),
        m_sink(std::move(sink)),
        m_max_pending(max_pending)
    {
    }

    ~ordered_pipeline() { quiesce(); }

    ordered_pipeline(const ordered_pipeline &) = delete;
    ordered_pipeline &operator=(const ordered_pipeline &) = delete;

    // Submit a task `() -> Result` to run on the pool. In bounded mode, blocks
    // until enough results have been emitted to make room. Emits the ready
    // prefix as a side effect, so it may throw a completed task's exception.
    template <typename F>
    void push(F task)
    {
        std::uint64_t seq;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            if(m_max_pending)
            {
                // Emission is producer-driven, so we must drain here to free
                // room; otherwise ready results would never be emitted and the
                // bound would deadlock.
                drain_locked(/*propagate=*/true);
                while(pending_locked() >= *m_max_pending)
                {
                    m_ready_cv.wait(
                        lock,
                        [this] {
                            return front_ready_locked()
                                || pending_locked() < *m_max_pending;
                        });
                    drain_locked(/*propagate=*/true);
                }
            }
            seq = m_push_seq++;
            ensure_slot_locked(seq); // reserve before posting
        }

        m_pool.post(
            [this, seq, task = std::move(task)]() mutable
            {
                slot outcome;
                try
                {
                    outcome.value.emplace(task());
                }
                catch(...)
                {
                    outcome.error = std::current_exception();
                }
                deposit(seq, std::move(outcome));
            });

        drain(/*propagate=*/true); // emit any ready prefix on the producer thread
    }

    // Emit every result that is ready now, without waiting for outstanding
    // tasks. Non-blocking apart from the sink calls. May be called from a
    // consumer thread distinct from the producer. May throw a task's exception.
    void poll() { drain(/*propagate=*/true); }

    // Block until every task pushed so far has completed and been emitted. May
    // throw a task's exception (in submission order); call again to continue.
    void flush()
    {
        for(;;)
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_ready_cv.wait(
                lock, [this] { return all_emitted_locked() || front_ready_locked(); });
            if(all_emitted_locked())
                return;
            drain_locked(/*propagate=*/true);
        }
    }

private:
    struct slot
    {
        std::optional<Result> value;
        std::exception_ptr error;
        bool ready = false;
    };

    bool all_emitted_locked() const { return m_emit_seq == m_push_seq; }

    std::size_t pending_locked() const
    {
        return static_cast<std::size_t>(m_push_seq - m_emit_seq);
    }

    bool front_ready_locked() const
    {
        return !m_buffer.empty() && m_buffer.front().ready;
    }

    // m_buffer.front() corresponds to m_emit_seq; reserve up to `seq`.
    void ensure_slot_locked(std::uint64_t seq)
    {
        const std::size_t idx = static_cast<std::size_t>(seq - m_emit_seq);
        if(idx >= m_buffer.size())
            m_buffer.resize(idx + 1);
    }

    void deposit(std::uint64_t seq, slot outcome)
    {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            const std::size_t idx = static_cast<std::size_t>(seq - m_emit_seq);
            m_buffer[idx] = std::move(outcome);
            m_buffer[idx].ready = true;
        }
        m_ready_cv.notify_all(); // a slot became ready / room may have opened
    }

    void drain(bool propagate)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        drain_locked(propagate);
    }

    // Emit the ready prefix in submission order. Precondition: m_mutex is held.
    // The lock is kept across sink calls (v1 simplicity; the sink must not
    // re-enter). With propagate == true a task's exception is re-thrown here in
    // order, after advancing past it; with false it (and any sink exception) is
    // swallowed so draining runs to completion.
    void drain_locked(bool propagate)
    {
        while(front_ready_locked())
        {
            slot s = std::move(m_buffer.front());
            m_buffer.pop_front();
            ++m_emit_seq;
            m_ready_cv.notify_all(); // room opened for a bounded-mode push

            if(s.error)
            {
                if(propagate)
                    std::rethrow_exception(s.error);
                continue; // swallow
            }
            if(propagate)
            {
                m_sink(std::move(*s.value));
            }
            else
            {
                try
                {
                    m_sink(std::move(*s.value));
                }
                catch(...)
                {
                }
            }
        }
    }

    // Drain to completion, emitting everything and swallowing exceptions. Used
    // by the destructor: it must wait for every outstanding task to deposit
    // (each posted lambda captures `this`) before members are destroyed.
    void quiesce()
    {
        for(;;)
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_ready_cv.wait(
                lock, [this] { return all_emitted_locked() || front_ready_locked(); });
            if(all_emitted_locked())
                return;
            drain_locked(/*propagate=*/false);
        }
    }

    thread_pool &m_pool;
    std::function<void(Result)> m_sink;
    std::optional<std::size_t> m_max_pending;

    std::mutex m_mutex;
    std::condition_variable m_ready_cv;

    std::deque<slot> m_buffer;    // front corresponds to m_emit_seq
    std::uint64_t m_push_seq = 0; // next sequence to assign on push
    std::uint64_t m_emit_seq = 0; // next sequence to emit
};

} // namespace vpp
