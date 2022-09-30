#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>

int main (int argc, char *argv[]) {
    std::cout << "You have entered " << argc << " arguments:" << "\n";
    for (int i = 0; i < argc; ++i)
        std::cout << argv[i] << "\n";

    if (argc != 2) {
        return -1;
    }
    std::string filename = argv[1];

    int lines_to_skip = 0;
    int bytes_per_line = 2*sizeof(unsigned long long) + 2*sizeof(float); // 2 unsinged long longs and 2 floats per line
    FILE *file;
    
    file = fopen(filename.c_str(), "rb");
    if (!file)
    {
        fprintf(stderr, "Unable to open file %s\n", filename.c_str());
        return -1;
    }

    // get file size
    fseek(file, 0L, SEEK_END);
    int filesize = ftell(file);
    int lines_to_read = (int)filesize/bytes_per_line;

    fseek(file, /* from the start */ lines_to_skip * bytes_per_line, SEEK_SET);
    unsigned char buffer[bytes_per_line];
    for (int i=0; i < lines_to_read; i++) {
        size_t result = fread(buffer, bytes_per_line, 1, file);
        unsigned long long t1;
        memcpy(&t1, &buffer, sizeof(unsigned long long));
        unsigned long long t2;
        memcpy(&t2, &buffer[0 + sizeof(unsigned long long)], sizeof(unsigned long long));
        float price;
        memcpy(&price, &buffer[0 + 2*sizeof(unsigned long long)], sizeof(float));
        float vol;
        memcpy(&vol, &buffer[0 + 2*sizeof(unsigned long long) + sizeof(float)], sizeof(float));

        printf("%lld %lld %f %f\n", t1,t2,price,vol);
    }
    
    fclose(file);
    return 0;
}