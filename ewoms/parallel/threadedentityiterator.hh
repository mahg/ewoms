/*
  Copyright (C) 2014 by Andreas Lauser

  This file is part of the Open Porous Media project (OPM).

  OPM is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  OPM is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with OPM.  If not, see <http://www.gnu.org/licenses/>.
*/
/*!
 * \file
 * \copydoc Ewoms::ThreadedEntityIterator
 */
#ifndef EWOMS_THREADED_ENTITY_ITERATOR_HH
#define EWOMS_THREADED_ENTITY_ITERATOR_HH

#if HAVE_OPENMP
#include <omp.h>
#endif

#include <ewoms/parallel/locks.hh>

namespace Ewoms {

/*!
 * \brief Provides an STL-iterator like interface to iterate over the enties of a
 *        GridView in OpenMP threaded applications
 *
 * ATTENTION: This class must be instantiated in a sequential context!
 */
template <class GridView, int codim>
class ThreadedEntityIterator
{
    typedef typename GridView::template Codim<codim>::Entity Entity;
    typedef typename GridView::template Codim<codim>::Iterator EntityIterator;
public:
    ThreadedEntityIterator(const GridView& gridView)
        : gridView_(gridView)
        , sequentialIt_(gridView.template begin<codim>())
        , sequentialEnd_(gridView.template end<codim>())
    { }

    ThreadedEntityIterator(const ThreadedEntityIterator &other) = default;

    // begin iterating over the grid in parallel
    void beginParallel(EntityIterator& threadPrivateIt)
    {
        mutex_.lock();
        threadPrivateIt = sequentialIt_;
        if (sequentialIt_ != sequentialEnd_)
            ++sequentialIt_;
        mutex_.unlock();
    }

    // returns true if the last element was reached
    bool isFinished(const EntityIterator& threadPrivateIt) const
    { return threadPrivateIt == sequentialEnd_; }

    // prefix increment: goes to the next element which is not yet worked on by any
    // thread
    void increment(EntityIterator& threadPrivateIt)
    {
        mutex_.lock();
        threadPrivateIt = sequentialIt_;
        if (sequentialIt_ != sequentialEnd_)
            ++sequentialIt_;
        mutex_.unlock();
    }

private:
    GridView gridView_;
    EntityIterator sequentialIt_;
    EntityIterator sequentialEnd_;

    OmpMutex mutex_;
};
} // namespace Ewoms

#endif