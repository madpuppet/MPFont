#pragma once

#include "types.h"
#include <mutex>
#include <semaphore>
#include <vector>
#include "SDL3/SDL.h"

#undef max

class WorkerFarm
{
public:
	WorkerFarm()
	{
		u32 num_processors = std::thread::hardware_concurrency();
		u32 spawnThreads = std::max((u32)1, num_processors - 2);
		for (u32 i = 0; i < spawnThreads; i++)
		{
			m_processors.push_back(new std::thread([this, i]() {Process(i); }));
		}
	}

	void Process(int thread)
	{
		for (;;)
		{
			m_semaphore.acquire();
			m_access.lock();
			GenericTask task;
			if (!m_highPriorityTasks.empty())
			{
				task = m_highPriorityTasks.back();
				m_highPriorityTasks.pop_back();
			}
			else
			{
				task = m_lowPriorityTasks.back();
				m_lowPriorityTasks.pop_back();
			}
			m_access.unlock();
			if (!m_abort)
				task();
			m_access.lock();
			m_taskCount--;
			m_access.unlock();
		}
	}

	void QueueLowPriorityTask(const GenericTask& task)
	{
		m_access.lock();
		m_lowPriorityTasks.push_back(task);
		m_taskCount++;
		m_access.unlock();
		m_semaphore.release();
	}

	void QueueHighPriorityTask(const GenericTask& task)
	{
		m_access.lock();
		m_highPriorityTasks.push_back(task);
		m_taskCount++;
		m_access.unlock();
		m_semaphore.release();
	}

	void Abort()
	{
		m_abort = true;
	}

	void WaitForTasks()
	{
		while (m_taskCount > 0)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		m_abort = false;
	}

	int TasksRemaining()
	{
		return m_taskCount;
	}

private:
	bool m_abort = false;
	std::vector<std::thread *> m_processors;
	std::vector<GenericTask> m_lowPriorityTasks;
	std::vector<GenericTask> m_highPriorityTasks;
	std::counting_semaphore<> m_semaphore{ 0 };
	std::mutex m_access;

	volatile int m_taskCount = 0;
};
