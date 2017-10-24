namespace cpp deltafs
namespace java deltafs

exception OperationFailure {
    1: string error
}

service DeltaFSKVStore {

    void append(1:string mdName, 2:string key, 3:string value),

    void appendBatch(1:list<string> mdName, 2:list<string> key, 3:list<string> value),

    string get(1:string mdName, 2:string key)

}
