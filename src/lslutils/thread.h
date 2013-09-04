#ifndef LIBUNITSYNCPP_THREAD_H
#define LIBUNITSYNCPP_THREAD_H

#include <boost/thread/thread.hpp>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/noncopyable.hpp>
#include <vector>

namespace LSL {

class WorkItemQueue;


/** @brief Abstraction of a piece of work to be done by WorkerThread
    Inherit this class to define concrete work items. */
class WorkItem : public boost::noncopyable
{
  public:

    /** @brief Construct a new WorkItem */
    WorkItem() : m_priority(0), m_toBeDeleted(true), m_queue(NULL) {}

    /** @brief Destructor */
    virtual ~WorkItem() {}

    /** @brief Implement this in derived class to do the work */
    virtual void Run() = 0;

    /** @brief Cancel this WorkItem, remove it from queue
        @return true if it was removed, false otherwise */
    bool Cancel();

    int GetPriority() const { return m_priority; }

  private:
    int m_priority;              ///< Priority of item, highest is run first
    volatile bool m_toBeDeleted; ///< Should this item be deleted after it has run?
    WorkItemQueue* m_queue;

    friend class WorkItemQueue;
    friend class WorkerThread;
};


/** @brief Priority queue of work items
 *	this is processed via a boost thread from \ref WorkerThread::operator ()
 * */
class WorkItemQueue : public boost::noncopyable
{
  public:
    WorkItemQueue();
    ~WorkItemQueue();
    /** @brief thread entry point */
    void Process();
    /** @brief Push more work onto the queue */
    void Push(WorkItem* item);
    /** @brief Remove a specific workitem from the queue
        @return true if it was removed, false otherwise */
    bool Remove(WorkItem* item);
    //! dangerous
    void Cancel();

  private:
    /** @brief Pop one work item from the queue
        @return A work item or NULL when the queue is empty */
    WorkItem* Pop();

  private:
    friend class boost::thread;
    void CleanupWorkItem(WorkItem* item);

    boost::mutex m_mutex;
    boost::mutex m_lock;
    boost::condition_variable m_cond;
    // this is a priority queue maintained as a heap stored in a vector :o
    std::vector<WorkItem*> m_queue;
    bool m_dying;
};


/** @brief Thread which processes WorkItems in it's WorkItemQueue */
class WorkerThread : public boost::noncopyable
{
  public:
    WorkerThread();
	~WorkerThread();
    /** @brief Adds a new WorkItem to the queue */
    void DoWork(WorkItem* item, int priority = 0, bool toBeDeleted = true);
	//! joins underlying thread
	void Wait();
  private:
    friend class boost::thread;
    WorkItemQueue m_workeritemqueue;
	boost::thread* m_thread;
    boost::mutex m_mutex;
};

} // namespace LSL

/**
 * \file thread.h
 * \section LICENSE
Copyright 2012 by The libSpringLobby team. All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are
permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice, this list of
      conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright notice, this list
      of conditions and the following disclaimer in the documentation and/or other materials
      provided with the distribution.

THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**/

#endif // LIBUNITSYNCPP_THREAD_H
