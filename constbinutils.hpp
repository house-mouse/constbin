#ifndef __CONSTBINUTILS_HPP__
#define __CONSTBINUTILS_HPP__

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

int file_to_string(std::string &filestring, const std::filesystem::path &path, bool add_terminator=false);

//
// One of our goals is to play well with build systems like make and cmake which
// may look at modification date/times to try to tell if the contents of a file have
// changed.  This function writes the data to the filename, but only if it is not
// able to read the identical data from the file first...

int write_to_file_if_different(const std::filesystem::path &path, const std::string &data);

std::string trim(const std::string& str,
                 const std::string& whitespace = " \t\n\r");

//
// string_to_hex
//
// Convert the string 'input' into hex values suitable for including in a C file.
//
// Each line generated can be prefixed with a line_prefix, and each line will contain
// at most bytes-per_line ehx digits.
//

void string_to_hex(std::string &hex, const std::string &input, const std::string &line_prefix, int bytes_per_line);

// Crush any characters that wouldn't be appropriate
void escape_string(std::string &s, const char *to_escape="/.\\\"' ", char escape='_');

int recurse_directory(std::vector<std::string> &paths, const std::string directory); 
int recurse_directory_with_virtual(std::vector<std::pair<std::string, std::string> > &paths, const std::string directory, const std::string virtual_directory);
#endif // __CONSTBINUTILS_HPP__
