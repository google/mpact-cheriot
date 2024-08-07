#include "cheriot/cheriot_rvv_decoder.h"

#include "absl/log/log_sink_registry.h"
#include "googlemock/include/gmock/gmock.h"
#include "mpact/sim/util/other/log_sink.h"

namespace {

using ::mpact::sim::cheriot::CheriotRVVDecoder;
using ::mpact::sim::util::LogSink;

TEST(RiscvCheriotRvvDecoderTest, Instantiation) {
  LogSink log_sink;
  absl::AddLogSink(&log_sink);
  CheriotRVVDecoder decoder(nullptr, nullptr);
  EXPECT_EQ(log_sink.num_error(), 0);
  absl::RemoveLogSink(&log_sink);
}

}  // namespace
