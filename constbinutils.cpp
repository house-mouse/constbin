#include "constbinutils.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <map>
#include <dirent.h>
#include <filesystem>

#include <string>       // std::string
#include <iostream>     // std::cout
#include <sstream>      // std::stringstream, std::stringbuf


//
// file_to_string
//
// Read a file into a std::string
//
// read the file 'filename' and return it in the provided 'filestring' argument.
//
// If add_terminator is true, add a null terminator to the string and include that in the
// string length
//
// Returns 0 on success
// Returns -1 is unable to open the file for reading
// Returns -2 if the file is not able to be read
//

int file_to_string(std::string &filestring, const std::filesystem::path &path, bool add_terminator) {
    FILE * f = fopen(path.c_str(), "rb");
    if (!f) {
        fprintf(stderr, "Unable to open file: %s\n", path.c_str());
        return -1;
    }
    fseek(f, 0, SEEK_END);
    size_t size = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    fprintf(stderr, "%s is %ld bytes long\n", path.c_str(), (long)size);

    filestring.resize(size + (add_terminator ? 1 : 0));

    char *buf = (char *)filestring.c_str();
    if (fread(buf, size, 1, f) != 1) {
        fprintf(stderr, "Failed to read %s!\n", path.c_str());
        fclose(f);
        return -2;
    }
    fclose(f);

    if (add_terminator) {
        buf[size]=0;
    }
    return 0;
}

//
// One of our goals is to play well with build systems like make and cmake which
// may look at modification date/times to try to tell if the contents of a file have
// changed.  This function writes the data to the filename, but only if it is not
// able to read the identical data from the file first...

int write_to_file_if_different(const std::filesystem::path &path, const std::string &data) {
    std::string data_in_file;
    if (!file_to_string(data_in_file, path, false)) {
        // No error, so we were able to read some data...
        if (data.compare(data_in_file) == 0) {
            // Data in the file is the same as the data in data... do nothing..
            return 0;
        }
    }

    FILE * out = fopen(path.c_str(), "wb");
    if (NULL == out) {
        return -1; // unable to open file for writing... check errno for more info...
    }

    size_t wv = fwrite(data.c_str(), data.size(), 1, out);
    fclose(out); // success or not, close the file...

    if (1 == wv) { // expect to write 1 record of data.size() length...
        return 0; // success
    }
    
    return -2; // unable to write data... check errno for more info...
}

std::string trim(const std::string& str,
                 const std::string& whitespace) {
    const auto strBegin = str.find_first_not_of(whitespace);
    if (strBegin == std::string::npos)
        return ""; // no content

    const auto strEnd = str.find_last_not_of(whitespace);
    const auto strRange = strEnd - strBegin + 1;

    return str.substr(strBegin, strRange);
}

//
// string_to_hex
//
// Convert the string 'input' into hex values suitable for including in a C file.
//
// Each line generated can be prefixed with a line_prefix, and each line will contain
// at most bytes-per_line ehx digits.
//

void string_to_hex(std::string &hex, const std::string &input, const std::string &line_prefix, int bytes_per_line) {

    size_t max_size = (input.size() / bytes_per_line + 1) *  (5 * bytes_per_line + line_prefix.size() + 1);

    hex.resize(max_size);

    char       *out = (char *)hex.c_str();
    const char *in  = input.c_str();
    const char *end = in + input.size();

    while(in < end) {
        memcpy(out, line_prefix.c_str(), line_prefix.size());
        out += line_prefix.size();
        int items=(int)(end - in);
        if (items > bytes_per_line) {
             items = bytes_per_line;
        }

        for (int i=0; i<items; i++) {

            (*out++)='0';
            (*out++)='x';
            unsigned char c=*in++;
        
            (*out++)=((c >> 4)  < 10)  ?  '0' + (c >> 4)  : 'a' - 10 + (c >> 4);
            (*out++)=((c & 0xf) < 10)  ?  '0' + (c & 0xf) : 'a' - 10 + (c & 0xf);
            (*out++)=',';
        } 
        (*out++)='\n';
    }

    hex.resize(out - hex.c_str());
}


// Crush any characters that wouldn't be appropriate...
void escape_string(std::string &s, const char *to_escape, char escape) {
    for (unsigned int i=0; i<s.size(); i++) {
        if (strchr(to_escape, s[i])) {
            s[i]=escape;
        }
    }
}

int recurse_directory(std::vector<std::string> &paths, const std::string directory) {
    DIR *dir = opendir(directory.c_str());
    if (!dir) {
        if (ENOTDIR != errno) {
            return -1;
        }
        return -2;
    }
    int r = 0;

    for (const struct dirent *f = readdir(dir); f; f = readdir(dir)) {
        if (strcmp(f->d_name, ".") == 0 || strcmp(f->d_name, "..") == 0)
            continue;
        std::string filename=directory+"/"+f->d_name;
        
        if (DT_DIR == f->d_type || DT_UNKNOWN == f->d_type) {
            recurse_directory(paths, filename);
        } else {
            paths.push_back(filename);
        }
    }
    if (errno) {  // from readdir
        r = -3;
    }
    if (closedir(dir)) {
        r = -4;
    }
    return r;
}


int recurse_directory_with_virtual(std::vector<std::pair<std::string, std::string> > &paths, const std::string directory, const std::string virtual_directory) {
    DIR *dir = opendir(directory.c_str());
    if (!dir) {
        if (ENOTDIR != errno) {
            return -1;
        }
        return -2;
    }
    int r = 0;

    for (const struct dirent *f = readdir(dir); f; f = readdir(dir)) {
        if (strcmp(f->d_name, ".") == 0 || strcmp(f->d_name, "..") == 0)
            continue;
        std::string filename=directory+"/"+f->d_name;
        std::string vitual_filename=virtual_directory+"/"+f->d_name;

        if (DT_DIR == f->d_type || DT_UNKNOWN == f->d_type) {
            recurse_directory_with_virtual(paths, filename, vitual_filename);
        } else {
            paths.push_back(std::pair<std::string, std::string>(filename, vitual_filename));
        }
    }
    if (errno) {  // from readdir
        r = -3;
    }
    if (closedir(dir)) {
        r = -4;
    }
    return r;
}

