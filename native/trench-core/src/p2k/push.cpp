#include "trench/core/p2k_push.hpp"

#include <Eigen/Dense>

#include <cmath>
#include <cstddef>
#include <cstdint>

namespace trench::core::p2k {

namespace {

struct Candidate {
  std::size_t section{};
  std::size_t word{};
  std::uint16_t value{};
};

std::vector<Candidate> enumerate_candidates(const StoredCorner& corner,
                                            std::uint32_t freedom_mask) {
  const auto& words = lattice_words();
  const auto len = lattice_len();
  std::vector<Candidate> out;
  for (std::size_t si = 0; si < kStageCount; ++si) {
    for (int root = 0; root < 2; ++root) {
      const bool is_pole = root == 1;
      const bool free_root =
          is_pole ? pole_free(freedom_mask, si) : zero_free(freedom_mask, si);
      if (!free_root) continue;
      const std::size_t mag_wi = is_pole ? 2 : 0;
      const std::size_t rsq_wi = is_pole ? 3 : 1;
      for (const auto wi : {mag_wi, rsq_wi}) {
        if (si == 5 && wi == 1) continue;
        const auto current = corner[si][wi];
        const auto index = nearest_lattice_word(current);
        for (const int step : {-1, 1}) {
          const auto neighbour = static_cast<std::ptrdiff_t>(index) + step;
          if (neighbour < 0 || neighbour >= static_cast<std::ptrdiff_t>(len)) continue;
          const auto candidate_word = words[static_cast<std::size_t>(neighbour)];
          if (candidate_word == current) continue;
          const auto mag =
              wi == mag_wi ? candidate_word : corner[si][mag_wi];
          const auto rsq =
              wi == rsq_wi ? candidate_word : corner[si][rsq_wi];
          if (!magnitude_admissible(nearest_lattice_word(mag), is_pole)) continue;
          const auto [p, q] = pq(mag, rsq);
          if (!is_legal(p, q, is_pole)) continue;
          out.push_back({si, wi, candidate_word});
        }
      }
    }
  }
  return out;
}

}  // namespace

Push compute_push(const StoredCorner& corner, std::uint32_t freedom_mask,
                  std::size_t max_directions, const Grid& g) {
  Push push;
  const auto candidates = enumerate_candidates(corner, freedom_mask);
  push.candidate_count = candidates.size();
  if (candidates.empty()) return push;

  const auto base = corner_response_db(corner, g);

  Eigen::MatrixXd rows(static_cast<Eigen::Index>(candidates.size()),
                       static_cast<Eigen::Index>(kNpts));
  std::vector<double> root_weight(kNpts);
  for (std::size_t k = 0; k < kNpts; ++k) root_weight[k] = std::sqrt(g.weight[k]);

  for (std::size_t ci = 0; ci < candidates.size(); ++ci) {
    auto varied = corner;
    varied[candidates[ci].section][candidates[ci].word] = candidates[ci].value;
    const auto response = corner_response_db(varied, g);
    double weighted_mean = 0.0;
    for (std::size_t k = 0; k < kNpts; ++k) {
      weighted_mean += g.weight[k] * (response[k] - base[k]);
    }
    weighted_mean /= g.weight_sum;
    for (std::size_t k = 0; k < kNpts; ++k) {
      rows(static_cast<Eigen::Index>(ci), static_cast<Eigen::Index>(k)) =
          root_weight[k] * (response[k] - base[k] - weighted_mean);
    }
  }

  Eigen::BDCSVD<Eigen::MatrixXd> svd(rows, Eigen::ComputeThinV);
  const auto rank = std::min<std::size_t>(
      max_directions, static_cast<std::size_t>(svd.singularValues().size()));
  for (std::size_t di = 0; di < rank; ++di) {
    const auto sigma = svd.singularValues()(static_cast<Eigen::Index>(di));
    if (!(sigma > 0.0)) break;
    PushDirection direction;
    direction.sigma = sigma;
    double norm_sq = 0.0;
    for (std::size_t k = 0; k < kNpts; ++k) {
      const auto value =
          svd.matrixV()(static_cast<Eigen::Index>(k), static_cast<Eigen::Index>(di)) /
          root_weight[k];
      direction.curve[k] = value;
      norm_sq += g.weight[k] * value * value;
    }
    const auto norm = std::sqrt(norm_sq);
    for (auto& value : direction.curve) value /= norm;
    push.directions.push_back(direction);
  }
  return push;
}

}  // namespace trench::core::p2k
