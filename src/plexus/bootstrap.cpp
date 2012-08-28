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
#include "plexus/bootstrap.h"

namespace UniSphere {
  
Bootstrap::Bootstrap()
{
}

SingleHostBootstrap::SingleHostBootstrap (const Contact &contact)
  : m_contact(contact)
{
}

Contact SingleHostBootstrap::getBootstrapContact()
{
  return m_contact;
}

DelayedBootstrap::DelayedBootstrap()
  : m_lastContact(m_contacts.end())
{
}

Contact DelayedBootstrap::getBootstrapContact()
{
  if (m_lastContact == m_contacts.end())
    m_lastContact = m_contacts.begin();
  
  if (m_lastContact == m_contacts.end())
    return Contact();
  
  Contact contact = *m_lastContact;
  m_lastContact++;
  return contact;
}

void DelayedBootstrap::addContact (const Contact &contact)
{
  m_contacts.push_back(contact);
  
  // If there were no contacts before, signal that there are now
  if (m_contacts.size() == 1)
    signalContactReady();
}

}
