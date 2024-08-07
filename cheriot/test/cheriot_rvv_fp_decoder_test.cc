#include "cheriot/cheriot_rvv_fp_decoder.h"

#include "absl/log/log_sink_registry.h"
#include "googlemock/include/gmock/gmock.h"
#include "mpact/sim/util/other/log_sink.h"

namespace {

using ::mpact::sim::cheriot::CheriotRVVFPDecoder;
using ::mpact::sim::util::LogSink;

TEST(RiscvCheriotRvvFpDecoderTest, Instantiation) {
  LogSink log_sink;
  absl::AddLogSink(&log_sink);
  CheriotRVVFPDecoder decoder(nullptr, nullptr);
  EXPECT_EQ(log_sink.num_error(), 0);
  absl::RemoveLogSink(&log_sink);
}

}  // namespace
