#include <thread>
#include <chrono>

#include <glog/logging.h>
#include <gflags/gflags.h>

DECLARE_int32(num_take_in_mb);
DEFINE_int32(num_take_in_mb, 10, "The size of memory baloon in MB");

int main(int argc, char **argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging("Memory bomb");
  FLAGS_logtostderr = 1;

  auto sz = FLAGS_num_take_in_mb * 1024 * 1024 / sizeof(int);
  auto q = new int[sz];
  for (auto i = 0; i < sz; i++) q[i] = i;
  LOG(INFO) << "Fork bomb created, size = " << FLAGS_num_take_in_mb;
  while (true) {
    std::this_thread::sleep_for(std::chrono::seconds(5));
    q[0] = 0;
  }
  return q[0];
}
