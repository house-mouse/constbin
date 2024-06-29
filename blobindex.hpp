#ifndef __BLOBINDEX_HPP__
#define __BLOBINDEX_HPP__

#include <vector>
#include <string>

// This class manages blobs, dedupping identical blobs.
class blobindex {
    std::vector<std::string> blobs;
public:
    int  index_blob(const std::string &blob);
    int  get_blob_index(const std::string &blob);
    bool has_blob(const std::string &blob);
};


#endif // __BLOBINDEX_HPP__
