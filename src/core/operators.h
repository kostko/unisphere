/*
 * This file is part of UNISPHERE.
 *
 * Copyright (C) 2012 Jernej Kos <jernej@kos.mx>
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
#ifndef UNISPHERE_CORE_OPERATORS_H
#define UNISPHERE_CORE_OPERATORS_H

namespace UniSphere {

/**
 * Operator for syntactically nicer construction of lists. The U template
 * parameter is used to support the use of other types that can be implicitly
 * converted to the type stored in list.
 */
template <typename T, typename U>
std::list<T> &operator<<(std::list<T> &list, U item)
{
  list.push_back(item);
  return list;
}

}

#endif
