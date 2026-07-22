/******************************************************************************
 * SPDX-License-Identifier: MIT
 * SPDX-FileCopyrightText: 2026 Michael Coutlakis
 *****************************************************************************/
#include <vpp/thread.hpp>
#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

// ---------------------------------------------------------------------------
// thread_pool
// ---------------------------------------------------------------------------

TEST(thread_pool, SizeReflectsRequestedWorkers)
{
    vpp::thread_pool pool(3);
    EXPECT_EQ(pool.size(), 3u);
}

TEST(thread_pool, ZeroWorkersIsClampedToOne)
{
    vpp::thread_pool pool(0);
    EXPECT_EQ(pool.size(), 1u);
}

TEST(thread_pool, RunsEveryPostedTask)
{
    std::atomic<int> count{0};
    {
        vpp::thread_pool pool(4);
        for(int i = 0; i < 1000; ++i)
            pool.post([&count] { ++count; });
        // Destructor finishes all queued work before returning.
    }
    EXPECT_EQ(count.load(), 1000);
}

TEST(thread_pool, AcceptsMoveOnlyTask)
{
    std::atomic<int> value{0};
    {
        vpp::thread_pool pool(2);
        auto p = std::make_unique<int>(42);
        pool.post([&value, p = std::move(p)] { value = *p; });
    }
    EXPECT_EQ(value.load(), 42);
}

TEST(thread_pool, ThrowingTaskDoesNotKillWorker)
{
    std::atomic<int> after{0};
    {
        vpp::thread_pool pool(1);
        pool.post([] { throw std::runtime_error("boom"); });
        pool.post([&after] { ++after; }); // same worker must still run this
    }
    EXPECT_EQ(after.load(), 1);
}

TEST(thread_pool, PostToAllRunsOncePerWorker)
{
    vpp::thread_pool pool(4);

    std::mutex mutex;
    std::set<std::thread::id> threads;
    std::atomic<int> calls{0};

    pool.post_to_all(
        [&]
        {
            ++calls;
            std::lock_guard<std::mutex> lock(mutex);
            threads.insert(std::this_thread::get_id());
        });

    EXPECT_EQ(calls.load(), 4);
    EXPECT_EQ(threads.size(), 4u); // one distinct worker thread each
}

// ---------------------------------------------------------------------------
// ordered_pipeline
// ---------------------------------------------------------------------------

TEST(ordered_pipeline, EmitsInPushOrderDespiteOutOfOrderCompletion)
{
    vpp::thread_pool pool(4);
    std::vector<int> out;
    {
        vpp::ordered_pipeline<int> pipe(pool, [&](vpp::task_result<int> r) { out.push_back(*r); });
        // Earlier tasks sleep longer, so they finish last.
        for(int i = 0; i < 8; ++i)
            pipe.push([i] {
                std::this_thread::sleep_for(std::chrono::milliseconds((8 - i) * 5));
                return i;
            });
        pipe.flush();
    }
    ASSERT_EQ(out.size(), 8u);
    for(int i = 0; i < 8; ++i)
        EXPECT_EQ(out[i], i);
}

TEST(ordered_pipeline, SinkRunsOnAWorkerAndIsNeverInvokedConcurrently)
{
    vpp::thread_pool pool(4);
    const std::thread::id producer = std::this_thread::get_id();
    std::atomic<int> in_sink{0};
    std::atomic<bool> overlapped{false};
    std::atomic<bool> ran_on_worker{false};

    {
        vpp::ordered_pipeline<int> pipe(
            pool,
            [&](vpp::task_result<int>) {
                // Emission is worker-driven now: the sink runs on a pool thread,
                // never the producer, and -- guarded by a single emitter -- never
                // two at once. Widen the window so any overlap is observed.
                if(std::this_thread::get_id() != producer)
                    ran_on_worker = true;
                if(in_sink.fetch_add(1) != 0)
                    overlapped = true;
                std::this_thread::sleep_for(1ms);
                in_sink.fetch_sub(1);
            });
        for(int i = 0; i < 50; ++i)
            pipe.push([i] { return i; });
        pipe.flush();
    }
    EXPECT_FALSE(overlapped);
    EXPECT_TRUE(ran_on_worker);
}

TEST(ordered_pipeline, FlushWaitsForAllResults)
{
    vpp::thread_pool pool(2);
    std::atomic<int> emitted{0};
    {
        vpp::ordered_pipeline<int> pipe(pool, [&](vpp::task_result<int>) { ++emitted; });
        for(int i = 0; i < 20; ++i)
            pipe.push([i] {
                std::this_thread::sleep_for(2ms);
                return i;
            });
        pipe.flush();
        EXPECT_EQ(emitted.load(), 20);
    }
}

TEST(ordered_pipeline, TaskExceptionDeliveredToSinkInOrderThenStreamContinues)
{
    vpp::thread_pool pool(4);
    std::vector<std::string> events; // successes as text, failures as "error"
    std::exception_ptr captured;
    {
        vpp::ordered_pipeline<int> pipe(pool, [&](vpp::task_result<int> r) {
            if(r)
            {
                events.push_back(std::to_string(*r));
            }
            else
            {
                events.push_back("error");
                captured = r.error();
            }
        });

        pipe.push([] { return 10; });
        pipe.push([]() -> int { throw std::runtime_error("boom"); });
        pipe.push([] { return 30; });

        pipe.flush(); // never throws: the failure arrives as data, in order
    }

    ASSERT_EQ(events.size(), 3u);
    EXPECT_EQ(events[0], "10");
    EXPECT_EQ(events[1], "error");
    EXPECT_EQ(events[2], "30");

    ASSERT_TRUE(captured);
    EXPECT_THROW(std::rethrow_exception(captured), std::runtime_error);
}

TEST(ordered_pipeline, SupportsMoveOnlyResultType)
{
    vpp::thread_pool pool(3);
    std::vector<int> out;
    {
        vpp::ordered_pipeline<std::unique_ptr<int>> pipe(
            pool, [&](vpp::task_result<std::unique_ptr<int>> r) { out.push_back(**r); });
        for(int i = 0; i < 10; ++i)
            pipe.push([i] { return std::make_unique<int>(i); });
        pipe.flush();
    }
    ASSERT_EQ(out.size(), 10u);
    for(int i = 0; i < 10; ++i)
        EXPECT_EQ(out[i], i);
}

TEST(ordered_pipeline, DestructorFlushesRemaining)
{
    vpp::thread_pool pool(2);
    std::vector<int> out;
    {
        vpp::ordered_pipeline<int> pipe(pool, [&](vpp::task_result<int> r) { out.push_back(*r); });
        for(int i = 0; i < 16; ++i)
            pipe.push([i] {
                std::this_thread::sleep_for(1ms);
                return i;
            });
        // No explicit flush: the destructor must drain and emit everything.
    }
    ASSERT_EQ(out.size(), 16u);
    for(int i = 0; i < 16; ++i)
        EXPECT_EQ(out[i], i);
}

TEST(ordered_pipeline, BoundedModePreservesOrderAndDoesNotDeadlock)
{
    vpp::thread_pool pool(4);
    std::vector<int> out;
    {
        // A tight bound plus a slow head exercises backpressure: worker-driven
        // emission must free slots so push can make room rather than block forever.
        vpp::ordered_pipeline<int> pipe(
            pool, [&](vpp::task_result<int> r) { out.push_back(*r); }, 2);
        for(int i = 0; i < 100; ++i)
            pipe.push([i] {
                if(i == 0)
                    std::this_thread::sleep_for(30ms);
                return i;
            });
        pipe.flush();
    }
    ASSERT_EQ(out.size(), 100u);
    for(int i = 0; i < 100; ++i)
        EXPECT_EQ(out[i], i);
}
