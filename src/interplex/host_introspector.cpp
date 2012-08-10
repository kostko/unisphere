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
#include "interplex/host_introspector.h"

#include <sys/types.h>
#include <ifaddrs.h>

namespace UniSphere {

HostIntrospector::HostIntrospector()
{
}

Contact HostIntrospector::localContact(unsigned short port)
{
  Contact contact;
  
  // Fetch address information from the local interfaces
  struct ifaddrs *addresses = NULL;
  getifaddrs(&addresses);
  for (auto ifa = addresses; ifa != NULL; ifa = ifa->ifa_next) {
    int family = ifa->ifa_addr->sa_family;
    
    // We are only interested in IPv4 and IPv6 addresses
    if (family == AF_INET || family == AF_INET6) {
      char host[NI_MAXHOST];
      
      if (getnameinfo(ifa->ifa_addr, (family == AF_INET) ?
        sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6),
        host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST) == 0) {
        // Convert IP address to Boost.ASIO address and add to contact
        contact.addAddress(Address(
          boost::asio::ip::address::from_string(host),
          port
        ));
      }
    }
  }
  freeifaddrs(addresses);
  
  return contact;
}

}
