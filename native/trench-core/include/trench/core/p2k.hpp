#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include "trench/core/role.hpp"

namespace trench::core::p2k {

inline constexpr double kSr = 44'100.0;
inline constexpr std::size_t kStageCount = 6;
inline constexpr std::size_t kWordCount = 5;
inline constexpr std::uint16_t kFiller = 0xFC;
inline constexpr std::uint16_t kS6ZeroRsqWord = 0x01F0;
inline constexpr std::size_t kMaxMagByte = 254;
inline constexpr double kPoleRMax = 0.999786473;
inline constexpr double kRootHiHz = 0.4665 * kSr;
inline constexpr std::uint16_t kPoleCeilingRsqWord = 0x4BFB;

inline constexpr std::array<std::uint16_t, 16> kLatticeLow{
    0xEE, 0xED, 0xF5, 0xF9, 0xFB, 0xFC, 0xFC, 0xFC,
    0xFC, 0xFC, 0xFC, 0xFC, 0xFC, 0xFC, 0xFC, 0xFD};
inline constexpr std::uint16_t kE15AltLow = 0x7D;

using StageWords = std::array<std::uint16_t, 4>;
using CornerWords = std::array<StageWords, kStageCount>;
using StoredStage = std::array<std::uint16_t, kWordCount>;
using StoredCorner = std::array<StoredStage, kStageCount>;
using PackedCorner = std::array<std::uint16_t, kStageCount * kWordCount>;
using StageScales = std::array<double, kStageCount>;

constexpr std::uint16_t word_of(std::size_t byte) {
  return static_cast<std::uint16_t>((byte << 8U) | kLatticeLow[byte >> 4U]);
}

const std::vector<std::uint16_t>& lattice_words();
const std::vector<double>& lattice_decoded();
std::size_t lattice_len();
bool magnitude_admissible(std::size_t index, bool is_pole);
bool intent_admits(const RoleIntent& intent, std::size_t si, const StageWords& candidate);
std::size_t nearest_lattice(double value);
std::size_t nearest_lattice_word(std::uint16_t word);
std::pair<double, double> pq(std::uint16_t w_mag, std::uint16_t w_rsq);
double pair_radius(double p, double q);
double pole_radius_ceiling();
bool is_legal(double p, double q, bool is_pole);
std::pair<std::uint16_t, std::uint16_t> words_from_root(double hz, double r);
std::uint16_t mag_word_for(double hz, std::uint16_t rsq_word);
std::uint16_t nearest_gain_word(double scale);
CornerWords enter(const CornerWords& words);

inline constexpr std::size_t kNpts = 512;
inline constexpr double kLoHz = 20.0;
inline constexpr double kHiHz = 0.499 * kSr;

double erb_hz(double f);
double psum(std::span<const double> values);

enum class Cost { kWeightedVar, kVariation, kUnweighted };

struct PerceptualSpace {
  enum class Weight { kErb, kFlat };
  struct Band {
    double lo_hz{};
    double hi_hz{};
    double gain{1.0};
    bool operator==(const Band&) const = default;
  };
  double lo_hz{kLoHz};
  double hi_hz{kHiHz};
  Weight weight{Weight::kErb};
  std::vector<Band> emphasis;
  double smooth_octaves{0.0};
  bool operator==(const PerceptualSpace&) const = default;
};

struct Grid {
  std::vector<double> hz;
  std::vector<double> weight;
  double weight_sum{};
  std::vector<double> z1r, z1i, z2r, z2i;
  std::size_t smooth_bins{};

  void factor_db(double p, double q, std::span<double> out) const;
  double weighted_var(std::span<const double> resid, std::span<double> scratch) const;
  double residual_var(std::span<const double> target, std::span<const double> model,
                      std::span<double> scratch) const;
  double variation(std::span<const double> resid) const;
  double unweighted_var(std::span<const double> resid) const;
  double score(std::span<const double> resid, Cost cost, std::span<double> scratch) const;
};

const Grid& grid();
Grid make_grid(const PerceptualSpace& space);

struct AbsoluteError {
  double max_db{};
  double rms_db{};
  double offset_db{};
  double max_after_offset_db{};
  double rms_after_offset_db{};
  double worst_hz{};
};

AbsoluteError absolute_error(std::span<const double> target, std::span<const double> written);

class Corner {
 public:
  static Corner identity(std::size_t seed_byte, std::size_t rsq_byte,
                         const Grid& g = trench::core::p2k::grid());
  static Corner from_words(const CornerWords& w, const Grid& g = trench::core::p2k::grid());

  [[nodiscard]] const Grid& grid() const noexcept { return *grid_; }
  void refresh(std::size_t si);
  [[nodiscard]] std::span<const double> num(std::size_t si) const;
  [[nodiscard]] std::span<const double> den(std::size_t si) const;
  void stage_db_into(std::size_t si, std::span<double> out) const;
  void total_into(std::span<double> out) const;
  [[nodiscard]] std::vector<double> total() const;

  CornerWords w{};
  RoleIntent intent{};

 private:
  Corner() = default;
  const Grid* grid_{};
  std::vector<double> num_;
  std::vector<double> den_;
};

std::pair<double, double> dc_terms(const StageWords& w);
StageScales stage_gain_pass(const Corner& c);
PackedCorner pack_corner(const Corner& c, const StageScales& scales);
std::array<std::uint8_t, 240> pack_body(const std::array<PackedCorner, 4>& corners);
double dc_gain_db(const PackedCorner& corner);

struct Scratch {
  std::vector<double> total = std::vector<double>(kNpts, 0.0);
  std::vector<double> base = std::vector<double>(kNpts, 0.0);
  std::vector<double> bank = std::vector<double>(kNpts, 0.0);
  std::vector<double> resid = std::vector<double>(kNpts, 0.0);
  std::vector<double> tmp = std::vector<double>(kNpts, 0.0);
  std::vector<double> cand = std::vector<double>(kNpts, 0.0);
};

using LossFn = std::function<double(std::span<const double> target,
                                    std::span<const double> candidate)>;

double residual_var(std::span<const double> target, std::span<const double> model, Scratch& s);
double corner_var(const Corner& c, std::span<const double> target, Scratch& s);
double corner_cost(const Corner& c, std::span<const double> target, Cost cost, Scratch& s,
                   const LossFn* loss = nullptr);
std::pair<double, bool> sweep_axis(Corner& c, std::span<const double> target, std::size_t si,
                                   std::size_t wi, double best, Scratch& s);
std::pair<double, bool> sweep_axis_cost(Corner& c, std::span<const double> target, std::size_t si,
                                        std::size_t wi, double best, Cost cost, Scratch& s,
                                        const LossFn* loss = nullptr);
std::pair<double, bool> sweep_radius(Corner& c, std::span<const double> target, std::size_t si,
                                     std::size_t root, double best, Scratch& s);
std::pair<double, bool> sweep_radius_cost(Corner& c, std::span<const double> target, std::size_t si,
                                          std::size_t root, double best, Cost cost, Scratch& s,
                                          const LossFn* loss = nullptr);
std::pair<double, bool> stage_moves(Corner& c, std::span<const double> target, std::size_t si,
                                    double best, Scratch& s);
std::pair<double, bool> stage_moves_cost(Corner& c, std::span<const double> target, std::size_t si,
                                         double best, Cost cost, Scratch& s,
                                         const LossFn* loss = nullptr);
double polish(Corner& c, std::span<const double> target, std::size_t max_passes, Scratch& s);

inline constexpr std::int32_t kFineSpan = 255;

std::pair<double, bool> sweep_axis_fine(Corner& c, std::span<const double> target, std::size_t si,
                                        std::size_t wi, double best, Scratch& s);
std::pair<double, bool> sweep_axis_fine_cost(Corner& c, std::span<const double> target,
                                             std::size_t si, std::size_t wi, double best, Cost cost,
                                             Scratch& s, const LossFn* loss = nullptr);
double polish_fine(Corner& c, std::span<const double> target, std::size_t max_passes, Scratch& s);

class Rng {
 public:
  explicit Rng(std::uint64_t seed) : state_(seed) {}
  double uniform(double lo, double hi);
  std::size_t below(std::size_t n);

 private:
  std::uint64_t next_u64();
  std::uint64_t state_;
};

struct ContinuousStage {
  double pole_hz{};
  double pole_r{};
  double zero_hz{};
  double zero_r{};
};

using ContinuousCorner = std::array<ContinuousStage, kStageCount>;

inline constexpr std::size_t kContinuousStarts = 3;

double s6_zero_radius();
StoredCorner rom_corner_words(std::span<const std::uint8_t> body, std::size_t corner);
CornerWords rom_seed(const StoredCorner& words);
bool pole_is_legal(const StageWords& w);
CornerWords identity_words();
CornerWords peel_seed(std::span<const double> target, const Grid& g = grid());
CornerWords peel_seed_by(std::span<const double> target, Cost cost, const Grid& g = grid());
std::vector<std::pair<std::string_view, ContinuousCorner>> topological_seeds(
    std::span<const double> target, const Grid& g = grid());
std::pair<ContinuousCorner, double> continuous_from(std::span<const double> target,
                                                    const ContinuousCorner& start,
                                                    const Grid& g = grid());
std::optional<std::pair<ContinuousCorner, double>> continuous_capacity(
    std::span<const double> target, Rng& rng, std::size_t starts, const Grid& g = grid());
std::optional<ContinuousCorner> continuous_seed(std::span<const double> target, Rng& rng,
                                                std::size_t starts, const Grid& g = grid());
std::optional<std::tuple<std::string_view, ContinuousCorner, double>> continuous_best(
    std::span<const double> target, const Grid& g = grid());
CornerWords words_from_continuous(const ContinuousCorner& seed);

struct SeedContinuous {};
struct SeedPeel {};
using Seed = std::variant<SeedContinuous, SeedPeel, CornerWords>;

struct FitOptions {
  std::size_t max_passes = 12;
  std::size_t continuous_starts = kContinuousStarts;
  std::uint64_t rng_seed = 20'260'820;
  bool allow_continuous = false;
  LossFn loss;
  std::optional<PackedCorner> baseline;
  const Grid* grid = nullptr;
  RoleIntent intent{};
};

StageScales stage_gain_pass_held(const Corner& c, std::uint32_t mask,
                                 const PackedCorner& baseline);

struct P2kFit {
  CornerWords words{};
  StageScales scales{};
  PackedCorner packed{};
  double shape_rms_db{};
  std::string_view seed_used;
};

std::pair<Corner, double> polish_from_words(const CornerWords& words,
                                            std::span<const double> target,
                                            std::size_t max_passes, const Grid& g = grid());
std::pair<Corner, double> polish_from_words_fine(const CornerWords& words,
                                                 std::span<const double> target,
                                                 std::size_t max_passes,
                                                 const Grid& g = grid());
std::optional<P2kFit> fit_corner(std::span<const double> target, std::span<const Seed> seeds,
                                 const FitOptions& opts);

using FreedomFn = std::function<std::uint32_t()>;

inline constexpr std::uint32_t kAllFree = 0xFFFFFFFFU;

constexpr bool pole_free(std::uint32_t mask, std::size_t si) {
  return (mask >> (3 * si)) & 1U;
}
constexpr bool zero_free(std::uint32_t mask, std::size_t si) {
  return (mask >> (3 * si + 1)) & 1U;
}
constexpr bool scale_free(std::uint32_t mask, std::size_t si) {
  return (mask >> (3 * si + 2)) & 1U;
}
constexpr std::uint32_t pole_bit(std::size_t si) { return 1U << (3 * si); }
constexpr std::uint32_t zero_bit(std::size_t si) { return 1U << (3 * si + 1); }
constexpr std::uint32_t scale_bit(std::size_t si) { return 1U << (3 * si + 2); }

struct StepReport {
  std::size_t section{};
  CornerWords words{};
  double cost{};
};

using StepFn = std::function<void(const StepReport&)>;

struct WatchedFit {
  CornerWords words{};
  StageScales scales{};
  PackedCorner packed{};
  double shape_rms_db{};
  std::string_view seed_used;
  bool stopped{};
};

std::optional<WatchedFit> fit_corner_watched(std::span<const double> target,
                                             std::span<const Seed> seeds, const FitOptions& opts,
                                             const FreedomFn& freedom,
                                             const std::function<bool()>& stop_requested,
                                             const StepFn& on_step);

double stage_db(const std::array<double, 5>& biquad, double hz);
std::vector<double> corner_response_db(const StoredCorner& words, const Grid& g = grid());
StoredCorner interpolate_plane(const std::array<StoredCorner, 4>& corners, float morph, float q);
std::array<StoredCorner, 4> body_corners(std::span<const std::uint8_t> body);
StoredCorner interpolate_body(std::span<const std::uint8_t> body, float morph, float q);
StoredCorner packed_as_words(const PackedCorner& packed);

}  // namespace trench::core::p2k
