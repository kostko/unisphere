/*
 * This file is part of UNISPHERE.
 *
 * Copyright (C) 2014 Jernej Kos <jernej@kos.mx>
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
#ifndef UNISPHERE_CORE_SERIALIZETUPLE_H
#define UNISPHERE_CORE_SERIALIZETUPLE_H

namespace boost {

namespace serialization {

namespace detail {

template<uint N>
struct SerializeTuple
{
  template<class Archive, typename... Args>
  static void serialize(Archive &ar, std::tuple<Args...> &tuple, const unsigned int version)
  {
    ar & std::get<N-1>(tuple);
    detail::SerializeTuple<N-1>::serialize(ar, tuple, version);
  }
};

template<>
struct SerializeTuple<0>
{
  template<class Archive, typename... Args>
  static void serialize(Archive &ar, std::tuple<Args...> &tuple, const unsigned int version)
  {
  }
};

}

template<class Archive, typename... Args>
void serialize(Archive &ar, std::tuple<Args...> &tuple, const unsigned int version)
{
  detail::SerializeTuple<sizeof...(Args)>::serialize(ar, tuple, version);
}

}

}

#endif
