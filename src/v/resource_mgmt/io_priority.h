/*
 * Copyright 2020 Vectorized, Inc.
 *
 * Use of this software is governed by the Business Source License
 * included in the file licenses/BSL.md
 *
 * As of the Change Date specified in that file, in accordance with
 * the Business Source License, use of this software will be governed
 * by the Apache License, Version 2.0
 */

#pragma once

#include "seastarx.h"

#include <seastar/core/distributed.hh>
#include <seastar/core/future.hh>
#include <seastar/core/reactor.hh>

class priority_manager {
public:
    ss::io_priority_class raft_priority() { return _raft_priority; }
    ss::io_priority_class controller_priority() { return _controller_priority; }
    ss::io_priority_class kafka_read_priority() { return _kafka_read_priority; }
    ss::io_priority_class compaction_priority() { return _compaction_priority; }

    static priority_manager& local() {
        static thread_local priority_manager pm = priority_manager();
        return pm;
    }

private:
    priority_manager()
      : _raft_priority(ss::engine().register_one_priority_class("raft", 1000))
      , _controller_priority(
          ss::engine().register_one_priority_class("controller", 1000))
      , _kafka_read_priority(
          ss::engine().register_one_priority_class("kafka_read", 200))
      , _compaction_priority(
          ss::engine().register_one_priority_class("compaction", 200)) {}

    ss::io_priority_class _raft_priority;
    ss::io_priority_class _controller_priority;
    ss::io_priority_class _kafka_read_priority;
    ss::io_priority_class _compaction_priority;
};

inline ss::io_priority_class raft_priority() {
    return priority_manager::local().raft_priority();
}

inline ss::io_priority_class controller_priority() {
    return priority_manager::local().controller_priority();
}

inline ss::io_priority_class kafka_read_priority() {
    return priority_manager::local().kafka_read_priority();
}

inline ss::io_priority_class compaction_priority() {
    return priority_manager::local().compaction_priority();
}
