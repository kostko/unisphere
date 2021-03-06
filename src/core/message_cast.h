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
#ifndef UNISPHERE_CORE_MESSAGECAST_H
#define UNISPHERE_CORE_MESSAGECAST_H

#include "core/exception.h"

namespace UniSphere {

/**
 * An exception that gets raised when a message cast fails.
 */
class UNISPHERE_EXPORT MessageCastFailed : public Exception {
};

/**
 * Default message cast handler that always throws an exception.
 */
template <typename T, typename MessageType>
T message_cast(const MessageType &msg)
{
  throw MessageCastFailed();
}

}

#endif
