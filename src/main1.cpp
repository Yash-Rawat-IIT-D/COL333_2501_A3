#include <stdio.h>
#include <stdlib.h>
#include <fstream>
#include <iostream>
#include "utils.hpp"

int main(int argc, char* argv[]) {
    
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <input_fname>" << std::endl;
        return 1;
    }

    ifstream file_stream(argv[1]);
    
    if (!file_stream.is_open()) {
        cerr << "Error: Unable to open file " << argv[1] << endl;
        return 1;
    }

    MetroMap metro_map = parseInputFile(file_stream);
    metro_map.printInfo();

    return 0;

}