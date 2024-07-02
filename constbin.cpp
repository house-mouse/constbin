#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <map>
#include <dirent.h>
#include <filesystem>

#include <string>       // std::string
#include <iostream>     // std::cout
#include <sstream>      // std::stringstream, std::stringbuf

#include "constbinutils.hpp"
#include "blobindex.hpp"

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



int add_c_file(std::stringstream &out, const char *label, const char *filename, std::string &raw, std::string line_prefix="  ", int bytes_per_line=20) {
    std::string hex;
    
    string_to_hex(hex, raw, line_prefix, bytes_per_line);

    out << label << "[" << raw.size() << "] = {\n" << hex << "};\n";
    
    return 0;
}

struct part {
    std::string filename;
    std::string label;
    std::string raw;
};

#define CONSTBIN_LITERAL  1  // Just a straight string with length
#define CONSTBIN_RECURSE  2  // A recursive reference to another ssi_line which should be included
#define CONSTBIN_VARIABLE 3  // Request for a variable to be inserted.  *start is a pointer to the name, length a uid


struct ssi_line {
    int blob_index;    // index number of the blob
    int command;       // command as specified above
    int next;          // ssi line that follows this one.  0 for none.
    int max_recursion; // the number of recursions this ssi uses
};

// This class contains structures used to index and parse server side includes into
// a sort of primitive language that allows for outputting of pre-processed server side
// includes.

class constindex {
    blobindex                  blobs;
    blobindex                  variables;
    std::map<std::string, int> ssi_index;   // map from ssi string to ssi index

    std::vector<ssi_line>      ssi_lines;
    
public:
//    int file_to_string(std::string &filestring, const std::filesystem::path &path, bool add_terminator=false);
    int parse_ssi(const std::string &input, const std::filesystem::path &path, int start=0);
    int add_file(const std::filesystem::path &path, bool parse_ssi=false, bool add_terminator=false);

};

// Look for Server Side Include comments...
// They look like
// <!--# tag -->
// If the tag starts with '@' then it includes the file with the given name
// otherwise it includes a variable with the tag name
//
// returns the SSI index for this object...


int constindex::parse_ssi(const std::string &input, const std::filesystem::path &path, int start) {
    if (0 == input.size()) {
        return -1;
    }
    
    auto index=ssi_index.find(input);
    if (index!=ssi_index.end()) {
        // we have already parsed this input...
        return index->second;   // return index
    }

    size_t part_end = input.size();
    int next = -1;
    
    size_t ssi_start=input.find("<!--#");
    std::cerr << "SEARCH " << ssi_start << "  " << input << "\n";
    if (std::string::npos != ssi_start) {
        // We found the start of a SSI line at offset ssi_start...
        
        // Look for the closing tag
        part_end=input.find("-->");
        if (std::string::npos == part_end) {
            // No closing tag found.  This is a problem.
            // This is a problem we should bark loudly about...
            std::cerr << "No closing ssi found for " << path << " starting at offset " << start + ssi_start;
            return -1; // TODO: This probably breaks things badly.
        }
        
        std::string tag = trim(input.substr(ssi_start + 5, part_end - ssi_start - 5));
        part_end+=3;
        
        struct ssi_line line;
        line.max_recursion = 0;

        std::cerr << "Found ssi tag " << tag << "\n";

        if (tag.at(0)=='@') { // recursively include a file...
            const std::filesystem::path newfile = path.parent_path() /= tag.substr(1);
            line.command    = CONSTBIN_RECURSE; // Add the file, recursively...
            line.blob_index = add_file(newfile, true, false);
            line.max_recursion=ssi_lines[line.blob_index].max_recursion+1;

        } else { // include variable "tag"...
            line.blob_index = variables.index_blob(tag);
            line.command    = CONSTBIN_VARIABLE; // Substitute the given variable...
            // This will change order in the strings which is ok but not ideal:
        }
        line.next       = parse_ssi(input.substr(part_end), path, start+(int)part_end);
        next=(int)ssi_lines.size();
        ssi_lines.push_back(line);
    }
    
    // Push the amount of text we have so far....
    if (0 != ssi_start) { // if ssi_start == 0 there isn't any text to include yet...
        struct ssi_line line;
        line.blob_index = blobs.index_blob(input.substr(0, ssi_start));
        line.command    = CONSTBIN_LITERAL; // Just output this text...
        line.next       = next;
        line.max_recursion=ssi_lines[next].max_recursion; // this is effectively tail recursive?

        next=(int)ssi_lines.size();
        ssi_index[input] = next;
        ssi_lines.push_back(line);
    }
        
    return next;
}

int constindex::add_file(const std::filesystem::path &path, bool ssi, bool add_terminator) {
    std::string blob;
    int rv=file_to_string(blob, path, add_terminator);
    if (rv) {
        fprintf(stderr, "Unable to process: %s\n", path.filename().c_str());

        return rv; // some error... we weren't able to read the file...
    }
    
    auto index=ssi_index.find(blob);
    if (index!=ssi_index.end()) {
        // we have already parsed this input...
        return index->second;   // return index
    }
    
    if (ssi) {
        return parse_ssi(blob, path);
    }
    
    struct ssi_line line;
    line.blob_index = blobs.index_blob(blob);
    line.command    = CONSTBIN_LITERAL; // Just output this text...
    line.next       = -1;
    int ssi_offset=(int)ssi_lines.size();

    ssi_index[blob] = ssi_offset;
    ssi_lines.push_back(line);
    
    return ssi_offset;
}



void add_parts_to_c(std::stringstream &out, std::vector<part> &parts) {
    for (auto &part:parts) {
        add_c_file(out, part.label.c_str(), part.filename.c_str(), part.raw, "  ", 20);
    }
}

void add_parts_to_h(std::stringstream &out, std::vector<part> &parts) {
    for (auto &part:parts) {
        out << "extern " << part.label << "[" << part.raw.size() << "];\n";
    }
}

int generate_header(std::stringstream &out, std::vector<part> &parts, const char *filename) {
    std::string escaped(filename);
    escape_string(escaped, "/.\\\"' -");


// It'd be cool if #pragma once always worked.. but... embedded systems... hmm.
//
    out << "#ifndef __" << escaped << "__\n#define __" << escaped << "__\n\n";

    add_parts_to_h(out, parts);
    
    out << "\n#endif // __" << escaped << "__\n\n";
    
    return write_to_file_if_different(filename, out.str());

}



struct options {
    std::string filename;       // file path in the file system
    std::filesystem::path url;  // url to use in the tree
    bool parse_ssi;             // look for server side includes
    bool add_null;              // add null terminator
    int ssi_index;              // number of the ssi interpreter line for this entry
};

int main(int argc, char * argv[]) {

    int rv=0;
    
    const char *output_c_file=argv[1];
    const char *output_h_file=argv[2];

    bool parse_ssi=false;               // only look for ssi if told to...
    bool add_null=false;
    
    std::filesystem::path url_base="/"; // path in the URL mapping for files
    
    if (argc<4) {
        fprintf(stderr, "Usage:\n");
        fprintf(stderr, "    %s <output_c_file> <output_h_file>", argv[0]);
        fprintf(stderr, "    [file_to_encode] - path to file to be encoded\n");

        fprintf(stderr, "    [--r <directory to recurse>] - path to directory to recursively encode using url base\n");
        fprintf(stderr, "    [--ssi]    - do parse server side include comments in file\n");
        fprintf(stderr, "    [--no-ssi] - don't parse server side include comments in file\n");
        fprintf(stderr, "    [--base <path>] - set the base or prefix of the url path\n");
        fprintf(stderr, "    [--add-null]    - add a null character to the end of the string\n");
        fprintf(stderr, "    [--no-add-null] - do not add a null character to the end of the string\n");
        exit(-1);
    }
    
    
    std::vector<struct options> paths;

    // Use the command line options to create a list of all the paths and options we care about
    for (int i=3; i<argc; i++) {
        const char *arg=argv[i];

        if (0==strncmp(arg, "--", 2)) {
            if (strlen(arg)>2) {
                if (strcmp(arg+2, "r")==0) { // recursively add directory
                    if (argc<i+1) {
                        fprintf(stderr, "Expected but did not find directory name after --r option\n");
                        exit(-2);
                    }
                    
                    i++; // advance to directory
                    std::vector<std::pair<std::string,std::string> > p;
                    rv=recurse_directory_with_virtual(p, argv[i], url_base);
                    if (rv<0) {
                        fprintf(stderr, "Unable to read directory: %s", argv[i]);
                        exit(-4);
                    }
                    for (auto path:p) {
                        struct options o;
                        o.filename=path.first;
                        o.url = path.second;
                        o.parse_ssi = parse_ssi;
                        o.add_null = add_null;

                        paths.push_back(o);
                    }
                } else if (strcmp(arg+2, "no-ssi")==0) {       // do not parse files for server side include comments
                    parse_ssi=false;
                } else if (strcmp(arg+2, "ssi")==0) {          // parse files for server side include comments
                    parse_ssi=true;
                } else if (strcmp(arg+2, "base")==0) {         // Set the base part of the url
                    i++;
                    url_base=argv[i];
                } else if (strcmp(arg+2, "add-null")==0) {     // add a trailing null character to the string
                    add_null=true;
                } else if (strcmp(arg+2, "no-add-null")==0) {  // do not add a trailing null character to the string
                    add_null=false;
                }
            }
        } else {
            
            struct options o;
            o.filename=arg;
            o.url = url_base /= o.filename;
            o.parse_ssi = parse_ssi;
            o.add_null = add_null;

            paths.push_back(o);
        }
    }
    
    constindex index;
    std::map<std::string, int> name_to_ssi;   // map from name string to ssi index

    for (auto &o:paths) {
        int i=index.add_file(o.filename, o.parse_ssi, o.add_null);
        if (i)
        name_to_ssi[o.url] = i;
        o.ssi_index = i;
        std::cout << o.filename << " -> " << i << "\n";
    }

// At this point we've created the option vector
    
    std::vector<part> file_parts;

    for (auto &item:paths) {
        struct part p;
        p.filename.assign(item.filename);
        std::string url(item.url);
        escape_string(url);
        while (url.at(0)=='_') {
            url.erase(url.begin());
        }
        p.label="unsigned char " + url;
        
        file_to_string(p.raw, p.filename.c_str(), true);
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


