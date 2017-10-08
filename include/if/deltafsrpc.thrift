namespace cpp deltafs
namespace java deltafs

service DeltaFSKVStore {

   void append(1:string mdName, 2:string key, 3:string value),

   string get(1:string mdName, 2:string key)

}
