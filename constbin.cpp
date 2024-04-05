#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <dirent.h>

#include <string>       // std::string
#include <iostream>     // std::cout
#include <sstream>      // std::stringstream, std::stringbuf

// There are tons of progams to do this but I've found all I've tried lacking...
// https://www.devever.net/~hl/incbin
// https://gist.github.com/sivachandran/3a0de157dccef822a230
//
// I really want cmake to "do the right thing" with dependencies, but it doesn't
// with a cmake script (that runs at config time not build time) 
// xxd would be a strong contender... but it's not universally available and there
// are limitations on naming schemes that I don't love.
//
// Unlike most tools that do this, I'm pretty sure I want unique .h and .c outputs.
//


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

int file_to_string(std::string &filestring, const char *filename, bool add_terminator=false) {
    FILE * f = fopen(filename, "rb");
    if (!f) {
//        fprintf(stderr, "Unable to open file: %s\n", filename);
        return -1;
    }
    fseek(f, 0, SEEK_END);
    size_t size = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    fprintf(stderr, "%s is %ld bytes long\n", filename, (long)size);

    filestring.resize(size + (add_terminator ? 1 : 0));

    char *buf = (char *)filestring.c_str();
    if (fread(buf, size, 1, f) != 1) {
//        fprintf(stderr, "Failed to read %s!\n", filename);
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

int write_to_file_if_different(const char *filename, const std::string &data) {
    std::string data_in_file;
    if (!file_to_string(data_in_file, filename, false)) {
        // No error, so we were able to read some data...
        if (data.compare(data_in_file) == 0) {
            // Data in the file is the same as the data in data... do nothing..
            return 0;
        }
    }

    FILE * out = fopen(filename, "wb");
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


int add_c_file(std::stringstream &out, const char *label, const char *filename, std::string &raw, std::string line_prefix="  ", int bytes_per_line=20) {
    std::string hex;
    
    string_to_hex(hex, raw, line_prefix, bytes_per_line);

    out << label << "[" << raw.size() << "] = {\n" << hex << "};\n";
    
    return 0;
}

struct parts {
    std::string filename;
    std::string label;
    std::string raw;
};


void add_parts_to_c(std::stringstream &out, std::vector<parts> &parts) {
    for (auto &part:parts) {
        add_c_file(out, part.label.c_str(), part.filename.c_str(), part.raw, "  ", 20);
    }
}

void add_parts_to_h(std::stringstream &out, std::vector<parts> &parts) {
    for (auto &part:parts) {
        out << "extern " << part.label << "[" << part.raw.size() << "];\n";
    }
}

// Crush any characters that wouldn't be approrpiate for a C name...
void escape_string(std::string &s) {
    for (unsigned int i=0; i<s.size(); i++) {
        switch(s[i]) {
            case '/':
            case '.':
            case '\'':
            case '"':
            case '\\':
            case ' ':
                s[i]='_';
                break;
            default :
                break;
        }
    }
}

int generate_header(std::stringstream &out, std::vector<parts> &parts, const char *filename) {
    std::string escape(filename);
    escape_string(escape);

// It'd be cool if #pragma once always worked.. but... embedded systems... hmm.
//
    out << "#ifndef __" << escape << "__\n#define __" << escape << "__\n\n";

    add_parts_to_h(out, parts);
    
    out << "\n#endif // __" << escape << "__\n\n";
    
    return write_to_file_if_different(filename, out.str());

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


int main(int argc, char * argv[]) {

    int rv=0;
    
    const char *output_c_file=argv[1];
    const char *output_h_file=argv[2];

    if (argc<4) {
        fprintf(stderr, "Usage:\n");
        fprintf(stderr, "    %s <output_c_file> <output_h_file> [files_to_encode] [<--r <directory to recurse> >]\n", argv[0]);
        exit(-1);
    }
    std::vector<std::string> paths;

    // Use the command line options to create a list of all the paths we care about
    for (int i=3; i<argc; i++) {
        const char *arg=argv[i];

        if (0==strncmp(arg, "--", 2)) {
            if (strlen(arg)>2) {
                if (arg[2]=='r') { // recursively add directory
                    if (argc<i+1) {
                        fprintf(stderr, "Expected but did not find directory name after --r option\n");
                        exit(-2);
                    }
                    
                    i++; // advance to directory
                    rv=recurse_directory(paths, argv[i]);
                    
                }
            }
        } else {
            paths.push_back(arg);
        }
    }
    
    std::vector<parts> file_parts;

    for (auto &filename:paths) {
        struct parts p;
        p.filename=filename;
        std::string file(p.filename);
        escape_string(file);
        p.label="unsigned char " + file;
        
        file_to_string(p.raw, filename.c_str(), true);
        file_parts.push_back(p);
    }


    std::stringstream c_out;
    add_parts_to_c(c_out, file_parts);
    rv=write_to_file_if_different(output_c_file, c_out.str());
    if (rv!=0) {
        std::cerr << "Unable to output data to file " << output_c_file;
    }

    std::stringstream h_out;
    rv = generate_header(h_out, file_parts, output_h_file);

    if (rv!=0) {
        std::cerr << "Unable to output data to file " << output_h_file;
    }
    
    return rv;
}


