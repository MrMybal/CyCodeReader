#pragma once
#include <filesystem>

namespace cycode::reader {
// Caller holds an exclusive InstanceLock. Only package-owned files are changed;
// data and separately installed extensions are preserved. ZIP_STORED is required.
void applyPortableUpdate(const std::filesystem::path &archive,const std::filesystem::path &installation);
}
