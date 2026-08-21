#include "trench/core/p2k.hpp"

#include <cmath>
#include <tuple>

namespace trench::core::p2k {

namespace {

std::pair<double, bool> stage_moves_masked(Corner& c, std::span<const double> target,
                                           std::size_t si, double best, std::uint32_t mask,
                                           bool fine, Scratch& s, const LossFn* loss) {
  bool moved = false;
  for (std::size_t wi = 0; wi < 4; ++wi) {
    if (si == 5 && wi == 1) {
      continue;
    }
    if (wi < 2 ? !zero_free(mask, si) : !pole_free(mask, si)) {
      continue;
    }
    const auto [v, ch] =
        fine ? sweep_axis_fine_cost(c, target, si, wi, best, Cost::kWeightedVar, s, loss)
             : sweep_axis_cost(c, target, si, wi, best, Cost::kWeightedVar, s, loss);
    best = v;
    moved |= ch;
  }
  for (std::size_t root = 0; root < 2; ++root) {
    if (si == 5 && root == 0) {
      continue;
    }
    if (root == 0 ? !zero_free(mask, si) : !pole_free(mask, si)) {
      continue;
    }
    const auto [v, ch] =
        sweep_radius_cost(c, target, si, root, best, Cost::kWeightedVar, s, loss);
    best = v;
    moved |= ch;
  }
  return {best, moved};
}

struct WatchedRun {
  Corner corner;
  double var{};
  bool stopped{};
};

WatchedRun watched_polish(const CornerWords& words, std::span<const double> target,
                          std::size_t max_passes, const FreedomFn& freedom,
                          const std::function<bool()>& stop_requested, const StepFn& on_step,
                          const LossFn* loss) {
  WatchedRun run{Corner::from_words(words), 0.0, false};
  Corner& c = run.corner;
  Scratch s;
  double best = corner_cost(c, target, Cost::kWeightedVar, s, loss);

  const auto step = [&](std::size_t si, bool fine) {
    if (stop_requested && stop_requested()) {
      run.stopped = true;
      return std::pair{false, false};
    }
    const std::uint32_t mask = freedom ? freedom() : kAllFree;
    const auto [v, moved] = stage_moves_masked(c, target, si, best, mask, fine, s, loss);
    best = v;
    if (moved && on_step) {
      on_step(StepReport{si, c.w, best});
    }
    return std::pair{true, moved};
  };

  for (std::size_t si = 0; si < kStageCount && !run.stopped; ++si) {
    for (std::size_t pass = 0; pass < 4; ++pass) {
      const auto [alive, moved] = step(si, false);
      if (!alive || !moved) {
        break;
      }
    }
  }
  for (std::size_t pass = 0; pass < max_passes && !run.stopped; ++pass) {
    bool changed = false;
    for (std::size_t si = 0; si < kStageCount && !run.stopped; ++si) {
      const auto [alive, moved] = step(si, false);
      changed |= alive && moved;
    }
    if (!changed) {
      break;
    }
  }
  if (!run.stopped) {
    best = corner_cost(c, target, Cost::kWeightedVar, s, loss);
  }
  for (std::size_t pass = 0; pass < max_passes && !run.stopped; ++pass) {
    bool changed = false;
    for (std::size_t si = 0; si < kStageCount && !run.stopped; ++si) {
      if (stop_requested && stop_requested()) {
        run.stopped = true;
        break;
      }
      const std::uint32_t mask = freedom ? freedom() : kAllFree;
      bool moved_any = false;
      for (std::size_t wi = 0; wi < 4; ++wi) {
        if (si == 5 && wi == 1) {
          continue;
        }
        if (wi < 2 ? !zero_free(mask, si) : !pole_free(mask, si)) {
          continue;
        }
        const auto [v, ch] =
            sweep_axis_fine_cost(c, target, si, wi, best, Cost::kWeightedVar, s, loss);
        best = v;
        moved_any |= ch;
      }
      for (std::size_t root = 0; root < 2; ++root) {
        if (si == 5 && root == 0) {
          continue;
        }
        if (root == 0 ? !zero_free(mask, si) : !pole_free(mask, si)) {
          continue;
        }
        const auto [v, ch] =
            sweep_radius_cost(c, target, si, root, best, Cost::kWeightedVar, s, loss);
        best = v;
        moved_any |= ch;
      }
      if (moved_any && on_step) {
        on_step(StepReport{si, c.w, best});
      }
      changed |= moved_any;
    }
    if (!changed) {
      break;
    }
  }
  run.var = best;
  return run;
}

}  // namespace

std::optional<WatchedFit> fit_corner_watched(std::span<const double> target,
                                             std::span<const Seed> seeds, const FitOptions& opts,
                                             const FreedomFn& freedom,
                                             const std::function<bool()>& stop_requested,
                                             const StepFn& on_step) {
  if (target.size() != kNpts) {
    return std::nullopt;
  }
  const LossFn* loss = opts.loss ? &opts.loss : nullptr;
  const auto scales_held = [&](std::uint32_t mask) {
    for (std::size_t si = 0; si < kStageCount; ++si) {
      if (!scale_free(mask, si)) {
        return true;
      }
    }
    return false;
  };
  if (freedom && scales_held(freedom()) && !opts.baseline) {
    return std::nullopt;
  }
  std::optional<std::tuple<Corner, double, std::string_view, bool>> best;

  for (const auto& seed : seeds) {
    if (stop_requested && stop_requested() && best) {
      break;
    }
    CornerWords words{};
    std::string_view label;
    if (std::holds_alternative<SeedContinuous>(seed)) {
      if (!opts.allow_continuous) {
        continue;
      }
      const auto cont = continuous_best(target);
      if (!cont) {
        continue;
      }
      words = words_from_continuous(std::get<1>(*cont));
      label = "continuous";
    } else if (std::holds_alternative<SeedPeel>(seed)) {
      words = peel_seed(target);
      label = "peel";
    } else {
      words = std::get<CornerWords>(seed);
      label = "rom";
    }
    auto run = watched_polish(enter(words), target, opts.max_passes, freedom, stop_requested,
                              on_step, loss);
    const double rms = std::sqrt(run.var);
    if (!best || rms < std::get<1>(*best)) {
      best.emplace(std::move(run.corner), rms, label, run.stopped);
    }
    if (run.stopped) {
      break;
    }
  }

  if (!best) {
    return std::nullopt;
  }
  auto& [c, rms, label, stopped] = *best;
  const std::uint32_t mask = freedom ? freedom() : kAllFree;
  WatchedFit fit;
  if (scales_held(mask)) {
    if (!opts.baseline) {
      return std::nullopt;
    }
    fit.scales = stage_gain_pass_held(c, mask, *opts.baseline);
    fit.packed = pack_corner(c, fit.scales);
    for (std::size_t si = 0; si < kStageCount; ++si) {
      if (!scale_free(mask, si)) {
        fit.packed[si * kWordCount + 4] = (*opts.baseline)[si * kWordCount + 4];
      }
    }
  } else {
    fit.scales = stage_gain_pass(c);
    fit.packed = pack_corner(c, fit.scales);
  }
  fit.words = c.w;
  fit.shape_rms_db = rms;
  fit.seed_used = label;
  fit.stopped = stopped;
  return fit;
}

}  // namespace trench::core::p2k
