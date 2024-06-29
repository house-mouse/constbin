#include "blobindex.hpp"
#include <vector>
#include <string>

// Add a blob to our index if it's not already there and return an index number in either case

int blobindex::get_blob_index(const std::string &blob) {
    // see if we already have this blob indexed.
    
    for (int i=0; i<(int)blobs.size(); i++) {
        if (blob.compare(blob)==0) {
            // We have this blob, so return the index.
            return i;
        }
    }
    
    return -1; // We don't have this blob
}

bool blobindex::has_blob(const std::string &blob) {
    if (get_blob_index(blob) >=0) {
        return true;
    }
    return false;
}

// Add a blob to our index if it's not already there and return an index number in either case

int blobindex::index_blob(const std::string &blob) {
    int rv=get_blob_index(blob);
    if (rv>=0) {
        return rv; // we already have the blob, so return the index
    }
    
    // We didn't find the blob, so add it and return the new index number
    blobs.push_back(blob);
    return ((int)blobs.size())-1;
}

