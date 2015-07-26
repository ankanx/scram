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
#ifndef SCRAM_TESTS_PREPROCESSOR_TESTS_H_
#define SCRAM_TESTS_PREPROCESSOR_TESTS_H_

#include "preprocessor.h"

#include <gtest/gtest.h>

using namespace scram;

/// @class PreprocessorTest
/// This test fixture is for white-box testing of algorithms for fault tree
/// preprossing.
class PreprocessorTest : public ::testing::Test {
 protected:
  typedef boost::shared_ptr<IGate> IGatePtr;

  virtual void SetUp() {
  }

  virtual void TearDown() {
  }

  bool ProcessConstantChild(const IGatePtr& gate, int child,
                            bool state, std::vector<int>* to_erase) {
    return false;
  }

  // Members for tests.
  Preprocessor* prep;
  IndexedFaultTree* fault_tree;
};

#endif // SCRAM_TESTS_PREPROCESSOR_TESTS_H_
