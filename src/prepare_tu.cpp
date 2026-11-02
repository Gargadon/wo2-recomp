#include <rex/filesystem/devices/stfs_container_device.h>
#include <rex/filesystem/entry.h>
#include <rex/filesystem/file.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace fs = std::filesystem;

rex::filesystem::Entry* FindModule(rex::filesystem::Entry* entry,
                                   std::string_view name) {
  if (!entry) return nullptr;
  if (entry->name() == name) return entry;
  for (const auto& child : entry->children()) {
    if (auto* result = FindModule(child.get(), name)) return result;
  }
  return nullptr;
}

bool Extract(const fs::path& package, const fs::path& destination,
             std::string_view name = "default.xexp") {
  using rex::X_STATUS;
  using namespace rex::filesystem;
  if (!StfsContainerDevice::ReadPackageHeader(package)) return false;
  StfsContainerDevice device("", package);
  if (!device.Initialize()) return false;
  auto* entry = FindModule(device.ResolvePath(""), name);
  if (!entry) return false;
  File* file = nullptr;
  if (entry->Open(0, &file) != X_STATUS_SUCCESS || !file)
    throw std::runtime_error("Cannot open " + std::string(name) + " in " + package.string());
  std::vector<uint8_t> bytes(entry->size());
  size_t count = 0;
  const auto status = file->ReadSync(bytes, 0, &count);
  file->Destroy();
  if (status != X_STATUS_SUCCESS || count != bytes.size() ||
      bytes.size() < 4 || bytes[0] != 'X' || bytes[1] != 'E' ||
      bytes[2] != 'X' || bytes[3] != '2')
    throw std::runtime_error("Invalid or incomplete " + std::string(name) + " in " + package.string());
  fs::create_directories(destination.parent_path());
  auto temporary = destination;
  temporary += ".tmp";
  {
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    output.close();
    if (!output) throw std::runtime_error("Cannot write " + temporary.string());
  }
  fs::rename(temporary, destination);
  std::cout << "Extracted " << name << " from " << package << '\n';
  return true;
}

int main(int argc, char** argv) {
  try {
    if (argc == 5 && std::string_view(argv[1]) == "--tu") {
      const fs::path package(argv[2]), base(argv[3]), directory(argv[4]);
      const auto executable = directory / "default.xex";
      const auto patch = directory / "default.xexp";
      if (!fs::is_regular_file(base))
        throw std::runtime_error("Base executable not found: " + base.string());
      if (!fs::is_regular_file(patch) && !Extract(package, patch, "default.xexp"))
        throw std::runtime_error("Title Update #1 patch not found in " + package.string() +
                                 ". Set WO2_TU_PACKAGE to the original TU #1 container.");
      // ReXGlue applies the sibling XEXP in memory. Keep an unpatched copy.
      if (!fs::is_regular_file(executable)) {
        fs::create_directories(directory);
        fs::copy_file(base, executable);
      }
      // The runtime only picks up a patch it finds as "<module>.xexp" next to
      // the module it launches (game:\default.xex), so the same patch has to
      // exist beside the base executable. Without it the guest runs the
      // unpatched image while the recompiled code comes from the patched one,
      // and the module entry point no longer resolves.
      const auto base_patch = base.parent_path() / (base.stem().string() + ".xexp");
      if (base_patch != patch) {
        fs::copy_file(patch, base_patch, fs::copy_options::overwrite_existing);
        std::cout << "Installed runtime XEX patch: " << base_patch << '\n';
      }
      return 0;
    }
    throw std::runtime_error("Usage: wo2_prepare_tu --tu <TU package> <base default.xex> <output dir>");
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
