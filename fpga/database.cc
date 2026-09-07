#include "fpga/database.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "fpga/database-parsers.h"
#include "fpga/memory-mapped-file.h"

namespace fpga {
absl::StatusOr<BanksTilesRegistry> BanksTilesRegistry::Create(
  const Part &part, const PackagePins &package_pins) {
  absl::flat_hash_map<std::string, std::vector<uint32_t>> tile_to_banks;
  absl::flat_hash_map<uint32_t, absl::flat_hash_set<std::string>>
    banks_to_tiles_set;
  for (const auto &pair : part.iobanks) {
    const std::string tile = "HCLK_IOI3_" + pair.second;
    banks_to_tiles_set[pair.first].insert(tile);
    if (!tile_to_banks.contains(tile)) {
      tile_to_banks.insert({tile, {}});
    }
    tile_to_banks.at(tile).push_back(pair.first);
  }

  for (const auto &pin : package_pins) {
    banks_to_tiles_set[pin.bank].insert(pin.tile);
    if (!tile_to_banks.contains(pin.tile)) {
      tile_to_banks.insert({pin.tile, {}});
    }
    tile_to_banks.at(pin.tile).push_back(pin.bank);
  }
  // Convert sets to vectors.
  banks_to_tiles_type banks_to_tiles;
  for (const auto &pair : banks_to_tiles_set) {
    banks_to_tiles.insert(
      {pair.first,
       std::vector<std::string>(pair.second.begin(), pair.second.end())});
  }
  return BanksTilesRegistry(tile_to_banks, banks_to_tiles);
}

std::optional<std::vector<std::string>> BanksTilesRegistry::Tiles(
  uint32_t bank) const {
  if (!banks_to_tiles_.contains(bank)) {
    return {};
  }
  return banks_to_tiles_.at(bank);
}

std::vector<uint32_t> BanksTilesRegistry::TileBanks(
  const std::string &tile) const {
  if (!tile_to_bank_.contains(tile)) {
    return {};
  }
  return tile_to_bank_.at(tile);
}

bool PartDatabase::AddSegbitsToCache(const std::string &tile_type) {
  // Already have the tile type segbits.
  if (segment_bits_cache_.contains(tile_type)) {
    return false;
  }
  const std::optional<SegmentsBitsWithPseudoPIPs> maybe_segbits =
    tiles_->bits(tile_type);
  if (!maybe_segbits.has_value()) {
    return false;
  }
  segment_bits_cache_.insert({tile_type, maybe_segbits.value()});
  return true;
}

static absl::StatusOr<PartInfo> ParsePartInfo(
  const std::filesystem::path &prjxray_db_path, const std::string &part) {
  const auto parts_yaml_content_result =
    fpga::MemoryMapFile(prjxray_db_path / "mapping" / "parts.yaml");
  if (!parts_yaml_content_result.ok()) {
    return parts_yaml_content_result.status();
  }
  const auto devices_yaml_content_result =
    fpga::MemoryMapFile(prjxray_db_path / "mapping" / "devices.yaml");
  if (!devices_yaml_content_result.ok()) {
    return devices_yaml_content_result.status();
  }
  const absl::StatusOr<absl::flat_hash_map<std::string, PartInfo>>
    parts_infos_result = fpga::ParsePartsInfos(
      parts_yaml_content_result.value()->AsStringView(),
      devices_yaml_content_result.value()->AsStringView());
  if (!parts_infos_result.ok()) {
    return parts_infos_result.status();
  }
  const absl::flat_hash_map<std::string, PartInfo> &parts_infos =
    parts_infos_result.value();
  if (!parts_infos.contains(part)) {
    return absl::InvalidArgumentError(
      absl::StrFormat("invalid or unknown part \"%s\"", part));
  }
  return parts_infos.at(part);
}

static absl::StatusOr<TileGrid> ParseTileGrid(
  const std::filesystem::path &prjxray_db_path,
  const fpga::PartInfo &part_info) {
  const auto tilegrid_json_content_result =
    MemoryMapFile(prjxray_db_path / part_info.fabric / "tilegrid.json");
  if (!tilegrid_json_content_result.ok()) {
    return tilegrid_json_content_result.status();
  }
  return fpga::ParseTileGridJSON(
    tilegrid_json_content_result.value()->AsStringView());
}

// Stores full absolute paths of the databases for each tile type.
struct TileTypeDatabasePaths {
  // Corresponds to tile_type_<tile-type>.json
  std::filesystem::path tile_type_json;

  // segbits_<tile-type>.block_ram.db
  std::optional<std::filesystem::path> segbits_db;

  // segbits_<tile-type>.block_ram.db
  std::optional<std::filesystem::path> segbits_block_ram_db;

  // ppips_<tile-type>.db
  std::optional<std::filesystem::path> ppips_db;

  // mask_<tile-type>.db
  std::optional<std::filesystem::path> mask_db;
};

constexpr std::string_view kTileTypeJSONPrefix = "tile_type_";
constexpr std::string_view kTileTypeJSONSuffix = ".json";
constexpr std::string_view KFileNameFormatSegbits = "segbits_%s.db";
constexpr std::string_view KFileNameFormatSegbitsBlockRAM =
  "segbits_%s.block_ram.db";
constexpr std::string_view KFileNameFormatPseudoPIPs = "ppips_%s.db";
constexpr std::string_view KFileNameFormatMask = "mask_%s.db";

absl::Status GetDatabasePaths(
  const std::filesystem::path &path,
  absl::flat_hash_map<std::string, TileTypeDatabasePaths> &out) {
  const std::string filename = path.filename();
  const std::filesystem::path base_path = path.parent_path();

  // Extract string core of tile_type_<tile-type>.json.
  const size_t core_start = kTileTypeJSONPrefix.size();
  const size_t core_length =
    filename.size() - kTileTypeJSONPrefix.size() - kTileTypeJSONSuffix.size();
  std::string tile_type = filename.substr(core_start, core_length);
  std::string tile_type_lower;
  std::transform(tile_type.begin(), tile_type.end(),
                 std::back_inserter(tile_type_lower),
                 [](unsigned char c) { return std::tolower(c); });

  struct TileTypeDatabasePaths paths;
  paths.tile_type_json = path;
  const std::filesystem::path segbits_db =
    base_path / absl::StrFormat(KFileNameFormatSegbits, tile_type_lower);
  if (std::filesystem::exists(segbits_db)) {
    paths.segbits_db.emplace(segbits_db);
  }

  const std::filesystem::path segbits_block_ram_db =
    base_path /
    absl::StrFormat(KFileNameFormatSegbitsBlockRAM, tile_type_lower);
  if (std::filesystem::exists(segbits_block_ram_db)) {
    paths.segbits_block_ram_db.emplace(segbits_block_ram_db);
  }

  const std::filesystem::path ppips_db =
    base_path / absl::StrFormat(KFileNameFormatPseudoPIPs, tile_type_lower);
  if (std::filesystem::exists(ppips_db)) {
    paths.ppips_db.emplace(ppips_db);
  }

  const std::filesystem::path mask_db =
    base_path / absl::StrFormat(KFileNameFormatMask, tile_type_lower);
  if (std::filesystem::exists(mask_db)) {
    paths.mask_db.emplace(mask_db);
  }
  out.insert({tile_type, paths});
  return absl::OkStatus();
}

absl::Status IndexTileTypes(
  const std::filesystem::path &database_path,
  absl::flat_hash_map<std::string, TileTypeDatabasePaths>
    &tile_types_database_paths) {
  std::error_code ec;
  // Create a recursive directory iterator with options to skip permission
  // errors
  std::filesystem::recursive_directory_iterator it(
    database_path, std::filesystem::directory_options::skip_permission_denied,
    ec);
  const std::filesystem::recursive_directory_iterator end;
  if (ec) {
    return absl::InternalError("could not initialize directory iterator");
  }
  for (; it != end; it.increment(ec)) {
    if (ec) {
      std::cerr << "error accessing " << it->path() << ": " << ec.message()
                << '\n';
      ec.clear();
      continue;
    }

    // Check if the current entry is a regular file without throwing exceptions
    std::error_code file_ec;
    if (std::filesystem::is_regular_file(it->path(), file_ec)) {
      if (file_ec) {
        std::cerr << "error checking file type for " << it->path() << ": "
                  << file_ec.message() << '\n';
        file_ec.clear();
      } else {
        const std::filesystem::path &path = std::string(it->path());
        const std::string filename = path.filename().string();
        if (absl::StartsWith(filename, kTileTypeJSONPrefix) &&
            absl::EndsWith(filename, kTileTypeJSONSuffix)) {
          absl::Status status =
            GetDatabasePaths(path, tile_types_database_paths);
          if (!status.ok()) return status;
        }
      }
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<SegmentsBitsWithPseudoPIPs> ParseTileTypeDatabase(
  const TileTypeDatabasePaths &paths) {
  SegmentsBitsWithPseudoPIPs out;
  // Parse pseudo pips db.
  if (paths.ppips_db.has_value()) {
    const auto content = fpga::MemoryMapFile(paths.ppips_db.value());
    if (!content.ok()) return content.status();
    auto ppips_db = ParsePseudoPIPsDatabase(content.value()->AsStringView());
    if (!ppips_db.ok()) return ppips_db.status();
    out.pips = ppips_db.value();
  }
  absl::flat_hash_map<ConfigBusType, SegmentsBits> &segment_bits =
    out.segment_bits;
  // Parse segments bits.
  if (paths.segbits_db.has_value()) {
    const auto content = fpga::MemoryMapFile(paths.segbits_db.value());
    if (!content.ok()) return content.status();
    auto segbits_db =
      ParseSegmentsBitsDatabase(content.value()->AsStringView());
    if (!segbits_db.ok()) return segbits_db.status();
    segment_bits.insert({ConfigBusType::kCLBIOCLK, segbits_db.value()});
  }
  if (paths.segbits_block_ram_db.has_value()) {
    const auto content =
      fpga::MemoryMapFile(paths.segbits_block_ram_db.value());
    if (!content.ok()) return content.status();
    auto segbits_db =
      ParseSegmentsBitsDatabase(content.value()->AsStringView());
    if (!segbits_db.ok()) return segbits_db.status();
    segment_bits.insert({ConfigBusType::kBlockRam, segbits_db.value()});
  }
  return out;
}

absl::StatusOr<fpga::BanksTilesRegistry> CreateBanksRegistry(
  const fpga::Part &part, const std::filesystem::path &package_pins_path) {
  // Parse package pins.
  const absl::StatusOr<std::unique_ptr<fpga::MemoryBlock>>
    package_pins_csv_result = fpga::MemoryMapFile(package_pins_path);
  if (!package_pins_csv_result.ok()) return package_pins_csv_result.status();
  const absl::StatusOr<fpga::PackagePins> package_pins_result =
    fpga::ParsePackagePins(package_pins_csv_result.value()->AsStringView());
  if (!package_pins_result.ok()) {
    return package_pins_result.status();
  }
  const fpga::PackagePins &package_pins = package_pins_result.value();
  return fpga::BanksTilesRegistry::Create(part, package_pins);
}

absl::StatusOr<PartDatabase> PartDatabase::Parse(std::string_view database_path,
                                                 std::string_view part_name) {
  const absl::StatusOr<PartInfo> part_info_result =
    ParsePartInfo(std::filesystem::path(database_path), std::string(part_name));
  if (!part_info_result.ok()) {
    return part_info_result.status();
  }
  const PartInfo &part_info = part_info_result.value();

  // Parse tilegrid.
  absl::StatusOr<fpga::TileGrid> tilegrid_result =
    ParseTileGrid(database_path, part_info);
  if (!tilegrid_result.ok()) {
    return tilegrid_result.status();
  }

  absl::flat_hash_map<std::string, TileTypeDatabasePaths>
    tiles_types_databases_paths;
  absl::Status status = IndexTileTypes(std::filesystem::path(database_path),
                                       tiles_types_databases_paths);
  if (!status.ok()) {
    return status;
  }

  auto tiles_database = [paths = std::move(tiles_types_databases_paths)](
                          const std::string &tile_type)
    -> std::optional<SegmentsBitsWithPseudoPIPs> {
    if (paths.contains(tile_type)) {
      absl::StatusOr<SegmentsBitsWithPseudoPIPs> tile_type_database_result =
        ParseTileTypeDatabase(paths.at(tile_type));
      if (!tile_type_database_result.ok()) {
        // TODO(lromor): Fix silent failure.
        return {};
      }
      return tile_type_database_result.value();
    }
    return {};
  };
  // Parse part.json.
  const absl::StatusOr<std::unique_ptr<fpga::MemoryBlock>> part_json_result =
    fpga::MemoryMapFile(std::filesystem::path(database_path) / part_name /
                        "part.json");
  if (!part_json_result.ok()) return part_json_result.status();
  const absl::StatusOr<fpga::Part> part_result =
    fpga::ParsePartJSON(part_json_result.value()->AsStringView());
  if (!part_result.ok()) {
    return part_result.status();
  }
  const fpga::Part &part = part_result.value();

  auto banks_tiles_registry_result =
    CreateBanksRegistry(part, std::filesystem::path(database_path) / part_name /
                                "package_pins.csv");
  if (!banks_tiles_registry_result.ok()) {
    return banks_tiles_registry_result.status();
  }
  const std::shared_ptr<Tiles> tiles = std::make_shared<Tiles>(
    std::move(tilegrid_result.value()), std::move(tiles_database),
    std::move(banks_tiles_registry_result.value()), part);
  return absl::StatusOr<PartDatabase>(tiles);
}

// CLBLM_R_X33Y38.SLICEM_X0.ALUT.INIT, CLBLM_R_X33Y38 is a tilename.
void PartDatabase::ConfigBits(const std::string &tile_name,
                              const std::string &feature, uint32_t address,
                              const BitSetter &bit_setter) {
  // fprintf(stderr, "%s %s %u\n", tile_name.c_str(), feature.c_str(), address);
  // Given the tilename, get the tile type.
  const Tile &tile = tiles_->grid.at(tile_name);

  // Pseudo PIPs are keyed by the tile's *own* type (ppips_<type>.db), even
  // when the tile borrows its segbits from another type through an alias
  // (e.g. LIOI3_SING -> LIOI3). Resolve them before applying the alias, since
  // the aliased type's database knows nothing about them.
  AddSegbitsToCache(tile.type);
  if (segment_bits_cache_.contains(tile.type)) {
    const std::string own_key = absl::StrJoin({tile.type, feature}, ".");
    if (segment_bits_cache_.at(tile.type).pips.contains(own_key)) {
      return;
    }
  }

  // Either the feature tile type of the tile type alias.
  std::string tile_type = tile.type;
  std::string aliased_feature = feature;

  // Materialize aliased bit maps.
  absl::flat_hash_map<ConfigBusType, BitsBlock> aliased_bits_map;
  for (const auto &pair : tile.bits) {
    const BitsBlock &bits_block = pair.second;
    const ConfigBusType &bus_type = pair.first;
    if (bits_block.alias.has_value()) {
      const BitsBlockAlias alias = bits_block.alias.value();
      // TODO: check that for each block the aliased tile type is the same.
      tile_type = bits_block.alias.value().type;
      std::vector<std::string> feature_parts =
        absl::StrSplit(feature, absl::MaxSplits('.', 1));
      if (feature_parts.size() >= 2) {
        std::string &site = feature_parts[1];
        if (alias.sites.contains(site)) {
          site = alias.sites.at(site);
        }
        aliased_feature = absl::StrJoin(feature_parts, ".");
      }
      const BitsBlock aliased_block = {
        .alias = {},
        .base_address = bits_block.base_address,
        .frames = bits_block.frames,
        .offset = int32_t(bits_block.offset - alias.start_offset),
        .words = bits_block.words,
      };
      aliased_bits_map.insert({bus_type, aliased_block});
    } else {
      aliased_bits_map.insert({bus_type, bits_block});
    }
  }

  // Fill the cache with the current tile type segbits.
  AddSegbitsToCache(tile_type);
  CHECK(segment_bits_cache_.contains(tile_type));
  const SegmentsBitsWithPseudoPIPs &tile_type_features_bits =
    segment_bits_cache_.at(tile_type);
  const std::string tile_segments_bits_key =
    absl::StrJoin({tile_name, aliased_feature}, ".");
  if (tile_type_features_bits.pips.contains(tile_segments_bits_key)) {
    return;
  }

  // Search our database of features and get the segbit.
  const struct TileFeature tile_feature = {
    .tile_feature = absl::StrJoin({tile_type, aliased_feature}, "."),
    .address = address,
  };

  // If it's a pseudo pip, skip.
  // TODO(lromor): Is this really necessary? It looks like all pips are
  // different.
  if (tile_type_features_bits.pips.contains(tile_feature.tile_feature)) {
    return;
  }

  // The tile name has some specific config bus base addresses.
  bool matched = false;
  for (const auto &config_bus_bits_pair : aliased_bits_map) {
    const ConfigBusType &bus = config_bus_bits_pair.first;
    const uint32_t base_address = config_bus_bits_pair.second.base_address;
    const uint32_t offset = config_bus_bits_pair.second.offset;
    if (!tile_type_features_bits.segment_bits.contains(bus)) {
      continue;
    }
    const SegmentsBits &features_segbits =
      tile_type_features_bits.segment_bits.at(bus);
    if (aliased_bits_map.size() > 1 && !features_segbits.count(tile_feature)) {
      // a feature will probably only match one bus (e.g. BRAM init or BRAM
      // config/routing)
      continue;
    }
    const auto segbits_it = features_segbits.find(tile_feature);
    CHECK(segbits_it != features_segbits.end())
      << "unknown feature " << tile_name << "." << feature << " (looked up "
      << tile_feature.tile_feature << " in the " << tile_type
      << " segbits/ppips database)";
    const auto &segbits = segbits_it->second;
    matched = true;
    for (const auto &segbit : segbits) {
      const uint32_t address = base_address + segbit.word_column;
      const uint32_t bit_pos = offset * kWordSizeBits + segbit.word_bit;
      const FrameBit frame_bit = {
        .word = bit_pos / kWordSizeBits,
        .index = bit_pos % kWordSizeBits,
      };
      bit_setter(bus, address, frame_bit, segbit.is_set);
    }
  }
  CHECK(matched);
}
}  // namespace fpga
