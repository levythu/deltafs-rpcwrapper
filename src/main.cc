#include <fcntl.h>
#include <map>

#include <glog/logging.h>
#include <gflags/gflags.h>
#include <deltafs/deltafs_api.h>

#include "DeltaFSKVStore.h"
#include "flags.h"

#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TSimpleServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/server/TThreadPoolServer.h>
#include <thrift/concurrency/ThreadManager.h>
#include <thrift/concurrency/PlatformThreadFactory.h>

using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;
using namespace ::apache::thrift::concurrency;
using namespace ::apache::thrift::server;

using boost::shared_ptr;

using namespace ::deltafs;

class DeltaFSKVStoreHandler : virtual public DeltaFSKVStoreIf {
 private:
  const std::map<std::string, deltafs_plfsdir_t*>& dirHandleMap_;

  static OperationFailure err(const std::string& reason) {
    OperationFailure ex;
    ex.error = reason;
    return ex;
  }
 public:
  DeltaFSKVStoreHandler(
      const std::map<std::string, deltafs_plfsdir_t*>& dirHandleMap):
        dirHandleMap_(dirHandleMap) {
    // NOTHING
  }

  void append(const std::string& mdName,
              const std::string& key,
              const std::string& value) {
    if (dirHandleMap_.find(mdName) == dirHandleMap_.end()) {
      throw err("MdName must be in the valid set.");
    }
    auto rc = deltafs_plfsdir_append(dirHandleMap_.at(mdName), key.c_str(), -1,
        value.c_str(), value.length());
    if (rc < 0) {
      throw err("deltafs_plfsdir_append returns -1");
    }
    LOG(INFO) << "Appended " << value << "to " << mdName << "/" << key;
  }

  void get(std::string& _return, const std::string& mdName, const std::string& key) {
    if (dirHandleMap_.find(mdName) == dirHandleMap_.end()) {
      throw err("MdName must be in the valid set.");
    }
    _return = "hmmmm";
  }
};

void initLogging() {
  google::InitGoogleLogging("DeltaFS-RPC-Wrapper");
  FLAGS_logtostderr = 1;
}

deltafs_plfsdir_t*
openDirectory(const std::string& dirName, const bool readMode) {
  auto dirHandler = deltafs_plfsdir_create_handle("rank=0",
      readMode ? O_RDONLY : O_WRONLY);
  auto dirAbsName = std::string("/tmp/") + dirName;
  auto res = deltafs_plfsdir_open(dirHandler, dirAbsName.c_str());
  LOG(INFO) << "Open directory " << dirName << ", rc = " << res;
  return dirHandler;
}

int main(int argc, char **argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  initLogging();
  std::set<std::string> validMdName {"traces", "services"};
  std::map<std::string, deltafs_plfsdir_t*> dirHandleMap;
  for (const auto& name : validMdName) {
    dirHandleMap[name] = openDirectory(name, FLAGS_readMode);
  }

  auto workerCount = 20;
  shared_ptr<ThreadManager> threadManager =
      ThreadManager::newSimpleThreadManager(workerCount);
  shared_ptr<ThreadFactory> threadFactory(new PlatformThreadFactory());
  threadManager->threadFactory(threadFactory);
  threadManager->start();

  int port = 9090;
  shared_ptr<DeltaFSKVStoreHandler> handler(
      new DeltaFSKVStoreHandler(dirHandleMap));
  shared_ptr<TProcessor> processor(new DeltaFSKVStoreProcessor(handler));
  shared_ptr<TServerTransport> serverTransport(new TServerSocket(port));
  shared_ptr<TTransportFactory> transportFactory(
      new TBufferedTransportFactory());
  shared_ptr<TProtocolFactory> protocolFactory(new TBinaryProtocolFactory());

  TThreadPoolServer server(
      processor,
      serverTransport,
      transportFactory,
      protocolFactory,
      threadManager);
  LOG(INFO) << "Start RPC Server...";
  server.serve();
  return 0;
}
