#pragma once

#include "hashing/crc32c.h"
#include "model/fundamental.h"
#include "model/record.h"
#include "model/record_batch_reader.h"
#include "storage/constants.h"
#include "storage/crc_record.h"
#include "utils/fragbuf.h"
#include "utils/vint.h"

#include <random>

namespace storage::test {

namespace internal {
size_t vint_size(size_t val) {
    std::array<bytes::value_type, vint::max_length> encoding_buffer;
    return vint::serialize(val, encoding_buffer.begin());
}
} // namespace internal

inline std::random_device::result_type get_seed() {
    std::random_device rd;
    auto seed = rd();
    std::cout << "storage::random_batch seed = " << seed << "\n";
    return seed;
}

inline std::default_random_engine gen(get_seed());

inline std::uniform_int_distribution<int> bool_dist{0, 1};
inline std::uniform_int_distribution<int> low_count_dist{2, 30};
inline std::uniform_int_distribution<int> high_count_dist{1024, 4096};
inline std::uniform_int_distribution<model::timestamp::value_type>
  timestamp_dist{model::timestamp::min().value(),
                 model::timestamp::min().value() + 2};

model::record_batch_header
make_random_header(model::offset o, model::timestamp ts, size_t num_records) {
    model::record_batch_header h;
    h.base_offset = o;
    h.last_offset_delta = num_records;
    h.first_timestamp = ts;
    h.max_timestamp = model::timestamp(ts.value() + num_records);
    if (bool_dist(gen)) {
        h.attrs = model::record_batch_attributes(4);
    }
    return h;
}

temporary_buffer<char> make_buffer(size_t blob_size) {
    static thread_local std::
      independent_bits_engine<std::default_random_engine, 8, uint8_t>
        random_bytes;
    temporary_buffer<char> blob(blob_size);
    auto* out = blob.get_write();
    for (unsigned i = 0; i < blob_size; ++i) {
        *out++ = random_bytes();
    }
    return blob;
}

fragbuf make_random_ftb(size_t blob_size) {
    auto first_chunk = blob_size / 2;
    auto second_chunk = blob_size - first_chunk;
    std::vector<temporary_buffer<char>> bufs;
    bufs.push_back(make_buffer(first_chunk));
    bufs.push_back(make_buffer(second_chunk));
    return fragbuf(std::move(bufs), first_chunk + second_chunk);
}

model::record make_random_record(unsigned index) {
    auto k = make_random_ftb(high_count_dist(gen));
    auto v = make_random_ftb(high_count_dist(gen));
    auto size = internal::vint_size(k.size_bytes()) + k.size_bytes()
                + v.size_bytes() + internal::vint_size(index) * 2 /* deltas */;
    return model::record(size, index, index, std::move(k), std::move(v));
}

model::record_batch make_random_batch(model::offset o) {
    crc32 crc;
    auto num_records = low_count_dist(gen);
    auto ts = model::timestamp(timestamp_dist(gen));
    auto header = make_random_header(o, ts, num_records);
    storage::crc_batch_header(crc, header, num_records);
    auto size = packed_header_size;
    model::record_batch::records_type records;
    if (header.attrs.compression() != model::compression::none) {
        auto blob = make_random_ftb(high_count_dist(gen));
        size += blob.size_bytes();
        auto cr = model::record_batch::compressed_records(
          num_records, std::move(blob));
        crc.extend(cr.records());
        records = std::move(cr);
    } else {
        auto rs = model::record_batch::uncompressed_records();
        for (unsigned i = 0; i < num_records; ++i) {
            auto r = make_random_record(i);
            size += r.size_bytes();
            size += internal::vint_size(r.size_bytes());
            storage::crc_record_header_and_key(
              crc,
              r.size_bytes(),
              r.timestamp_delta(),
              r.offset_delta(),
              r.key());
            crc.extend(r.packed_value_and_headers());
            rs.push_back(std::move(r));
        }
        records = std::move(rs);
    }
    header.size_bytes = size;
    header.crc = crc.value();
    return model::record_batch(std::move(header), std::move(records));
}

std::vector<model::record_batch>
make_random_batches(model::offset o, size_t count) { // start offset + count
    std::vector<model::record_batch> ret;
    ret.reserve(count);
    for (size_t i = 0; i < count; i++) {
        auto b = make_random_batch(o);
        o = b.last_offset() + 1;
        ret.push_back(std::move(b));
    }
    return ret;
}

std::vector<model::record_batch>
make_random_batches(model::offset o = model::offset(0)) {
    return make_random_batches(o, low_count_dist(gen));
}

} // namespace storage::test