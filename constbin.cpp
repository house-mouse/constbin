#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>
#include <string.h>

// Read a file into a std::string

// There are tons of progams to do this but I've found all I've tried lacking...
// https://www.devever.net/~hl/incbin
// https://gist.github.com/sivachandran/3a0de157dccef822a230
//
// I really want cmake to "do the right thing" with dependencies, but it doesn't
// with a cmake script (that runs at config time not build time) and xxd
// would be a strong contender... but it's not universally available and there
// are limitations on naming scheme that I don't love.
//
// Unlike most tools that do this, I'm pretty sure I want unique .h and .c.
//

int file_to_string(std::string &filestring, const char *filename, bool add_terminator) {
    FILE * f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Unable to open file: %s\n", filename);
        return -1;
    }
    fseek(f, 0, SEEK_END);
    size_t size = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    fprintf(stderr, "%s is %ld bytes long\n", filename, (long)size);


    filestring.resize(size + (add_terminator ? 1 : 0));

    char *buf = (char *)filestring.c_str();
    if (fread(buf, size, 1, f) != 1) {
        fprintf(stderr, "Failed to read %s!\n", filename);
        fclose(f);
        return -2;
    }
    fclose(f);

    if (add_terminator) {
        buf[size]=0;
    }
    return 0;
}

void string_to_hex(std::string &hex, const std::string &input, const std::string &line_prefix, int bytes_per_line) {

    size_t max_size = (input.size() / bytes_per_line + 1) *  (5 * bytes_per_line + line_prefix.size() + 1);

    hex.resize(max_size);

    char       *out = (char *)hex.c_str();
    const char *in  = input.c_str();
    const char *end = in + input.size();

    while(in < end) {
        memcpy(out, line_prefix.c_str(), line_prefix.size());
        out += line_prefix.size();
        int items=end - in;
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

size_t output_c_file(FILE *out, const char *label, const char *filename, std::string line_prefix="  ", int bytes_per_line=20) {
    std::string raw;
    std::string hex;

    file_to_string(raw, filename, true);
    string_to_hex(hex, raw, line_prefix, bytes_per_line);

    fprintf(out, "%s[%zu] = {\n", label, raw.size());

    fwrite(hex.c_str(), hex.size(), 1, out);

    fprintf(out, "};\n");

    return raw.size();
}

struct parts {
    const char *filename;
    size_t size;
    std::string label;
};

void output_parts_to_c(FILE *out, std::vector<parts> &parts) {
    for (auto &part:parts) {
        part.size = output_c_file(out, part.label.c_str(), part.filename, "  ", 20);
    }
}

void output_parts_to_h(FILE *out, std::vector<parts> &parts) {
    for (auto &part:parts) {
        fprintf(out, "extern %s[%zu];\n", part.label.c_str(), part.size);
    }
}

// Crush any characters that wouldn't be approrpiate for a C name...

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

void output_header(const char *filename, std::vector<parts> &parts) {
    std::string escape(filename);
    escape_string(escape);

    FILE * out = fopen(filename, "wb"); 
// It'd be cool if #pragma once always worked.. but... embedded systems... hmm.
//
    fprintf(out, "#ifndef __%s__\n#define __%s__\n\n", escape.c_str(), escape.c_str());

    output_parts_to_h(out, parts);
    fprintf(out, "\n#endif // __%s__\n\n", escape.c_str());
}

int main(int argc, char * argv[]) {

    std::vector<parts> files;

    const char *output=argv[1];
    const char *header_file=argv[2];

    if (argc<2) {
        fprintf(stderr, "Usage:\n");
        fprintf(stderr, "    %s <output_c_file> <output_h_file> [files_to_encode]\n", argv[0]);
        exit(-1);
    }
    for (int i=3; i<argc; i++) {
        struct parts p;
        p.filename=argv[i];
        std::string file(p.filename);
        escape_string(file);
        p.label="unsigned char " + file;
        files.push_back(p);
    }

    FILE * out = fopen(output, "wb"); 

    output_parts_to_c(out, files);

//    output_parts_to_h(out, files);

    fclose(out);


    output_header(header_file, files);
    return 0;
}


