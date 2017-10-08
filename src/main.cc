#include <fcntl.h>

#include <glog/logging.h>
#include <deltafs/deltafs_api.h>

#include "DeltaFSKVStore.h"
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TSimpleServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TBufferTransports.h>

void initLogging() {
  google::InitGoogleLogging("DeltaFS-RPC-Wrapper");
  FLAGS_logtostderr = 1;
}

using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;
using namespace ::apache::thrift::server;

using boost::shared_ptr;

using namespace  ::deltafs;

class DeltaFSKVStoreHandler : virtual public DeltaFSKVStoreIf {
  public:
  DeltaFSKVStoreHandler() {
    // Your initialization goes here
  }

  void append(const std::string& mdName, const std::string& key, const std::string& value) {
    // Your implementation goes here
    printf("append\n");
  }

  void get(std::string& _return, const std::string& mdName, const std::string& key) {
    // Your implementation goes here
    printf("get\n");
  }
};

int main(int argc, char **argv) {
  initLogging();
  LOG(INFO) << "Hello World";
  const auto dirHandler = deltafs_plfsdir_create_handle("rank=0", O_WRONLY);
  LOG(INFO) << deltafs_plfsdir_open(dirHandler, "/tmp/p1");

  int port = 9090;
  shared_ptr<DeltaFSKVStoreHandler> handler(new DeltaFSKVStoreHandler());
  shared_ptr<TProcessor> processor(new DeltaFSKVStoreProcessor(handler));
  shared_ptr<TServerTransport> serverTransport(new TServerSocket(port));
  shared_ptr<TTransportFactory> transportFactory(new TBufferedTransportFactory());
  shared_ptr<TProtocolFactory> protocolFactory(new TBinaryProtocolFactory());

  TSimpleServer server(processor, serverTransport, transportFactory, protocolFactory);
  server.serve();
  return 0;
}
