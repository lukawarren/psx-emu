#include "cue.h"
#include "io.h"

constexpr static u32 sector_size = 0x930;

Cue::Cue(const std::string& cue_file, const std::string& bin_file)
{
    FILE* file = fopen(cue_file.c_str(), "r");
    if (!file) throw std::runtime_error("failed to open CUE file " + cue_file);

    cd = cue_parse_file(file);
    if (cd == nullptr) throw std::runtime_error("failed to parse CUE file " + cue_file);
    fclose(file);

    dbg("TODO: assuming one BIN per CUE");
    auto mapped_bin = map_file(bin_file);
    bin = std::get<0>(mapped_bin);
    length = std::get<1>(mapped_bin);
    fd = std::get<2>(mapped_bin);

    if (cd_get_ntrack(cd) != 1)
    {
        dbg("TODO: multi-track CUE files");
        exit(1);
    }
}

u8 const* Cue::read_byte(const u32 lba)
{
#if 0
    // TODO: assume LBA is in track 1
    const Track* track = cd_get_track(cd, 1);
    assert(track != nullptr);

    // Get start LBA for track
    const long track_start_lba = track_get_start(track);

    // Add on supplied LBA, then convert to sector
    const long byte_offset = (track_start_lba + lba) * sector_size;
    assert((size_t)byte_offset < length);

    return &bin[byte_offset];
#else
    assert((size_t)lba < length);
    return &bin[lba];
#endif
}

Cue::~Cue()
{
    unmap_file(bin, length, fd);
    cd_delete(cd);
}
