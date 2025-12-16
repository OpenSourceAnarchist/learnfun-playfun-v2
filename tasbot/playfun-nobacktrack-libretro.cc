// playfun-nobacktrack-libretro.cc - Greedy playfun using Libretro backend
// Plays games using learned objectives without backtracking

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <deque>
#include <format>
#include <iostream>
#include <string>
#include <vector>

#include "emulator-libretro.h"
#include "simplefm2.h"
#include "weighted-objectives.h"
#include "motifs.h"
#include "tasbot.h"
#include "../cc-lib/arcfour.h"
#include "../cc-lib/util.h"
#include "util.h"

using std::string;
using std::vector;

// Global flag for magnitude scoring (Tom7 TODO)
static bool use_magnitude_scoring = false;

struct PlayFun {
  string game;
  string movie_file;
  ArcFour rc{"playfun"};
  WeightedObjectives* objectives = nullptr;
  Motifs* motifs = nullptr;
  vector<vector<uint8>> motifvec;
  vector<uint8> solution;
  vector<uint8> movie;
  
  // Adaptive futures: track recent quality to adjust search strategy
  // Rolling history of recent future scores (Tom7 TODO: adapt depth based on quality)
  static constexpr size_t HISTORY_SIZE = 50;
  std::deque<double> recent_futures;
  
  // Adaptive depth parameters - when futures are bad, use shorter/more;
  // when good, use longer/fewer (per Tom7's TODO)
  int avoid_depths[2] = {20, 75};
  int seek_depths[3] = {30, 30, 50};
  
  // Selective motif exploration (Tom7 TODO): track motif quality
  // Maps motif index to cumulative score
  std::vector<double> motif_scores;
  size_t motif_uses = 0;
  
  double GetAverageFutureScore() const {
    if (recent_futures.empty()) return 0.0;
    double sum = 0.0;
    for (double d : recent_futures) sum += d;
    return sum / static_cast<double>(recent_futures.size());
  }
  
  void RecordFutureScore(double score) {
    recent_futures.push_back(score);
    while (recent_futures.size() > HISTORY_SIZE) {
      recent_futures.pop_front();
    }
  }
  
  void AdaptFutureDepths() {
    // Don't adapt until we have enough history
    if (recent_futures.size() < HISTORY_SIZE / 2) return;
    
    double avg = GetAverageFutureScore();
    
    // Tom7's insight: bad futures → shorter & more; good futures → longer & fewer
    // Threshold: avg < 0.3 = bad, avg > 0.7 = good
    if (avg < 0.3) {
      // Futures are bad - shorten depths, search wider
      avoid_depths[0] = 10;  avoid_depths[1] = 30;
      seek_depths[0] = 15;   seek_depths[1] = 15;  seek_depths[2] = 25;
    } else if (avg > 0.7) {
      // Futures are good - lengthen depths, search deeper
      avoid_depths[0] = 40;  avoid_depths[1] = 150;
      seek_depths[0] = 50;   seek_depths[1] = 50;  seek_depths[2] = 100;
    } else {
      // Middle ground - default values
      avoid_depths[0] = 20;  avoid_depths[1] = 75;
      seek_depths[0] = 30;   seek_depths[1] = 30;  seek_depths[2] = 50;
    }
  }
  
  // Tom7 TODO: "Don't bother trying every next. Pick the best half,
  // and some random subset of the rest. Use the time instead to explore more futures."
  std::vector<size_t> SelectMotifsToTry() {
    const size_t n = motifvec.size();
    std::vector<size_t> indices(n);
    for (size_t i = 0; i < n; i++) indices[i] = i;
    
    // Early: try all motifs until we have usage data
    if (motif_uses < 100) {
      Shuffle(&indices);
      return indices;
    }
    
    // Sort by score (best first)
    std::vector<std::pair<double, size_t>> scored;
    for (size_t i = 0; i < n; i++) {
      scored.push_back({motif_scores[i], i});
    }
    std::sort(scored.begin(), scored.end(), 
              [](const auto& a, const auto& b) { return a.first > b.first; });
    
    std::vector<size_t> selected;
    
    // Take best half
    size_t best_half = n / 2;
    for (size_t i = 0; i < best_half; i++) {
      selected.push_back(scored[i].second);
    }
    
    // Random subset from the rest (about 25% of the remaining)
    for (size_t i = best_half; i < n; i++) {
      if (rc.Byte() < 64) {  // ~25% chance
        selected.push_back(scored[i].second);
      }
    }
    
    // Shuffle so we don't always try in score order
    Shuffle(&selected);
    return selected;
  }
  
  void UpdateMotifScore(size_t motif_idx, double score) {
    if (motif_scores.empty()) {
      motif_scores.resize(motifvec.size(), 0.0);
    }
    // Exponential moving average
    motif_scores[motif_idx] = motif_scores[motif_idx] * 0.95 + score * 0.05;
    motif_uses++;
  }
  
  // Score memory change using either binary or magnitude scoring
  double ScoreChange(const vector<uint8>& mem1, const vector<uint8>& mem2) const {
    if (use_magnitude_scoring) {
      return objectives->EvaluateMagnitude(mem1, mem2);
    } else {
      return objectives->Evaluate(mem1, mem2);
    }
  }

  PlayFun(const string& g, const string& m) : game(g), movie_file(m) {
    objectives = WeightedObjectives::LoadFromFile(game + ".objectives");
    CHECK(objectives);
    std::cerr << std::format("Loaded {} objective functions\n", objectives->Size());

    motifs = Motifs::LoadFromFile(game + ".motifs");
    CHECK(motifs);

    EmulatorLibretro::ResetCache(100000, 10000);
    motifvec = motifs->AllMotifs();

    solution = SimpleFM2::ReadInputs(movie_file);
    
    // Fast-forward past initial idle
    size_t start = 0;
    while (start < solution.size()) {
      EmulatorLibretro::Step(solution[start]);
      movie.push_back(solution[start]);
      if (solution[start] != 0) break;
      start++;
    }
    std::cout << std::format("Skipped {} frames until first keypress.\n", start);
  }

  double AvoidBadFutures(const vector<uint8>& base_memory) {
    // Use adaptive depths (member variable, not static)
    vector<uint8> base_state;
    EmulatorLibretro::SaveUncompressed(&base_state);

    double total = 1.0;
    for (size_t i = 0; i < 2; i++) {
      if (i) EmulatorLibretro::LoadUncompressed(&base_state);
      for (int d = 0; d < avoid_depths[i]; d++) {
        const vector<uint8>& m = motifs->RandomWeightedMotif();
        for (size_t x = 0; x < m.size(); x++) {
          EmulatorLibretro::CachingStep(m[x]);
          vector<uint8> future_memory;
          EmulatorLibretro::GetMemory(&future_memory);
          double score = ScoreChange(base_memory, future_memory);
          total = (i || d || x) ? std::min(total, score) : score;
        }
      }
    }
    return total;
  }

  double SeekGoodFutures(const vector<uint8>& base_memory) {
    // Use adaptive depths (member variable, not static)
    vector<uint8> base_state;
    EmulatorLibretro::SaveUncompressed(&base_state);

    double total = 1.0;
    for (size_t i = 0; i < 3; i++) {
      if (i) EmulatorLibretro::LoadUncompressed(&base_state);
      for (int d = 0; d < seek_depths[i]; d++) {
        const vector<uint8>& m = motifs->RandomWeightedMotif();
        for (size_t x = 0; x < m.size(); x++) {
          EmulatorLibretro::CachingStep(m[x]);
        }
      }

      vector<uint8> future_memory;
      EmulatorLibretro::GetMemory(&future_memory);
      double score = ScoreChange(base_memory, future_memory);
      total = i ? std::max(total, score) : score;
    }
    return total;
  }

  void Greedy() {
    vector<vector<uint8>> memories;
    vector<uint8> current_state;
    vector<uint8> current_memory;

    static const int NUMFRAMES = 10000;
    for (int framenum = 0; framenum < NUMFRAMES; framenum++) {
      EmulatorLibretro::SaveUncompressed(&current_state);
      EmulatorLibretro::GetMemory(&current_memory);
      memories.push_back(current_memory);

      // Tom7 TODO: "Don't bother trying every next. Pick the best half,
      // and some random subset of the rest."
      std::vector<size_t> motifs_to_try = SelectMotifsToTry();

      double best_score = -999999999.0;
      double best_future = 0.0, best_immediate = 0.0;
      size_t best_motif_idx = 0;
      
      for (size_t trial = 0; trial < motifs_to_try.size(); trial++) {
        size_t motif_idx = motifs_to_try[trial];
        if (trial != 0) EmulatorLibretro::LoadUncompressed(&current_state);
        
        for (size_t j = 0; j < motifvec[motif_idx].size(); j++) {
          EmulatorLibretro::CachingStep(motifvec[motif_idx][j]);
        }

        vector<uint8> new_memory;
        EmulatorLibretro::GetMemory(&new_memory);
        vector<uint8> new_state;
        EmulatorLibretro::SaveUncompressed(&new_state);
        
        double immediate_score = ScoreChange(current_memory, new_memory);
        double future_score = AvoidBadFutures(new_memory);
        
        EmulatorLibretro::LoadUncompressed(&new_state);
        future_score += SeekGoodFutures(new_memory);

        double score = immediate_score + future_score;
        
        // Update motif quality tracking
        UpdateMotifScore(motif_idx, score);

        if (score > best_score) {
          best_score = score;
          best_immediate = immediate_score;
          best_future = future_score;
          best_motif_idx = motif_idx;
        }
      }

      std::cout << std::format("{:8} best score {:.2f} ({:.2f} + {:.2f} future) [tried {}/{}]\n",
                               movie.size(), best_score, best_immediate, best_future,
                               motifs_to_try.size(), motifvec.size());

      // Track future quality and adapt search depth (Tom7 TODO)
      RecordFutureScore(best_future);
      AdaptFutureDepths();
      
      // Occasionally report adaptive state
      if (framenum % 100 == 0) {
        std::cout << std::format("         [adaptive: avg_future={:.2f}, avoid=[{},{}], seek=[{},{},{}]]\n",
                                 GetAverageFutureScore(),
                                 avoid_depths[0], avoid_depths[1],
                                 seek_depths[0], seek_depths[1], seek_depths[2]);
      }

      EmulatorLibretro::LoadUncompressed(&current_state);
      const vector<uint8>& best_motif = motifvec[best_motif_idx];
      for (size_t j = 0; j < best_motif.size(); j++) {
        EmulatorLibretro::CachingStep(best_motif[j]);
        movie.push_back(best_motif[j]);
      }

      if (framenum % 10 == 0) {
        SimpleFM2::WriteInputs(game + "-playfun-motif-progress.fm2", 
                               game + ".nes",
                               "base64:Ww5XFVjIx5aTe5avRpVhxg==",
                               movie);
        objectives->SaveSVG(memories, game + "-playfun.svg");
        EmulatorLibretro::PrintCacheStats();
        std::cout << "                     (wrote)\n";
      }
    }

    SimpleFM2::WriteInputs(game + "-playfun-motif-final.fm2",
                           game + ".nes",
                           "base64:Ww5XFVjIx5aTe5avRpVhxg==",
                           movie);
  }
};

static void PrintUsage(const char* prog) {
  std::cerr << std::format("Usage: {} [options] <game> <movie.fm2>\n", prog);
  std::cerr << "       " << prog << " (uses smb with smb-walk.fm2)\n";
  std::cerr << "\nOptions:\n";
  std::cerr << "  --core /path/to/core.so  Use a specific Libretro core\n";
  std::cerr << "  --magnitude              Use magnitude-weighted scoring (Tom7 TODO)\n";
  std::cerr << "  --help, -h               Show this help\n";
}

int main(int argc, char* argv[]) {
  string game, movie, core_path;

  // Parse args
  for (int i = 1; i < argc; i++) {
    string arg = argv[i];
    if (arg == "--core" && i + 1 < argc) {
      core_path = argv[++i];
    } else if (arg == "--magnitude") {
      use_magnitude_scoring = true;
    } else if (arg == "--help" || arg == "-h") {
      PrintUsage(argv[0]);
      return 0;
    } else if (game.empty()) {
      game = arg;
      if (game.ends_with(".nes")) {
        game = game.substr(0, game.size() - 4);
      }
    } else if (movie.empty()) {
      movie = arg;
    }
  }
  if (game.empty()) game = "smb";
  if (movie.empty()) movie = "smb-walk.fm2";

  // Environment override for core path
  if (core_path.empty()) {
    if (const char* env = getenv("LIBRETRO_CORE")) {
      core_path = env;
    }
  }

  std::cerr << std::format("Starting playfun for {}...\n", game);

  bool ok;
  if (core_path.empty()) {
    ok = EmulatorLibretro::Initialize(game + ".nes");
  } else {
    ok = EmulatorLibretro::Initialize(core_path, game + ".nes");
  }

  if (!ok) {
    std::cerr << "Failed to initialize emulator\n";
    return 1;
  }

  PlayFun pf(game, movie);
  pf.Greedy();

  EmulatorLibretro::Shutdown();
  return 0;
}
