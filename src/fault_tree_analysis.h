/// @file fault_tree_analysis.h
/// Fault Tree Analysis.
#ifndef SCRAM_FAULT_TREE_ANALYSIS_H_
#define SCRAM_FAULT_TREE_ANALYSIS_H_

#include <fstream>
#include <map>
#include <set>
#include <string>
#include <queue>

#include <boost/serialization/map.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/unordered_map.hpp>

#include "error.h"
#include "fault_tree.h"
#include "event.h"
#include "superset.h"

class FaultTreeAnalysisTest;

typedef boost::shared_ptr<scram::Event> EventPtr;
typedef boost::shared_ptr<scram::Gate> GatePtr;
typedef boost::shared_ptr<scram::PrimaryEvent> PrimaryEventPtr;

typedef boost::shared_ptr<scram::Superset> SupersetPtr;

typedef boost::shared_ptr<scram::FaultTree> FaultTreePtr;

namespace scram {

class RiskAnalysis;
class Reporter;

/// @class FaultTreeAnalysis
/// Fault tree analysis functionality.
class FaultTreeAnalysis {
  friend class ::FaultTreeAnalysisTest;
  friend class RiskAnalysis;
  friend class Reporter;

 public:
  /// The main constructor of the Fault Tree Analysis.
  /// @param[in] analysis The type of fault tree analysis.
  /// @param[in] approx The kind of approximation for probability calculations.
  /// @param[in] limit_order The maximum limit on minimal cut sets' order.
  /// @param[in] nsums The number of sums in the probability series.
  /// @throws ValueError if any of the parameters are invalid.
  FaultTreeAnalysis(std::string analysis, std::string approx = "no",
                    int limit_order = 20, int nsums = 1000000);

  /// Analyzes the fault tree and performs computations.
  /// This function must be called only after initilizing the tree with or
  /// without its probabilities.
  /// @throws Error if called before tree initialization from an input file.
  /// @note Cut set generator: O_avg(N) O_max(N)
  void Analyze(const FaultTreePtr& fault_tree,
               const std::map<std::string, std::string>& orig_ids,
               bool prob_requested);

  virtual ~FaultTreeAnalysis() {}

 private:
  /// Expands the children of a top or intermediate event to Supersets.
  /// @param[in] inter_index The index number of the parent node.
  /// @param[out] sets The final Supersets from the children.
  /// @throws ValueError if the parent's gate is not recognized.
  /// @note The final sets are dependent on the gate of the parent.
  /// @note O_avg(N, N*logN) O_max(N^2, N^3*logN) where N is a children number.
  void ExpandSets_(int inter_index, std::vector<SupersetPtr>& sets);

  /// Expands sets for OR operator.
  /// @param[in] events_children The indices of the children of the event.
  /// @param[out] sets The final Supersets generated for OR operator.
  /// @param[in] mult The positive or negative event indicator.
  /// @note O_avg(N) O_max(N^2)
  void SetOr_(std::vector<int>& events_children,
              std::vector<SupersetPtr>& sets, int mult = 1);

  /// Expands sets for AND operator.
  /// @param[in] events_children The indices of the children of the event.
  /// @param[out] sets The final Supersets generated for OR operator.
  /// @param[in] mult The positive or negative event indicator.
  /// @note O_avg(N*logN) O_max(N*logN) where N is the number of children.
  void SetAnd_(std::vector<int>& events_children,
               std::vector<SupersetPtr>& sets, int mult = 1);

  /// Finds minimal cut sets from cut sets.
  /// Applys rule 4 to reduce unique cut sets to minimal cut sets.
  /// @param[in] cut_sets Cut sets with primary events.
  /// @param[in] mcs_lower_order Reference minimal cut sets of some order.
  /// @param[in] min_order The order of sets to become minimal.
  /// @note T_avg(N^3 + N^2*logN + N*logN) = O_avg(N^3)
  void FindMCS_(const std::set< std::set<int> >& cut_sets,
                const std::set< std::set<int> >& mcs_lower_order,
                int min_order);

  // -------------------- Algorithm for Cut Sets and Probabilities -----------
  /// Assigns an index to each primary event, and then populates with this
  /// indices new databases of minimal cut sets and primary to integer
  /// converting maps.
  /// @note O_avg(N) O_max(N^2) where N is the total number of tree nodes.
  void AssignIndices_(const FaultTreePtr& fault_tree);

  /// Converts minimal cut sets from indices to strings.
  void SetsToString_();

  /// Calculates a probability of a set of minimal cut sets, which are in OR
  /// relationship with each other. This function is a brute force probability
  /// calculation without approximations.
  /// @param[in] min_cut_sets Sets of indices of primary events.
  /// @param[in] nsums The number of sums in the series.
  /// @returns The total probability.
  /// @note This function drastically modifies min_cut_sets by deleting
  /// sets inside it. This is for better performance.
  /// @note O_avg(M*logM*N*2^N) where N is the number of sets, and M is
  /// the average size of the sets.
  double ProbOr_(std::set< std::set<int> >& min_cut_sets,
                 int nsums = 1000000);

  /// Calculates a probability of a minimal cut set, whose members are in AND
  /// relationship with each other. This function assumes independence of each
  /// member.
  /// @param[in] min_cut_set A set of indices of primary events.
  /// @returns The total probability.
  /// @note O_avg(N) where N is the size of the passed set.
  double ProbAnd_(const std::set<int>& min_cut_set);

  /// Calculates A(and)( B(or)C ) relationship for sets using set algebra.
  /// @param[in] el A set of indices of primary events.
  /// @param[in] set Sets of indices of primary events.
  /// @param[out] combo_set A final set resulting from joining el and sets.
  /// @note O_avg(N*M*logM) where N is the size of the set, and M is the
  /// average size of the elements.
  void CombineElAndSet_(const std::set<int>& el,
                        const std::set< std::set<int> >& set,
                        std::set< std::set<int> >& combo_set);

  std::set< std::set<int> > imcs_;  ///< Min cut sets with indices of events.
  /// Indices min cut sets to strings min cut sets mapping.
  std::map< std::set<int>, std::set<std::string> > imcs_to_smcs_;

  std::vector<PrimaryEventPtr> int_to_prime_;  ///< Primary events from indices.
  /// Indices of primary events.
  boost::unordered_map<std::string, int> prime_to_int_;
  std::vector<double> iprobs_;  ///< Holds probabilities of primary events.

  int top_event_index_;  ///< The index of the top event.
  /// Intermediate events from indices.
  boost::unordered_map<int, GatePtr> int_to_inter_;
  /// Indices of intermediate events.
  boost::unordered_map<std::string, int> inter_to_int_;
  // -----------------------------------------------------------------
  // ---- Algorithm for Equation Construction for Monte Carlo Sim -------
  /// Generates positive and negative terms of probability equation expansion.
  /// @param[in] min_cut_sets Sets of indices of primary events.
  /// @param[in] sign The sign of the series.
  /// @param[in] nsums The number of sums in the series.
  void MProbOr_(std::set< std::set<int> >& min_cut_sets, int sign = 1,
                int nsums = 1000000);

  /// Performs Monte Carlo Simulation.
  /// @todo Implement the simulation.
  void MSample_();

  std::vector< std::set<int> > pos_terms_;  ///< Plus terms of the equation.
  std::vector< std::set<int> > neg_terms_;  ///< Minus terms of the equation.
  std::vector<double> sampled_results_;  ///< Storage for sampled values.
  // -----------------------------------------------------------------
  // ----------------------- Member Variables of this Class -----------------
  /// This member is used to provide any warnings about assumptions,
  /// calculations, and settings. These warnings must be written into output
  /// file.
  std::string warnings_;

  /// Type of analysis to be performed.
  std::string analysis_;

  /// Approximations for probability calculations.
  std::string approx_;

  /// Input file path.
  std::string input_file_;

  /// Indicator if probability calculations are requested.
  bool prob_requested_;

  /// Indicate if analysis of the tree is done.
  bool analysis_done_;

  /// Number of sums in series expansion for probability calculations.
  int nsums_;

  /// Container of original names of events with capitalizations.
  std::map<std::string, std::string> orig_ids_;

  /// List of all valid gates.
  std::set<std::string> gates_;

  /// List of all valid types of primary events.
  std::set<std::string> types_;

  /// Top event.
  GatePtr top_event_;

  /// Holder for intermediate events.
  boost::unordered_map<std::string, GatePtr> inter_events_;

  /// Container for primary events.
  boost::unordered_map<std::string, PrimaryEventPtr> primary_events_;

  /// Container for minimal cut sets.
  std::set< std::set<std::string> > min_cut_sets_;

  /// Container for minimal cut sets and their respective probabilities.
  std::map< std::set<std::string>, double > prob_of_min_sets_;

  /// Container for minimal cut sets ordered by their probabilities.
  std::multimap< double, std::set<std::string> > ordered_min_sets_;

  /// Container for primary events and their contribution.
  std::map< std::string, double > imp_of_primaries_;

  /// Container for primary events ordered by their contribution.
  std::multimap< double, std::string > ordered_primaries_;

  /// Maximum order of the minimal cut sets.
  int max_order_;

  /// Limit on the size of the minimal cut sets for performance reasons.
  int limit_order_;

  /// Total probability of the top event.
  double p_total_;

  // Time logging
  double exp_time_;  ///< Expansion of tree gates time.
  double mcs_time_;  ///< Time for MCS generation.
  double p_time_;  ///< Time for probability calculations.
};

}  // namespace scram

#endif  // SCRAM_FAULT_TREE_ANALYSIS_H_
