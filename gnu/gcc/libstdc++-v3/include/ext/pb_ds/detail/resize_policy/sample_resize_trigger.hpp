// -*- C++ -*-

// Copyright (C) 2005, 2006 Free Software Foundation, Inc.
//
// This file is part of the GNU ISO C++ Library.  This library is free
// software; you can redistribute it and/or modify it under the terms
// of the GNU General Public License as published by the Free Software
// Foundation; either version 2, or (at your option) any later
// version.

// This library is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this library; see the file COPYING.  If not, write to
// the Free Software Foundation, 59 Temple Place - Suite 330, Boston,
// MA 02111-1307, USA.

// As a special exception, you may use this file as part of a free
// software library without restriction.  Specifically, if other files
// instantiate templates or use macros or inline functions from this
// file, or you compile this file and link it with other files to
// produce an executable, this file does not by itself cause the
// resulting executable to be covered by the GNU General Public
// License.  This exception does not however invalidate any other
// reasons why the executable file might be covered by the GNU General
// Public License.

// Copyright (C) 2004 Ami Tavory and Vladimir Dreizin, IBM-HRL.

// Permission to use, copy, modify, sell, and distribute this software
// is hereby granted without fee, provided that the above copyright
// notice appears in all copies, and that both that copyright notice
// and this permission notice appear in supporting documentation. None
// of the above authors, nor IBM Haifa Research Laboratories, make any
// representation about the suitability of this software for any
// purpose. It is provided "as is" without express or implied
// warranty.

/**
 * @file sample_resize_trigger.hpp
 * Contains a sample resize trigger policy class.
 */

#ifndef PB_DS_SAMPLE_RESIZE_TRIGGER_HPP
#define PB_DS_SAMPLE_RESIZE_TRIGGER_HPP

// A sample resize trigger policy.
class sample_resize_trigger
{
public:

  // Size type.
  typedef size_t size_type;

  // Default constructor.
  sample_resize_trigger();

  // Copy constructor.
  sample_range_hashing(const sample_resize_trigger& other);

  // Swaps content.
  inline void
  swap(sample_resize_trigger& other);

protected:

  // Notifies a search started.
  inline void
  notify_insert_search_start();

  // Notifies a search encountered a collision.
  inline void
  notify_insert_search_collision();

  // Notifies a search ended.
  inline void
  notify_insert_search_end();

  // Notifies a search started.
  inline void
  notify_find_search_start();

  // Notifies a search encountered a collision.
  inline void
  notify_find_search_collision();

  // Notifies a search ended.
  inline void
  notify_find_search_end();

  // Notifies a search started.
  inline void
  notify_erase_search_start();

  // Notifies a search encountered a collision.
  inline void
  notify_erase_search_collision();

  // Notifies a search ended.
  inline void
  notify_erase_search_end();

  // Notifies an element was inserted. the total number of entries in
  // the table is num_entries.
  inline void
  notify_inserted(size_type num_entries);

  // Notifies an element was erased.
  inline void
  notify_erased(size_type num_entries);

  // Notifies the table was cleared.
  void
  notify_cleared();

  // Notifies the table was resized as a result of this object's
  // signifying that a resize is needed.
  void
  notify_resized(size_type new_size);

  // Notifies the table was resized externally.
  void
  notify_externally_resized(size_type new_size);

  // Queries whether a resize is needed.
  inline bool
  is_resize_needed() const;

  // Queries whether a grow is needed.
  inline bool
  is_grow_needed(size_type size, size_type num_entries) const;

private:

  // Resizes to new_size.
  virtual void
  do_resize(size_type new_size);
};

} // namespace pb_ds

#endif 
