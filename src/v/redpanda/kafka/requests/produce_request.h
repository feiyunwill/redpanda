#pragma once

#include "redpanda/kafka/requests/headers.h"
#include "redpanda/kafka/requests/request_context.h"
#include "redpanda/kafka/requests/response.h"
#include "seastarx.h"

#include <seastar/core/future.hh>

namespace kafka::requests {

class produce_request final {
public:
    static constexpr const char* name = "produce";
    static constexpr api_key key = api_key(0);
    // We don't support the obsolete Kafka record format
    static constexpr api_version min_supported = api_version(3);
    static constexpr api_version max_supported = api_version(7);

    static future<response_ptr>
    process(request_context&&, seastar::smp_service_group);
};

} // namespace kafka::requests