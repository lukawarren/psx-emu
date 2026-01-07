#pragma once

class Cue
{
public:
    Cue(const std::string& cue_file, const std::string& bin_file);
    ~Cue();

    u8 const* read_sector(const u32 lba);

private:
    // BIN file
    u8* bin;
    size_t length;
    int fd;

    // CUE file
    Cd* cd;
};
