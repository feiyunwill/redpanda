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

#include "kafka/client.h"
#include "pandaproxy/client/broker.h"
#include "pandaproxy/client/brokers.h"
#include "pandaproxy/client/configuration.h"
#include "pandaproxy/client/fetcher.h"
#include "pandaproxy/client/producer.h"
#include "pandaproxy/client/retry_with_mitigation.h"
#include "utils/retry.h"
#include "utils/unresolved_address.h"

#include <seastar/core/condition-variable.hh>
#include <seastar/core/semaphore.hh>

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>

namespace pandaproxy::client {

/// \brief wait or start a function
///
/// Start the function and wait for it to finish, or, if an instance of the
/// function is already running, wait for that one to finish.
class wait_or_start {
public:
    // Prevent accidentally calling the protected func.
    struct tag {};
    using func = ss::noncopyable_function<ss::future<>(tag)>;

    explicit wait_or_start(func func)
      : _func{std::move(func)} {}

    ss::future<> operator()() {
        if (_lock.try_wait()) {
            return _func(tag{}).finally(
              [this]() { _lock.signal(_lock.waiters() + 1); });
        }
        return _lock.wait();
    }

private:
    func _func;
    ss::semaphore _lock{1};
};

class client {
public:
    explicit client(std::vector<unresolved_address> broker_addrs);

    /// \brief Connect to all brokers.
    ss::future<> connect();
    /// \brief Disconnect from all brokers.
    ss::future<> stop();

    /// \brief Dispatch a request to any broker.
    template<typename Func>
    CONCEPT(
      requires(typename std::invoke_result_t<Func>::api_type::response_type))
    ss::future<typename std::invoke_result_t<
      Func>::api_type::response_type> dispatch(Func func) {
        return ss::with_gate(_gate, [this, func{std::move(func)}]() {
            return retry_with_mitigation(
              shard_local_cfg().retries(),
              shard_local_cfg().retry_base_backoff(),
              [this, func{std::move(func)}]() {
                  _gate.check();
                  return _brokers.any().then(
                    [func{std::move(func)}](shared_broker_t broker) mutable {
                        return broker->dispatch(func());
                    });
              },
              [this](std::exception_ptr ex) {
                  return mitigate_error(std::move(ex));
              });
        });
    }

    ss::future<kafka::produce_response::partition> produce_record_batch(
      model::topic_partition tp, model::record_batch&& batch);

    ss::future<kafka::fetch_response::partition> fetch_partition(
      model::topic_partition tp,
      model::offset offset,
      int32_t max_bytes,
      std::chrono::milliseconds timeout);

private:
    /// \brief Connect and update metdata.
    ss::future<> do_connect(unresolved_address addr);

    /// \brief Update metadata
    ///
    /// If an existing update is in progress, the future returned will be
    /// satisfied by the outstanding request.
    ///
    /// Uses round-robin load-balancing strategy.
    ss::future<> update_metadata(wait_or_start::tag);

    /// \brief Handle errors by performing an action that may fix the cause of
    /// the error
    ss::future<> mitigate_error(std::exception_ptr ex);

    /// \brief Seeds are used when no brokers are connected.
    std::vector<unresolved_address> _seeds;
    /// \brief Broker lookup from topic_partition.
    brokers _brokers;
    /// \brief Update metadata, or wait for an existing one.
    wait_or_start _wait_or_start_update_metadata;
    /// \brief Batching producer.
    producer _producer;
    /// \brief Wait for retries.
    ss::gate _gate;
};

} // namespace pandaproxy::client
