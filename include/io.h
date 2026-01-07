#pragma once

std::pair<std::ifstream, std::streamsize> read_file(
    const std::string& filename,
    const size_t max_size
);

void read_file(
    u8* buffer,
    size_t max_size,
    const std::string& filename
);

u8* read_file(const std::string& filename);

std::tuple<u8*, size_t, int> map_file(const std::string& filename);
void unmap_file(const u8* buffer, const size_t length, const int fd);
