#pragma once

#include <string>
#include <vector>

#include <prometheus/collectable.h>

namespace sys_info {

struct Diskstat {
    using ul = unsigned long;
    using ui = unsigned int;

    int major;
    int minor;
    std::string devname;

    ul ReadCompleted;  // Blocks
    ul ReadMerged;
    ul ReadSectors;
    ui ReadTime;  // ms
    ul WriteCompleted;
    ul WriteMerged;
    ul WriteSectors;
    ui WriteTime;       // ms
    ui IOInProgress;    // May go to 0
    ui IOTime;          // ms
    ui WeightedIOTime;  // ms
    ul DiscardCompleted;
    ul DiscardMerged;
    ul DiscardSectors;
    ui DiscardTime;
    ul FlushComplete;
    ui FlushTime;

    static double time_to_float(ui time);
    static double sector_byte_size();

    static std::vector<Diskstat> create();
};

}  // namespace sys_info
