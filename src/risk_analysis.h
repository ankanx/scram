/*
 * Copyright (C) 2014-2015 Olzhas Rakhimov
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
/// @file risk_analysis.h
/// Contains the main system for performing analysis.
#ifndef SCRAM_SRC_RISK_ANALYISIS_H_
#define SCRAM_SRC_RISK_ANALYISIS_H_

#include <iostream>
#include <map>
#include <string>
#include <vector>

#include <boost/shared_ptr.hpp>

#include "fault_tree_analysis.h"
#include "probability_analysis.h"
#include "settings.h"
#include "uncertainty_analysis.h"

namespace scram {

class Gate;
class FaultTree;
class Model;

/// @class RiskAnalysis
/// Main system that performs analyses.
class RiskAnalysis {
 public:
  typedef boost::shared_ptr<Model> ModelPtr;
  typedef boost::shared_ptr<FaultTreeAnalysis> FaultTreeAnalysisPtr;
  typedef boost::shared_ptr<ProbabilityAnalysis> ProbabilityAnalysisPtr;
  typedef boost::shared_ptr<UncertaintyAnalysis> UncertaintyAnalysisPtr;

  /// Constructs RiskAnalysis with a valid model and analysis settings.
  ///
  /// @param[in] model An analysis model with fault trees, events, etc.
  /// @param[in] settings Analysis settings for the given model.
  RiskAnalysis(const ModelPtr& model, const Settings& settings);

  /// Provides graphing instructions
  /// for each fault tree initialized in the analysis.
  /// All top events from fault trees are processed
  /// into output files named with fault tree and top event names.
  ///
  /// @throws IOError The output file cannot be accessed for writing.
  ///
  /// @note This function must be called
  ///       only after initializing the tree
  ///       with or without its probabilities.
  void GraphingInstructions();

  /// Performs the main analysis operations.
  /// Analyzes the fault tree and performs computations.
  ///
  /// @note This function must be called
  ///       only after initializing the tree
  ///       with or without its probabilities.
  void Analyze();

  /// Reports all results generated by all analyses
  /// into XML formatted stream.
  /// The report is appended to the stream.
  ///
  /// @param[out] out The output stream.
  ///
  /// @note This function must be called only after Analyze() function.
  void Report(std::ostream& out);

  /// Reports the results of analyses
  /// to a specified output destination.
  /// This function overwrites the file.
  ///
  /// @param[out] output The output destination.
  ///
  /// @throws IOError The output file is not accessible.
  ///
  /// @note This function must be called only after Analyze() function.
  void Report(std::string output);

  /// @returns Fault tree analyses performed on one-top-event fault trees.
  ///          The top gate identifier is used as the analysis identifier.
  inline const std::map<std::string, FaultTreeAnalysisPtr>&
      fault_tree_analyses() const {
    return fault_tree_analyses_;
  }

  /// @returns Probability analysis performed on
  ///          minimal cut sets generated by
  ///          fault tree analyses.
  inline const std::map<std::string, ProbabilityAnalysisPtr>&
      probability_analyses() const {
    return probability_analyses_;
  }

  /// @returns Uncertainty analyses performed on
  ///          minimal cut sets generated by
  ///          fault tree analyses.
  inline const std::map<std::string, UncertaintyAnalysisPtr>
      uncertainty_analyses() const {
    return uncertainty_analyses_;
  }

 private:
  typedef boost::shared_ptr<Gate> GatePtr;
  typedef boost::shared_ptr<FaultTree> FaultTreePtr;

  ModelPtr model_;  ///< Analysis model with constructs.
  Settings settings_;  ///< Settings for analysis.

  /// Fault tree analyses that are performed on a specific fault tree.
  std::map<std::string, FaultTreeAnalysisPtr> fault_tree_analyses_;

  /// Probability analyses that are performed on a specific fault tree.
  std::map<std::string, ProbabilityAnalysisPtr> probability_analyses_;

  /// Uncertainty analyses that are performed on a specific fault tree.
  std::map<std::string, UncertaintyAnalysisPtr> uncertainty_analyses_;
};

}  // namespace scram

#endif  // SCRAM_SRC_RISK_ANALYSIS_H_
