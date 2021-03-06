/*
 * Copyright 2019 Peifeng Yu <peifeng@umich.edu>
 * 
 * This file is part of Salus
 * (see https://github.com/SymbioticLab/Salus).
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *    http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SALUS_EXEC_SESSIONITEM_H
#define SALUS_EXEC_SESSIONITEM_H

#include "utils/containerutils.h"
#include "resources/resources.h"
#include "resources/iteralloctracker.h"
#include "execution/devices.h"
#include "execution/engine/taskexecutor.h"
#include "execution/engine/allocationlistener.h"
#include "platform/thread_annotations.h"

#include <list>
#include <string>
#include <functional>
#include <mutex>
#include <unordered_set>
#include <unordered_map>
#include <memory>
#include <any>
#include <utility>

struct OperationItem;
using POpItem = std::shared_ptr<OperationItem>;

namespace salus {
class ExecutionEngine;
}
/**
 * @todo write docs
 */
struct SessionItem : public salus::AllocationListener
{
    using KernelQueue = std::list<POpItem>;
    using UnsafeQueue = std::list<POpItem>;
private:
    // protected by mu (may be accessed both in schedule thread and close session thread)
    salus::PagingCallbacks pagingCb GUARDED_BY(mu);
    std::function<void()> cleanupCb GUARDED_BY(mu);

    // called if the execution engine requires to interrupt the session
    std::function<void()> interruptCb GUARDED_BY(mu);

    KernelQueue queue GUARDED_BY(mu);
    // total number of executed op in this session
    uint64_t totalExecutedOp = 0 GUARDED_BY(mu);

    // rm for current iteration
    const static constexpr ResourceTag trackerTag = resources::GPU0Memory;
    std::unordered_map<uint64_t, salus::IterAllocTracker> allocTrackers GUARDED_BY(mu);

    void updateTracker(uint64_t graphId, const ResourceTag &tag);

    std::mutex mu;

    size_t lastScheduled = 0;

    uint64_t holWaiting = 0;
    size_t queueHeadHash = 0;

    std::unordered_set<uint64_t> tickets;
    std::mutex tickets_mu;

    // Accessed by multiple scheduling thread
    std::atomic_bool protectOOM{true};

    // Iters should goto blockingIters queue
    std::atomic_bool exlusiveMode{true};

    friend class salus::TaskExecutor;
    friend class BaseScheduler;
    friend class salus::ExecutionEngine;

public:
    std::string sessHandle;

    // Only accessed by main scheduling thread
    UnsafeQueue bgQueue;
    bool forceEvicted{false};

    // target runnimg time
    uint64_t totalRunningTime {0};
    std::atomic_uint_fast64_t usedRunningTime {0};
    std::atomic_uint_fast64_t numFinishedIters {0};

    explicit SessionItem(std::string handle)
        : sessHandle(std::move(handle))
    {
        // NOTE: add other devices
        resUsage[resources::GPU0Memory].get() = 0;
        resUsage[resources::GPU1Memory].get() = 0;
        resUsage[resources::CPU0Memory].get() = 0;
        resUsage[{ResourceType::GPU_STREAM, salus::devices::GPU0}].get() = 0;
        resUsage[{ResourceType::GPU_STREAM, salus::devices::GPU1}].get() = 0;
    }

    ~SessionItem() override;

    sstl::MutableAtom::value_type &resourceUsage(const ResourceTag &tag)
    {
        return resUsage.at(tag).get();
    }

    void setPagingCallbacks(salus::PagingCallbacks pcb);
    void setInterruptCallback(std::function<void()> cb);
    void setExclusiveMode(bool mode)
    {
        exlusiveMode = mode;
    }

    void queueTask(POpItem &&opItem);

    bool beginIteration(AllocationRegulator::Ticket t, ResStats newRm, uint64_t graphId);

    void endIteration(uint64_t graphId);

    /**
     * @brief prepare to remove session from execution engine.
     * 
     * This clears paging callbacks, and setup a cleanup callback that gets called
     * once the item is actually remove from execution engine.
     * 
     * Typical use:
     * ```
     * item.finalCleanup(cleanupCallback);
     * engine.deleteSession(std::move(item));
     * ```
     */
    void prepareDelete(std::function<void()> cb);

    void notifyAlloc(uint64_t graphId, uint64_t ticket, const ResourceTag &tag, size_t num) override;
    void notifyDealloc(uint64_t graphId, uint64_t ticket, const ResourceTag &tag, size_t num, bool last) override;

    void interrupt();

private:
    using AtomicResUsages = std::unordered_map<ResourceTag, sstl::MutableAtom>;
    // must be initialized in constructor
    AtomicResUsages resUsage;
};
using PSessionItem = std::shared_ptr<SessionItem>;
using SessionList = std::list<PSessionItem>;
using SessionSet = std::unordered_set<PSessionItem>;

#endif // SALUS_EXEC_SESSIONITEM_H
