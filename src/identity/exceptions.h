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
#ifndef UNISPHERE_IDENTITY_EXCEPTIONS_H
#define UNISPHERE_IDENTITY_EXCEPTIONS_H

#include "core/exception.h"

namespace UniSphere {

/**
 * Peer key is null exception.
 */
struct UNISPHERE_EXPORT NullPeerKey : public Exception {
  NullPeerKey(const std::string &msg = "") : Exception(msg) {}
};

/**
 * Exception raised when the cryptographic signature is invalid.
 */
struct UNISPHERE_EXPORT InvalidSignature : public Exception {
  InvalidSignature(const std::string &msg = "") : Exception(msg) {}
};

}

#endif
