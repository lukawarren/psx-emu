#include "io.h"

std::pair<std::ifstream, std::streamsize> read_file(const std::string& filename, const size_t max_size)
{
    // Check isn't folder
    if (!std::filesystem::is_regular_file(filename))
        throw std::runtime_error(filename + " is not a file");

    // Open file
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("unable to open file " + filename);

    // Get file length
    file.seekg(0, std::ios::end);
    const auto file_length = file.tellg();
    file.seekg(0, std::ios::beg);

    if (max_size != 0 && (size_t)file_length > max_size)
        throw std::runtime_error(filename + " is too large");

    return { std::move(file), file_length };
}

void read_file(u8* buffer, const size_t max_size, const std::string& filename)
{
    auto file = read_file(filename, max_size);
    file.first.read(reinterpret_cast<char*>(buffer), file.second);
    file.first.close();
}

u8* read_file(const std::string& filename)
{
    auto file = read_file(filename, 0);
    u8* buffer = new u8[file.second];
    file.first.read(reinterpret_cast<char*>(buffer), file.second);
    file.first.close();
    return buffer;
}

std::tuple<u8*, size_t, int> map_file(const std::string& filename)
{
    int fd = open(filename.c_str(), O_RDWR);
    if (fd < 0)
        throw std::runtime_error("failed to load file " + filename);

    struct stat info;
    if (fstat(fd, &info) < 0)
        throw std::runtime_error("failed to determine file size for " + filename);

    u8* file = (u8*)mmap(0, (size_t)info.st_size, PROT_READ, MAP_SHARED, fd, 0);
    if (file == MAP_FAILED)
        throw std::runtime_error("failed to mmap " + filename);

    return { file, info.st_size, fd };
}

void unmap_file(const u8* buffer, const size_t length, const int fd)
{
    close(fd);
    if (munmap((void*)buffer, length) < 0)
        throw std::runtime_error("failed to munmap file");
}
