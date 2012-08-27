/*
 * This file is part of UNISPHERE.
 *
 * Copyright (C) 2012 Jernej Kos <k@jst.sm>
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
#ifndef UNISPHERE_CORE_HASHTUPLE_H
#define UNISPHERE_CORE_HASHTUPLE_H

#include <tuple>

namespace std {
  namespace {
    // Code from boost
    // Reciprocal of the golden ratio helps spread entropy
    //     and handles duplicates.
    // See Mike Seymour in magic-numbers-in-boosthash-combine:
    //     http://stackoverflow.com/questions/4948780
    template <class T>
    inline void hash_combine(std::size_t &seed, const T &v)
    {
      seed ^= hash<T>()(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
    }
    
    // Recursive template code derived from Matthieu M.
    template <class Tuple, size_t Index = std::tuple_size<Tuple>::value - 1>
    struct HashValueImpl
    {
      static void apply(size_t &seed, const Tuple &tuple)
      {
        HashValueImpl<Tuple, Index - 1>::apply(seed, tuple);
        hash_combine(seed, get<Index>(tuple));
      }
    };
    
    template <class Tuple>
    struct HashValueImpl<Tuple, 0>
    {
      static void apply(size_t &seed, const Tuple &tuple)
      {
        std::hash_combine(seed, get<0>(tuple));
      }
    };
  }
  
  template <typename ... TT>
  struct hash<std::tuple<TT...>> 
  {
    size_t operator()(const std::tuple<TT...> &tt) const
    {
      size_t seed = 0;
      HashValueImpl<std::tuple<TT...> >::apply(seed, tt);
      return seed;
    }
  };
}

#endif
