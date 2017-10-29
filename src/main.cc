#include <fcntl.h>
#include <map>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <vector>
#include <csignal>

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

const int FLUSH_TICK_IN_S = 5;
std::map<std::string, bool> validMdName {
  {"traces", false},
  {"services", true}
};

class DeltaFSKVStoreHandler : virtual public DeltaFSKVStoreIf {
 private:
  struct MdCache {
    std::set<std::string> allVal;
    std::set<std::string> valToFlush;
  };

 private:
  const std::map<std::string, deltafs_plfsdir_t*>& dirHandleMap_;

   // MdName, key -> value
  std::map<std::pair<std::string, std::string>, MdCache> cache_;
  std::mutex cacheLock;
  std::atomic<bool> stopFlush;
  std::thread flushThread;

  static OperationFailure err(const std::string& reason) {
    OperationFailure ex;
    ex.error = reason;
    return ex;
  }
 public:
  DeltaFSKVStoreHandler(
      const std::map<std::string, deltafs_plfsdir_t*>& dirHandleMap):
        dirHandleMap_(dirHandleMap) {
    if (!FLAGS_readMode) {
      stopFlush = false;
      flushThread = std::thread([&] {
        this->onFlushTick();
      });
    }
  }

  ~DeltaFSKVStoreHandler() {
    if (!FLAGS_readMode) {
      stopFlush = true;
      flushThread.join();
    }
  }

  void onFlushTick() {
    while (true) {
      std::this_thread::sleep_for(std::chrono::seconds(FLUSH_TICK_IN_S));
      if (stopFlush) return;
      cacheFlush();
    }
  }

  void cacheFlush() {
    std::lock_guard<std::mutex> g(cacheLock);
    for (auto& kvpair : cache_) {
      auto& cacheVal = kvpair.second;
      const auto& mdName = kvpair.first.first;
      const auto& key = kvpair.first.second;
      if (!cacheVal.valToFlush.empty()) {
        int succ = 0;
        for (const auto& value : cacheVal.valToFlush) {
          auto rc = deltafs_plfsdir_append(
              dirHandleMap_.at(mdName), key.c_str(), -1,
              value.c_str(), value.length());
          if (rc >= 0) succ++;
        }
        LOG(INFO) << "Flushed " << mdName+"/"+key << ", success: "
                  << succ << "/" << cacheVal.valToFlush.size();
        cacheVal.valToFlush.clear();
      }
    }
  }

  void appendBatch(const std::vector<std::string>& mdName,
                   const std::vector<std::string>& key,
                   const std::vector<std::string>& value) {
    if (mdName.size() != key.size() || mdName.size() != value.size()) {
      throw err("Batch size not equal.");
    }
    for (auto i = 0; i < mdName.size(); i++) {
      append(mdName[i], key[i], value[i]);
    }
  }

  void append(const std::string& mdName,
              const std::string& key,
              const std::string& value) {
    if (FLAGS_readMode) {
      throw err("RPCWrapper is in readmode. Cannot write.");
    }
    LOG(INFO) << "Request: append. mdName=" << mdName
              << ", key=" << key << ", value=" << value;
    if (dirHandleMap_.find(mdName) == dirHandleMap_.end()) {
      throw err("MdName must be in the valid set.");
    }
    if (validMdName.at(mdName)) {
      // Cache mode
      std::lock_guard<std::mutex> g(cacheLock);
      auto& cacheEntry = cache_[std::make_pair(mdName, key)];
      if (cacheEntry.allVal.find(value) == cacheEntry.allVal.end()) {
        cacheEntry.allVal.insert(value);
        cacheEntry.valToFlush.insert(value);
      }
      LOG(INFO) << "Cached " << value << "to " << mdName << "/" << key;
    } else {
      auto rc = deltafs_plfsdir_append(dirHandleMap_.at(mdName), key.c_str(), -1,
          value.c_str(), value.length());
      if (rc < 0) {
        throw err("deltafs_plfsdir_append returns -1");
      }
      LOG(INFO) << "Appended " << value << "to " << mdName << "/" << key;
    }
  }

  void get(std::string& _return, const std::string& mdName, const std::string& key) {
    if (!FLAGS_readMode) {
      throw err("RPCWrapper is in writemode. Cannot read.");
    }
    LOG(INFO) << "Request: get. mdName=" << mdName
              << ", key=" << key;
    if (dirHandleMap_.find(mdName) == dirHandleMap_.end()) {
      throw err("MdName must be in the valid set.");
    }
    if (validMdName.at(mdName)) {
      // Cache mode
      std::lock_guard<std::mutex> g(cacheLock);
      auto cacheIter = cache_.find(std::make_pair(mdName, key));
      if (cacheIter != cache_.end()) {
        _return = "";
        for (const auto& value : cacheIter->second.allVal) {
          _return.append(value);
        }
        LOG(INFO) << "GET from Cache: " << mdName << "/" << key
                  << "=" << _return;
        return;
      }
    }
    size_t valLen;
    auto charStr = deltafs_plfsdir_get(dirHandleMap_.at(mdName), key.c_str(),
        key.length(), &valLen, NULL, NULL);
    if (charStr == NULL) {
      LOG(INFO) << "GET: " << mdName << "/" << key << "not found.";
      return;
    }
    LOG(INFO) << valLen << (uint64_t)charStr << std::string(charStr);
    _return = std::string(charStr, valLen);
    free(charStr);
    if (!_return.empty() && validMdName.at(mdName)) {
      std::lock_guard<std::mutex> g(cacheLock);
      cache_[std::make_pair(mdName, key)] = MdCache();
      cache_[std::make_pair(mdName, key)].allVal.insert(_return);
    }
    LOG(INFO) << "GET: " << mdName << "/" << key << "=" << _return;
  }
};

void initLogging() {
  google::InitGoogleLogging("DeltaFS-RPC-Wrapper");
  FLAGS_logtostderr = 1;
}

deltafs_plfsdir_t*
openDirectory(const std::string& dirName, const bool readMode) {
  auto dirAbsName = std::string("/tmp/") + dirName;
  if (FLAGS_conversion && readMode) {
    // Force converting the massive directory from write mode to read
    auto dirHandler = deltafs_plfsdir_create_handle("rank=0", O_WRONLY);
    auto res = deltafs_plfsdir_open(dirHandler, dirAbsName.c_str());
    LOG(INFO) << "Open directory " << dirName << ", rc = " << res
              << "And trying to convert that into read mode";
    deltafs_plfsdir_finish(dirHandler);
    deltafs_plfsdir_free_handle(dirHandler);
  }
  auto dirHandler = deltafs_plfsdir_create_handle("rank=0",
      readMode ? O_RDONLY : O_WRONLY);
  auto res = deltafs_plfsdir_open(dirHandler, dirAbsName.c_str());
  LOG(INFO) << "Open directory " << dirName << ", rc = " << res;
  return dirHandler;
}

static std::map<std::string, deltafs_plfsdir_t*> dirHandleMap;

void signal_handler(int signal) {
  if (!FLAGS_readMode) {
    LOG(INFO) << "Flushing all md...";
    for (const auto& name : validMdName) {
      deltafs_plfsdir_finish(dirHandleMap[name.first]);
      deltafs_plfsdir_free_handle(dirHandleMap[name.first]);
    }
  }
  exit(0);
}

int main(int argc, char **argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  initLogging();

  for (const auto& name : validMdName) {
    dirHandleMap[name.first] = openDirectory(name.first, FLAGS_readMode);
  }
  std::signal(SIGINT, signal_handler);

  //===========================================================================

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
