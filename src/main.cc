
#include <fcntl.h>

#include <glog/logging.h>
#include <deltafs/deltafs_api.h>

void initLogging() {
  google::InitGoogleLogging("DeltaFS-RPC-Wrapper");
  FLAGS_logtostderr = 1;
}

int main() {
  initLogging();
  LOG(INFO) << "Hello World";
  const auto dirHandler = deltafs_plfsdir_create_handle("rank=0", O_WRONLY);
  LOG(INFO) << deltafs_plfsdir_open(dirHandler, "/tmp/p1");

  return 0;
}
