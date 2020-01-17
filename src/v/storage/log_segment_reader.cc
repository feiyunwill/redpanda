#include "storage/log_segment_reader.h"

#include <seastar/core/fstream.hh>
#include <seastar/core/iostream.hh>
#include <seastar/core/sstring.hh>

namespace storage {

log_segment_reader::log_segment_reader(
  ss::sstring filename,
  ss::file data_file,
  model::term_id term,
  model::offset base_offset,
  uint64_t file_size,
  size_t buffer_size) noexcept
  : _filename(std::move(filename))
  , _data_file(std::move(data_file))
  , _term(term)
  , _base_offset(base_offset)
  , _file_size(file_size)
  , _buffer_size(buffer_size) {}

ss::input_stream<char>
log_segment_reader::data_stream(uint64_t pos, const ss::io_priority_class& pc) {
    ss::file_input_stream_options options;
    options.buffer_size = _buffer_size;
    options.io_priority_class = pc;
    options.read_ahead = 4;
    options.dynamic_adjustments = _history;
    return make_file_input_stream(
      _data_file,
      pos,
      std::min(_file_size, _file_size - pos),
      std::move(options));
}

std::ostream& operator<<(std::ostream& os, const log_segment_reader& seg) {
    return os << "{log_segment:" << seg.get_filename() << ", "
              << seg.base_offset() << "-" << seg.max_offset() << "}";
}

std::ostream& operator<<(std::ostream& os, segment_reader_ptr seg) {
    if (seg) {
        return os << *seg;
    }
    return os << "{{log_segment: null}}";
}
} // namespace storage