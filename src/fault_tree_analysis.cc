/// @file fault_tree_analysis.cc
/// Implementation of fault tree analysis.
#include "fault_tree_analysis.h"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/date_time.hpp>

namespace fs = boost::filesystem;
namespace pt = boost::posix_time;

namespace scram {

FaultTreeAnalysis::FaultTreeAnalysis(std::string analysis,
                                     bool graph_only,
                                     std::string approx,
                                     int limit_order, int nsums)
    : graph_only_(graph_only),
      warnings_(""),
      top_event_id_(""),
      top_event_index_(-1),
      input_file_("deal_in_future"),
      prob_requested_(false),
      analysis_done_(false),
      max_order_(1),
      p_total_(0),
      exp_time_(0),
      mcs_time_(0),
      p_time_(0) {
  // Check for valid analysis type.
  if (analysis != "default" && analysis != "mc") {
    std::string msg = "The analysis type is not recognized.";
    throw scram::ValueError(msg);
  }
  analysis_ = analysis;

  // Check for right limit order.
  if (limit_order < 1) {
    std::string msg = "The limit on the order of minimal cut sets "
                      "cannot be less than one.";
    throw scram::ValueError(msg);
  }
  limit_order_ = limit_order;

  // Check for right limit order.
  if (nsums < 1) {
    std::string msg = "The number of sums in the probability calculation "
                      "cannot be less than one";
    throw scram::ValueError(msg);
  }
  nsums_ = nsums;

  // Check the right approximation for probability calculations.
  if (approx != "no" && approx != "rare" && approx != "mcub") {
    std::string msg = "The probability approximation is not recognized.";
    throw scram::ValueError(msg);
  }
  approx_ = approx;

  // Add valid gates.
  gates_.insert("and");
  gates_.insert("or");
  gates_.insert("not");
  gates_.insert("nor");
  gates_.insert("nand");
  gates_.insert("xor");
  gates_.insert("null");
  gates_.insert("inhibit");
  gates_.insert("vote");

  // Add valid primary event types.
  types_.insert("basic");
  types_.insert("undeveloped");
  types_.insert("house");
  types_.insert("conditional");

  // Pointer to the top event.
  GatePtr top_event_;

  // Initialize a fault tree with a default name.
  FaultTreePtr fault_tree_;
}

void FaultTreeAnalysis::GraphingInstructions(const FaultTreePtr& fault_tree) {
  // List inter events and their children.
  // List inter events and primary events' descriptions.
  // Getting events from the fault tree object.

  top_event_ = fault_tree->top_event();
  inter_events_ = fault_tree->inter_events();
  primary_events_ = fault_tree->primary_events();

  std::string graph_name = "graph.dot";
  graph_name.erase(graph_name.find_last_of("."), std::string::npos);

  std::string output_path = graph_name + ".dot";

  graph_name = graph_name.substr(graph_name.find_last_of("/") +
                                 1, std::string::npos);
  std::ofstream out(output_path.c_str());
  if (!out.good()) {
    throw scram::IOError(output_path +  " : Cannot write the graphing file.");
  }

  boost::to_upper(graph_name);
  out << "digraph " << graph_name << " {\n";

  // Write top event.
  // Keep track of number of repetitions of the primary events.
  std::map<std::string, int> pr_repeat;
  // Populate intermediate and primary events of the top.
  FaultTreeAnalysis::GraphNode_(top_event_, pr_repeat, out);
  out.flush();
  // Do the same for all intermediate events.
  boost::unordered_map<std::string, GatePtr>::iterator it_inter;
  for (it_inter = inter_events_.begin(); it_inter != inter_events_.end();
       ++it_inter) {
    FaultTreeAnalysis::GraphNode_(it_inter->second, pr_repeat, out);
    out.flush();
  }

  /// @todo Change to re-use of gates.
  /// @deprecated Re-use of gates instead of transfer symbols.
  /*
  // Do the same for all transfers.
  std::pair<std::string, std::string> tr_pair;
  while (!transfers_.empty()) {
    tr_pair = transfers_.front();
    transfers_.pop();
    out << "\"" <<  orig_ids_[tr_pair.first] << "\" -> "
        << "\"" << orig_ids_[tr_pair.second] <<"\";\n";
    // Apply format.
    std::string tr_name = orig_ids_[tr_pair.second];
    tr_name = tr_name.substr(tr_name.find_last_of("/") + 1, std::string::npos);
    out << "\"" << orig_ids_[tr_pair.second] << "\" [shape=triangle, "
        << "fontsize=10, fontcolor=black, fontname=\"times-bold\", "
        << "label=\"" << tr_name << "\"]\n";
  }
  */

  // Format events.
  std::map<std::string, std::string> gate_colors;
  gate_colors.insert(std::make_pair("or", "blue"));
  gate_colors.insert(std::make_pair("and", "green"));
  gate_colors.insert(std::make_pair("not", "red"));
  gate_colors.insert(std::make_pair("xor", "brown"));
  gate_colors.insert(std::make_pair("inhibit", "yellow"));
  gate_colors.insert(std::make_pair("vote", "cyan"));
  gate_colors.insert(std::make_pair("null", "gray"));
  gate_colors.insert(std::make_pair("nor", "magenta"));
  gate_colors.insert(std::make_pair("nand", "orange"));
  std::string gate = top_event_->type();
  boost::to_upper(gate);
  out << "\"" <<  orig_ids_[top_event_id_] << "\" [shape=ellipse, "
      << "fontsize=12, fontcolor=black, fontname=\"times-bold\", "
      << "color=" << gate_colors[top_event_->type()] << ", "
      << "label=\"" << orig_ids_[top_event_id_] << "\\n"
      << "{ " << gate;
  if (gate == "VOTE") {
    out << " " << top_event_->vote_number() << "/"
        << top_event_->children().size();
  }
  out << " }\"]\n";
  for (it_inter = inter_events_.begin(); it_inter != inter_events_.end();
       ++it_inter) {
    gate = it_inter->second->type();
    boost::to_upper(gate);
    out << "\"" <<  orig_ids_[it_inter->first] << "\" [shape=box, "
        << "fontsize=11, fontcolor=black, "
        << "color=" << gate_colors[it_inter->second->type()] << ", "
        << "label=\"" << orig_ids_[it_inter->first] << "\\n"
        << "{ " << gate;
    if (gate == "VOTE") {
      out << " " << it_inter->second->vote_number() << "/"
          << it_inter->second->children().size();
    }
    out << " }\"]\n";
  }
  out.flush();

  std::map<std::string, std::string> event_colors;
  event_colors.insert(std::make_pair("basic", "black"));
  event_colors.insert(std::make_pair("undeveloped", "blue"));
  event_colors.insert(std::make_pair("house", "green"));
  event_colors.insert(std::make_pair("conditional", "red"));
  std::map<std::string, int>::iterator it;
  for (it = pr_repeat.begin(); it != pr_repeat.end(); ++it) {
    for (int i = 0; i < it->second + 1; ++i) {
      out << "\"" << orig_ids_[it->first] << "_R" << i << "\" [shape=circle, "
          << "height=1, fontsize=10, fixedsize=true, "
          << "fontcolor=" << event_colors[primary_events_[it->first]->type()]
          << ", " << "label=\"" << orig_ids_[it->first] << "\\n["
          << primary_events_[it->first]->type() << "]";
      if (prob_requested_) { out << "\\n" << primary_events_[it->first]->p(); }
      out << "\"]\n";
    }
  }

  out << "}";
  out.flush();
}

void FaultTreeAnalysis::Analyze(
    const FaultTreePtr& fault_tree,
    const std::map<std::string, std::string>& orig_ids,
    bool prob_requested) {

  std::cout << analysis_ << std::endl;
  std::cout << approx_ << std::endl;

  // Timing Initialization
  std::clock_t start_time;
  start_time = std::clock();
  // End of Timing Initialization

  // Container for cut sets with intermediate events.
  std::vector< SupersetPtr > inter_sets;

  // Container for cut sets with primary events only.
  std::vector< std::set<int> > cut_sets;

  orig_ids_ = orig_ids;
  prob_requested_ = prob_requested;

  FaultTreeAnalysis::AssignIndices_(fault_tree);

  FaultTreeAnalysis::ExpandSets_(top_event_index_, inter_sets);

  // An iterator for a vector with sets of ids of events.
  std::vector< std::set<int> >::iterator it_vec;

  // An iterator for a vector with Supersets.
  std::vector< SupersetPtr >::iterator it_sup;

  // Generate cut sets.
  while (!inter_sets.empty()) {
    // Get rightmost set.
    SupersetPtr tmp_set = inter_sets.back();
    // Delete rightmost set.
    inter_sets.pop_back();

    // Discard this tmp set if it is larger than the limit.
    if (tmp_set->NumOfPrimeEvents() > limit_order_) continue;

    if (tmp_set->NumOfGates() == 0) {
      // This is a set with primary events only.
      cut_sets.push_back(tmp_set->primes());
      continue;
    }

    // To hold sets of children.
    std::vector< SupersetPtr > children_sets;

    FaultTreeAnalysis::ExpandSets_(tmp_set->PopGate(), children_sets);

    // Attach the original set into this event's sets of children.
    for (it_sup = children_sets.begin(); it_sup != children_sets.end();
         ++it_sup) {
      // Add this set to the original inter_sets.
      if ((*it_sup)->InsertSet(tmp_set)) inter_sets.push_back(*it_sup);
    }
  }

  // Duration of the expansion.
  exp_time_ = (std::clock() - start_time) / static_cast<double>(CLOCKS_PER_SEC);

  // At this point cut sets are generated.
  // Now we need to reduce them to minimal cut sets.

  // First, defensive check if cut sets exist for the specified limit order.
  if (cut_sets.empty()) {
    std::stringstream msg;
    msg << "No cut sets for the limit order " <<  limit_order_;
    warnings_ += msg.str();
    analysis_done_ = true;
    return;
  }

  // Choose to convert vector to a set to get rid of any duplications.
  std::set< std::set<int> > unique_cut_sets;
  for (it_vec = cut_sets.begin(); it_vec != cut_sets.end(); ++it_vec) {
    if (it_vec->size() == 1) {
      // Minimal cut set is detected.
      imcs_.insert(*it_vec);
      continue;
    }
    unique_cut_sets.insert(*it_vec);
  }

  FaultTreeAnalysis::FindMCS_(unique_cut_sets, imcs_, 2);
  // Duration of MCS generation.
  mcs_time_ = (std::clock() - start_time) / static_cast<double>(CLOCKS_PER_SEC);
  FaultTreeAnalysis::SetsToString_();  // MCS with event ids.

  analysis_done_ = true;  // Main analysis enough for reporting is done.

  // Compute probabilities only if requested.
  if (!prob_requested_) return;

  // Maximum number of sums in the series.
  if (nsums_ > imcs_.size()) nsums_ = imcs_.size();

  // Perform Monte Carlo Uncertainty analysis.
  if (analysis_ == "mc") {
    // Generate the equation.
    FaultTreeAnalysis::MProbOr_(imcs_, 1, nsums_);
    // Sample probabilities and generate data.
    FaultTreeAnalysis::MSample_();
    return;
  }

  // Iterator for minimal cut sets.
  std::set< std::set<int> >::iterator it_min;

  // Iterate minimal cut sets and find probabilities for each set.
  for (it_min = imcs_.begin(); it_min != imcs_.end(); ++it_min) {
    // Calculate a probability of a set with AND relationship.
    double p_sub_set = FaultTreeAnalysis::ProbAnd_(*it_min);
    // Update a container with minimal cut sets and probabilities.
    prob_of_min_sets_.insert(std::make_pair(imcs_to_smcs_[*it_min],
                                            p_sub_set));
    ordered_min_sets_.insert(std::make_pair(p_sub_set,
                                            imcs_to_smcs_[*it_min]));
  }

  // Check if the rare event approximation is requested.
  if (approx_ == "rare") {
    warnings_ += "Using the rare event approximation\n";
    bool rare_event_legit = true;
    std::map< std::set<std::string>, double >::iterator it_pr;
    for (it_pr = prob_of_min_sets_.begin();
         it_pr != prob_of_min_sets_.end(); ++it_pr) {
      // Check if a probability of a set does not exceed 0.1,
      // which is required for the rare event approximation to hold.
      if (rare_event_legit && (it_pr->second > 0.1)) {
        rare_event_legit = false;
        warnings_ += "The rare event approximation may be inaccurate for this"
            "\nfault tree analysis because one of minimal cut sets'"
            "\nprobability exceeded 0.1 threshold requirement.\n\n";
      }
      p_total_ += it_pr->second;
    }

  } else if (approx_ == "mcub") {
    warnings_ += "Using the MCUB approximation\n";
    double m = 1;
    std::map< std::set<std::string>, double >::iterator it;
    for (it = prob_of_min_sets_.begin(); it != prob_of_min_sets_.end();
         ++it) {
      m *= 1 - it->second;
    }
    p_total_ = 1 - m;

  } else {  // No approximation technique is assumed.
    // Exact calculation of probability of cut sets.
    p_total_ = FaultTreeAnalysis::ProbOr_(imcs_, nsums_);
  }

  // Calculate failure contributions of each primary event.
  boost::unordered_map<std::string, PrimaryEventPtr>::iterator it_prime;
  for (it_prime = primary_events_.begin(); it_prime != primary_events_.end();
       ++it_prime) {
    double contrib_pos = 0;  // Total positive contribution of this event.
    double contrib_neg = 0;  // Negative event contribution.
    std::map< std::set<std::string>, double >::iterator it_pr;
    for (it_pr = prob_of_min_sets_.begin();
         it_pr != prob_of_min_sets_.end(); ++it_pr) {
      if (it_pr->first.count(it_prime->first)) {
        contrib_pos += it_pr->second;
      } else if (it_pr->first.count("not " + it_prime->first)) {
        contrib_neg += it_pr->second;
      }
    }
    imp_of_primaries_.insert(std::make_pair(it_prime->first, contrib_pos));
    ordered_primaries_.insert(std::make_pair(contrib_pos, it_prime->first));
    if (contrib_neg > 0) {
      imp_of_primaries_.insert(std::make_pair("not " + it_prime->first,
                                              contrib_neg));
      ordered_primaries_.insert(std::make_pair(contrib_neg,
                                               "not " + it_prime->first));
    }
  }
  // Duration of probability related operations.
  p_time_ = (std::clock() - start_time) / static_cast<double>(CLOCKS_PER_SEC);}

void FaultTreeAnalysis::Report(std::string output) {
  // Check if the analysis has been performed before requesting a report.
  if (!analysis_done_) {
    std::string msg = "Perform analysis before calling this report function.";
    throw scram::Error(msg);
  }
  // Check if output to file is requested.
  std::streambuf* buf;
  std::ofstream of;
  if (output != "cli") {
    of.open(output.c_str());
    buf = of.rdbuf();

  } else {
    buf = std::cout.rdbuf();
  }
  std::ostream out(buf);

  // An iterator for a set with ids of events.
  std::set<std::string>::iterator it_set;

  // Iterator for minimal cut sets.
  std::set< std::set<std::string> >::iterator it_min;

  // Iterator for a map with minimal cut sets and their probabilities.
  std::map< std::set<std::string>, double >::iterator it_pr;

  // Convert MCS into representative strings.
  std::map< std::set<std::string>, std::string> represent;
  std::map< std::set<std::string>, std::vector<std::string> > lines;
  for (it_min = min_cut_sets_.begin(); it_min != min_cut_sets_.end();
       ++it_min) {
    std::stringstream rep;
    rep << "{ ";
    std::string line = "{ ";
    std::vector<std::string> vec_line;
    int j = 1;
    int size = it_min->size();
    for (it_set = it_min->begin(); it_set != it_min->end(); ++it_set) {
      std::vector<std::string> names;
      boost::split(names, *it_set, boost::is_any_of(" "),
                   boost::token_compress_on);
      assert(names.size() < 3);
      assert(names.size() > 0);
      std::string name = "";
      if (names.size() == 1) {
        name = orig_ids_[names[0]];
      } else if (names.size() == 2) {
        name = "NOT " + orig_ids_[names[1]];
      }
      rep << name;

      if (line.length() + name.length() + 2 > 60) {
        vec_line.push_back(line);
        line = name;
      } else {
        line += name;
      }

      if (j < size) {
        rep << ", ";
        line += ", ";
      } else {
        rep << " ";
        line += " ";
      }
      ++j;
    }
    rep << "}";
    line += "}";
    vec_line.push_back(line);
    represent.insert(std::make_pair(*it_min, rep.str()));
    lines.insert(std::make_pair(*it_min, vec_line));
  }

  // Print warnings of calculations.
  if (warnings_ != "") {
    out << "\n" << warnings_ << "\n";
  }

  // Print minimal cut sets by their order.
  out << "\n" << "Minimal Cut Sets" << "\n";
  out << "================\n\n";
  out << std::setw(40) << std::left << "Fault Tree: " << input_file_ << "\n";
  out << std::setw(40) << "Time: " << pt::second_clock::local_time() << "\n\n";
  out << std::setw(40) << "Analysis algorithm: " << analysis_ << "\n";
  out << std::setw(40) << "Limit on order of cut sets: " << limit_order_ << "\n";
  out << std::setw(40) << "Number of Primary Events: " << primary_events_.size() << "\n";
  out << std::setw(40) << "Minimal Cut Set Maximum Order: " << max_order_ << "\n";
  out << std::setw(40) << "Gate Expansion Time: " << std::setprecision(5)
      << exp_time_ << "s\n";
  out << std::setw(40) << "MCS Generation Time: " << std::setprecision(5)
      << mcs_time_ - exp_time_ << "s\n";
  out.flush();

  int order = 1;  // Order of minimal cut sets.
  std::vector<int> order_numbers;  // Number of sets per order.
  while (order < max_order_ + 1) {
    std::set< std::set<std::string> > order_sets;
    for (it_min = min_cut_sets_.begin(); it_min != min_cut_sets_.end();
        ++it_min) {
      if (it_min->size() == order) {
        order_sets.insert(*it_min);
      }
    }
    order_numbers.push_back(order_sets.size());
    if (!order_sets.empty()) {
      out << "\nOrder " << order << ":\n";
      int i = 1;
      for (it_min = order_sets.begin(); it_min != order_sets.end(); ++it_min) {
        std::stringstream number;
        number << i << ") ";
        out << std::left;
        std::vector<std::string>::iterator it;
        int j = 0;
        for (it = lines[*it_min].begin(); it != lines[*it_min].end(); ++it) {
          if (j == 0) {
            out << number.str() <<  *it << "\n";
          } else {
            out << "  " << std::setw(number.str().length()) << " "
                << *it << "\n";
          }
          ++j;
        }
        out.flush();
        i++;
      }
    }
    order++;
  }

  out << "\nQualitative Importance Analysis:" << "\n";
  out << "--------------------------------\n";
  out << std::left;
  out << std::setw(20) << "Order" << "Number\n";
  out << std::setw(20) << "-----" << "------\n";
  for (int i = 1; i < max_order_ + 1; ++i) {
    out << "  " << std::setw(18) << i << order_numbers[i-1] << "\n";
  }
  out << "  " << std::setw(18) << "ALL" << min_cut_sets_.size() << "\n";
  out.flush();

  // Print probabilities of minimal cut sets only if requested.
  if (!prob_requested_) return;

  out << "\n" << "Probability Analysis" << "\n";
  out << "====================\n\n";
  out << std::setw(40) << std::left << "Fault Tree: " << input_file_ << "\n";
  out << std::setw(40) << "Time: " << pt::second_clock::local_time() << "\n\n";
  out << std::setw(40) << "Analysis type:" << analysis_ << "\n";
  out << std::setw(40) << "Limit on series: " << nsums_ << "\n";
  out << std::setw(40) << "Number of Primary Events: "
      << primary_events_.size() << "\n";
  out << std::setw(40) << "Number of Minimal Cut Sets: "
      << min_cut_sets_.size() << "\n";
  out << std::setw(40) << "Probability Operations Time: " << std::setprecision(5)
      << p_time_ - mcs_time_ << "s\n\n";
  out.flush();

  if (analysis_ == "default") {
    out << "Minimal Cut Set Probabilities Sorted by Order:\n";
    out << "----------------------------------------------\n";
    out.flush();
    order = 1;  // Order of minimal cut sets.
    std::multimap < double, std::set<std::string> >::reverse_iterator it_or;
    while (order < max_order_ + 1) {
      std::multimap< double, std::set<std::string> > order_sets;
      for (it_min = min_cut_sets_.begin(); it_min != min_cut_sets_.end();
           ++it_min) {
        if (it_min->size() == order) {
          order_sets.insert(std::make_pair(prob_of_min_sets_[*it_min],
                                           *it_min));
        }
      }
      if (!order_sets.empty()) {
        out << "\nOrder " << order << ":\n";
        int i = 1;
        for (it_or = order_sets.rbegin(); it_or != order_sets.rend(); ++it_or) {
          std::stringstream number;
          number << i << ") ";
          out << std::left;
          std::vector<std::string>::iterator it;
          int j = 0;
          for (it = lines[it_or->second].begin();
               it != lines[it_or->second].end(); ++it) {
            if (j == 0) {
              out << number.str() << std::setw(70 - number.str().length())
                  << *it << std::setprecision(7) << it_or->first << "\n";
            } else {
              out << "  " << std::setw(number.str().length()) << " "
                  << *it << "\n";
            }
            ++j;
          }
          out.flush();
          i++;
        }
      }
      order++;
    }

    out << "\nMinimal Cut Set Probabilities Sorted by Probability:\n";
    out << "----------------------------------------------------\n";
    out.flush();
    int i = 1;
    for (it_or = ordered_min_sets_.rbegin(); it_or != ordered_min_sets_.rend();
         ++it_or) {
      std::stringstream number;
      number << i << ") ";
      out << std::left;
      std::vector<std::string>::iterator it;
      int j = 0;
      for (it = lines[it_or->second].begin();
           it != lines[it_or->second].end(); ++it) {
        if (j == 0) {
          out << number.str() << std::setw(70 - number.str().length())
              << *it << std::setprecision(7) << it_or->first << "\n";
        } else {
          out << "  " << std::setw(number.str().length()) << " "
              << *it << "\n";
        }
        ++j;
      }
      i++;
      out.flush();
    }

    // Print total probability.
    out << "\n" << "================================\n";
    out <<  "Total Probability: " << std::setprecision(7) << p_total_ << "\n";
    out << "================================\n\n";

    if (p_total_ > 1) out << "WARNING: Total Probability is invalid.\n\n";

    out.flush();

    // Primary event analysis.
    out << "Primary Event Analysis:\n";
    out << "-----------------------\n";
    out << std::left;
    out << std::setw(20) << "Event" << std::setw(20) << "Failure Contrib."
        << "Importance\n\n";
    std::multimap < double, std::string >::reverse_iterator it_contr;
    for (it_contr = ordered_primaries_.rbegin();
         it_contr != ordered_primaries_.rend(); ++it_contr) {
      out << std::left;
      std::vector<std::string> names;
      boost::split(names, it_contr->second, boost::is_any_of(" "),
                   boost::token_compress_on);
      assert(names.size() < 3);
      assert(names.size() > 0);
      if (names.size() == 1) {
        out << std::setw(20) << orig_ids_[names[0]] << std::setw(20)
            << it_contr->first << 100 * it_contr->first / p_total_ << "%\n";

      } else if (names.size() == 2) {
        out << "NOT " << std::setw(16) << orig_ids_[names[1]] << std::setw(20)
            << it_contr->first << 100 * it_contr->first / p_total_ << "%\n";
      }
      out.flush();
    }

  } else if (analysis_ == "mc") {
    // Report for Monte Carlo Uncertainty Analysis.
    // Show the terms of the equation.
    // Positive terms.
    out << "\nPositive Terms in the Probability Equation:\n";
    out << "--------------------------------------------\n";
    std::vector< std::set<int> >::iterator it_vec;
    std::set<int>::iterator it_set;
    for (it_vec = pos_terms_.begin(); it_vec != pos_terms_.end(); ++it_vec) {
      out << "{ ";
      int j = 1;
      int size = it_vec->size();
      for (it_set = it_vec->begin(); it_set != it_vec->end(); ++it_set) {
        if (*it_set > 0) {
          out << orig_ids_[int_to_prime_[*it_set]->id()];
        } else {
          out << "NOT " << orig_ids_[int_to_prime_[std::abs(*it_set)]->id()];
        }
        if (j < size) {
          out << ", ";
        } else {
          out << " ";
        }
        ++j;
      }
      out << "}\n";
      out.flush();
    }
    // Negative terms.
    out << "\nNegative Terms in the Probability Equation:\n";
    out << "-------------------------------------------\n";
    for (it_vec = neg_terms_.begin(); it_vec != neg_terms_.end(); ++it_vec) {
      out << "{ ";
      int j = 1;
      int size = it_vec->size();
      for (it_set = it_vec->begin(); it_set != it_vec->end(); ++it_set) {
        if (*it_set > 0) {
          out << orig_ids_[int_to_prime_[*it_set]->id()];
        } else {
          out << "NOT " << orig_ids_[int_to_prime_[std::abs(*it_set)]->id()];
        }
        if (j < size) {
          out << ", ";
        } else {
          out << " ";
        }
        ++j;
      }
      out << "}\n";
      out.flush();
    }
  }
}

void FaultTreeAnalysis::GraphNode_(GatePtr t, std::map<std::string,
                                   int>& pr_repeat, std::ofstream& out) {
  // Populate intermediate and primary events of the input inter event.
  std::map<std::string, EventPtr> events_children = t->children();
  std::map<std::string, EventPtr>::iterator it_child;
  for (it_child = events_children.begin(); it_child != events_children.end();
       ++it_child) {
    // Deal with repeated primary events.
    if (primary_events_.count(it_child->first)) {
      if (pr_repeat.count(it_child->first)) {
        int rep = pr_repeat[it_child->first];
        rep++;
        pr_repeat.erase(it_child->first);
        pr_repeat.insert(std::make_pair(it_child->first, rep));
      } else if (!inter_events_.count(it_child->first)) {
        pr_repeat.insert(std::make_pair(it_child->first, 0));
      }
      out << "\"" <<  orig_ids_[t->id()] << "\" -> "
          << "\"" <<orig_ids_[it_child->first] <<"_R"
          << pr_repeat[it_child->first] << "\";\n";
    } else {
      out << "\"" << orig_ids_[t->id()] << "\" -> "
          << "\"" << orig_ids_[it_child->first] << "\";\n";
    }
  }
}

void FaultTreeAnalysis::ExpandSets_(int inter_index,
                                    std::vector< SupersetPtr >& sets) {
  // Populate intermediate and primary events of the top.
  const std::map<std::string, EventPtr>* children =
      &int_to_inter_[std::abs(inter_index)]->children();

  std::string gate = int_to_inter_[std::abs(inter_index)]->type();

  // Iterator for children of top and intermediate events.
  std::map<std::string, EventPtr>::const_iterator it_children;
  std::vector<int> events_children;
  std::vector<int>::iterator it_child;

  for (it_children = children->begin();
       it_children != children->end(); ++it_children) {
    if (inter_events_.count(it_children->first)) {
      events_children.push_back(inter_to_int_[it_children->first]);
    } else {
      events_children.push_back(prime_to_int_[it_children->first]);
    }
  }

  // Type dependent logic.
  if (gate == "or") {
    assert(events_children.size() > 1);
    if (inter_index > 0) {
      FaultTreeAnalysis::SetOr_(events_children, sets);
    } else {
      FaultTreeAnalysis::SetAnd_(events_children, sets, -1);
    }
  } else if (gate == "and") {
    assert(events_children.size() > 1);
    if (inter_index > 0) {
      FaultTreeAnalysis::SetAnd_(events_children, sets);
    } else {
      FaultTreeAnalysis::SetOr_(events_children, sets, -1);
    }
  } else if (gate == "not") {
    int mult = 1;
    if (inter_index < 0) mult = -1;
    // Only one child is expected.
    assert(events_children.size() == 1);
    FaultTreeAnalysis::SetAnd_(events_children, sets, -1 * mult);
  } else if (gate == "nor") {
    assert(events_children.size() > 1);
    if (inter_index > 0) {
      FaultTreeAnalysis::SetAnd_(events_children, sets, -1);
    } else {
      FaultTreeAnalysis::SetOr_(events_children, sets);
    }
  } else if (gate == "nand") {
    assert(events_children.size() > 1);
    if (inter_index > 0) {
      FaultTreeAnalysis::SetOr_(events_children, sets, -1);
    } else {
      FaultTreeAnalysis::SetAnd_(events_children, sets);
    }
  } else if (gate == "xor") {
    assert(events_children.size() == 2);
    SupersetPtr tmp_set_one(new scram::Superset());
    SupersetPtr tmp_set_two(new scram::Superset());
    if (inter_index > 0) {
      int j = 1;
      for (it_child = events_children.begin();
           it_child != events_children.end(); ++it_child) {
        if (*it_child > top_event_index_) {
          tmp_set_one->InsertGate(j * (*it_child));
          tmp_set_two->InsertGate(-1 * j * (*it_child));
        } else {
          tmp_set_one->InsertPrimary(j * (*it_child));
          tmp_set_two->InsertPrimary(-1 * j * (*it_child));
        }
        j = -1;
      }
    } else {
      for (it_child = events_children.begin();
           it_child != events_children.end(); ++it_child) {
        if (*it_child > top_event_index_) {
          tmp_set_one->InsertGate(*it_child);
          tmp_set_two->InsertGate(-1 * (*it_child));
        } else {
          tmp_set_one->InsertPrimary(*it_child);
          tmp_set_two->InsertPrimary(-1 * (*it_child));
        }
      }
    }
    sets.push_back(tmp_set_one);
    sets.push_back(tmp_set_two);
  } else if (gate == "null") {
    int mult = 1;
    if (inter_index < 0) mult = -1;
    // Only one child is expected.
    assert(events_children.size() == 1);
    FaultTreeAnalysis::SetAnd_(events_children, sets, mult);
  } else if (gate == "inhibit") {
    assert(events_children.size() == 2);
    if (inter_index > 0) {
      FaultTreeAnalysis::SetAnd_(events_children, sets);
    } else {
      FaultTreeAnalysis::SetOr_(events_children, sets, -1);
    }
  } else if (gate == "vote") {
    int vote_number = int_to_inter_[std::abs(inter_index)]->vote_number();
    assert(vote_number > 1);
    assert(events_children.size() >= vote_number);
    std::set< std::set<int> > all_sets;
    int size = events_children.size();

    for (int j = 0; j < size; ++j) {
      std::set<int> set;
      set.insert(events_children[j]);
      all_sets.insert(set);
    }

    int mult = 1;
    if (inter_index < 0) {
      mult = -1;
      vote_number = size - vote_number + 1;  // The main trick for negation.
    }

    for (int i = 1; i < vote_number; ++i) {
      std::set< std::set<int> > tmp_sets;
      std::set< std::set<int> >::iterator it_sets;
      for (it_sets = all_sets.begin(); it_sets != all_sets.end(); ++it_sets) {
        for (int j = 0; j < size; ++j) {
          std::set<int> set = *it_sets;
          set.insert(events_children[j]);
          if (set.size() > i) {
            tmp_sets.insert(set);
          }
        }
      }
      all_sets = tmp_sets;
    }

    std::set< std::set<int> >::iterator it_sets;
    for (it_sets = all_sets.begin(); it_sets != all_sets.end(); ++it_sets) {
      SupersetPtr tmp_set_c(new scram::Superset());
      std::set<int>::iterator it;
      for (it = it_sets->begin(); it != it_sets->end(); ++it) {
        if (*it > top_event_index_) {
          tmp_set_c->InsertGate(*it * mult);
        } else {
          tmp_set_c->InsertPrimary(*it * mult);
        }
      }
      sets.push_back(tmp_set_c);
    }

  } else {
    boost::to_upper(gate);
    std::string msg = "No algorithm defined for " + gate;
    throw scram::ValueError(msg);
  }
}

void FaultTreeAnalysis::SetOr_(std::vector<int>& events_children,
                       std::vector<SupersetPtr>& sets, int mult) {
  std::vector<int>::iterator it_child;
  for (it_child = events_children.begin();
       it_child != events_children.end(); ++it_child) {
    SupersetPtr tmp_set_c(new scram::Superset());
    if (*it_child > top_event_index_) {
      tmp_set_c->InsertGate(*it_child * mult);
    } else {
      tmp_set_c->InsertPrimary(*it_child * mult);
    }
    sets.push_back(tmp_set_c);
  }
}

void FaultTreeAnalysis::SetAnd_(std::vector<int>& events_children,
                        std::vector<SupersetPtr>& sets, int mult) {
  SupersetPtr tmp_set_c(new scram::Superset());
  std::vector<int>::iterator it_child;
  for (it_child = events_children.begin();
       it_child != events_children.end(); ++it_child) {
    if (*it_child > top_event_index_) {
      tmp_set_c->InsertGate(*it_child * mult);
    } else {
      tmp_set_c->InsertPrimary(*it_child * mult);
    }
  }
  sets.push_back(tmp_set_c);
}

void FaultTreeAnalysis::FindMCS_(const std::set< std::set<int> >& cut_sets,
                         const std::set< std::set<int> >& mcs_lower_order,
                         int min_order) {
  if (cut_sets.empty()) return;

  // Iterator for cut_sets.
  std::set< std::set<int> >::iterator it_uniq;

  // Iterator for minimal cut sets.
  std::set< std::set<int> >::iterator it_min;

  std::set< std::set<int> > temp_sets;  // For mcs of a level above.
  std::set< std::set<int> > temp_min_sets;  // For mcs of this level.

  for (it_uniq = cut_sets.begin();
       it_uniq != cut_sets.end(); ++it_uniq) {
    bool include = true;  // Determine to keep or not.

    for (it_min = mcs_lower_order.begin(); it_min != mcs_lower_order.end();
         ++it_min) {
      if (std::includes(it_uniq->begin(), it_uniq->end(),
                        it_min->begin(), it_min->end())) {
        // Non-minimal cut set is detected.
        include = false;
        break;
      }
    }
    // After checking for non-minimal cut sets,
    // all minimum sized cut sets are guaranteed to be minimal.
    if (include) {
      if (it_uniq->size() == min_order) {
        temp_min_sets.insert(*it_uniq);
        // Update maximum order of the sets.
        if (min_order > max_order_) max_order_ = min_order;
      } else {
        temp_sets.insert(*it_uniq);
      }
    }
    // Ignore the cut set because include = false.
  }
  imcs_.insert(temp_min_sets.begin(), temp_min_sets.end());
  min_order++;
  FaultTreeAnalysis::FindMCS_(temp_sets, temp_min_sets, min_order);
}

// -------------------- Algorithm for Cut Sets and Probabilities -----------
void FaultTreeAnalysis::AssignIndices_(const FaultTreePtr& fault_tree) {
  // Assign an index to each primary event, and populate relevant
  // databases.

  // Getting events from the fault tree object.
  top_event_ = fault_tree->top_event();
  inter_events_ = fault_tree->inter_events();
  primary_events_ = fault_tree->primary_events();

  int j = 1;
  boost::unordered_map<std::string, PrimaryEventPtr>::iterator itp;
  // Dummy primary event at index 0.
  int_to_prime_.push_back(PrimaryEventPtr(new PrimaryEvent("dummy")));
  iprobs_.push_back(0);
  for (itp = primary_events_.begin(); itp != primary_events_.end(); ++itp) {
    int_to_prime_.push_back(itp->second);
    prime_to_int_.insert(std::make_pair(itp->second->id(), j));
    if (prob_requested_) iprobs_.push_back(itp->second->p());
    ++j;
  }

  // Assign an index to each top and intermediate event and populate
  // relevant databases.
  top_event_index_ = j;
  int_to_inter_.insert(std::make_pair(j, top_event_));
  inter_to_int_.insert(std::make_pair(top_event_id_, j));
  ++j;
  boost::unordered_map<std::string, GatePtr>::iterator iti;
  for (iti = inter_events_.begin(); iti != inter_events_.end(); ++iti) {
    int_to_inter_.insert(std::make_pair(j, iti->second));
    inter_to_int_.insert(std::make_pair(iti->second->id(), j));
    ++j;
  }
}

void FaultTreeAnalysis::SetsToString_() {
  std::set< std::set<int> >::iterator it_min;
  for (it_min = imcs_.begin(); it_min != imcs_.end(); ++it_min) {
    std::set<std::string> pr_set;
    std::set<int>::iterator it_set;
    for (it_set = it_min->begin(); it_set != it_min->end(); ++it_set) {
      if (*it_set < 0) {  // NOT logic.
        pr_set.insert("not " + int_to_prime_[std::abs(*it_set)]->id());
      } else {
        pr_set.insert(int_to_prime_[*it_set]->id());
      }
    }
    imcs_to_smcs_.insert(std::make_pair(*it_min, pr_set));
    min_cut_sets_.insert(pr_set);
  }
}

double FaultTreeAnalysis::ProbOr_(std::set< std::set<int> >& min_cut_sets, int nsums) {
  // Recursive implementation.
  if (min_cut_sets.empty()) return 0;

  if (nsums == 0) return 0;

  // Base case.
  if (min_cut_sets.size() == 1) {
    // Get only element in this set.
    return FaultTreeAnalysis::ProbAnd_(*min_cut_sets.begin());
  }

  // Get one element.
  std::set< std::set<int> >::iterator it = min_cut_sets.begin();
  std::set<int> element_one = *it;

  // Delete element from the original set. WARNING: the iterator is invalidated.
  min_cut_sets.erase(it);
  std::set< std::set<int> > combo_sets;
  FaultTreeAnalysis::CombineElAndSet_(element_one, min_cut_sets, combo_sets);

  return FaultTreeAnalysis::ProbAnd_(element_one) +
         FaultTreeAnalysis::ProbOr_(min_cut_sets, nsums) -
         FaultTreeAnalysis::ProbOr_(combo_sets, nsums - 1);
}

double FaultTreeAnalysis::ProbAnd_(const std::set<int>& min_cut_set) {
  // Test just in case the min cut set is empty.
  if (min_cut_set.empty()) return 0;

  double p_sub_set = 1;  // 1 is for multiplication.
  std::set<int>::iterator it_set;
  for (it_set = min_cut_set.begin(); it_set != min_cut_set.end(); ++it_set) {
    if (*it_set > 0) {
      p_sub_set *= iprobs_[*it_set];
    } else {
      p_sub_set *= 1 - iprobs_[std::abs(*it_set)];
    }
  }
  return p_sub_set;
}

void FaultTreeAnalysis::CombineElAndSet_(const std::set<int>& el,
                                 const std::set< std::set<int> >& set,
                                 std::set< std::set<int> >& combo_set) {
  std::set<int> member_set;
  std::set<int>::iterator it;
  std::set< std::set<int> >::iterator it_set;
  for (it_set = set.begin(); it_set != set.end(); ++it_set) {
    member_set = *it_set;
    bool include = true;
    for (it = el.begin(); it != el.end(); ++it) {
      if (it_set->count(-1 * (*it))) {
        include = false;
        break;
      }
      member_set.insert(*it);
    }
    if (include) combo_set.insert(member_set);
  }
}

// ----------------------------------------------------------------------
// ----- Algorithm for Total Equation for Monte Carlo Simulation --------
// Generation of the representation of the original equation.
void FaultTreeAnalysis::MProbOr_(std::set< std::set<int> >& min_cut_sets,
                         int sign, int nsums) {
  // Recursive implementation.
  if (min_cut_sets.empty()) return;

  if (nsums == 0) return;

  // Get one element.
  std::set< std::set<int> >::iterator it = min_cut_sets.begin();
  std::set<int> element_one = *it;

  // Delete element from the original set. WARNING: the iterator is invalidated.
  min_cut_sets.erase(it);

  // Put this element into the equation.
  if ((sign % 2) == 1) {
    // This is a positive member.
    pos_terms_.push_back(element_one);
  } else {
    // This must be a negative member.
    neg_terms_.push_back(element_one);
  }

  std::set< std::set<int> > combo_sets;
  FaultTreeAnalysis::CombineElAndSet_(element_one, min_cut_sets, combo_sets);
  FaultTreeAnalysis::MProbOr_(min_cut_sets, sign, nsums);
  FaultTreeAnalysis::MProbOr_(combo_sets, sign + 1, nsums - 1);
  return;
}

void FaultTreeAnalysis::MSample_() {}
// ----------------------------------------------------------------------

}  // namespace scram
