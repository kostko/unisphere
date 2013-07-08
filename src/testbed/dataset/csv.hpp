/*
 * This file is part of UNISPHERE.
 *
 * Copyright (C) 2013 Jernej Kos <k@jst.sm>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef UNISPHERE_TESTBED_DATASETCSV_H
#define UNISPHERE_TESTBED_DATASETCSV_H

#include <fstream>

namespace UniSphere {

namespace TestBed {

namespace detail {

class OutputCsvVisitor : public boost::static_visitor<> {
public:
  OutputCsvVisitor(std::ofstream &file)
    : file(file)
  {}

  void operator()(const std::string &value) const
  {
    file << '"' << value << '"';
  }

  template <typename T>
  void operator()(const T &value) const
  {
    file << value;
  }
private:
  std::ofstream &file;
};

}

template <class DataSet>
void outputCsvDataset(const DataSet &dataset,
                      std::initializer_list<std::string> fields,
                      const std::string &outputFilename)
{
  std::ofstream file;
  file.open(outputFilename);

  // Output column list
  for (const std::string &field : fields) {
    file << field;
    file << "\t";
  }
  file << "\n";

  // Output data
  for (const auto &record : dataset) {
    for (const std::string &field : fields) {
      boost::apply_visitor(detail::OutputCsvVisitor(file), record.at(field));
      file << "\t";
    }
    file << "\n";
  }
}

}

}

#endif
