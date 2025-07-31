#pragma once

#include "types.h"
#include <mutex>
#include <semaphore>
#include <vector>
#include "SDL.h"

class WorkerFarm
{
public:
	WorkerFarm()
	{
		u32 num_processors = std::thread::hardware_concurrency();
		for (u32 i = 0; i < num_processors-1; i++)
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
			auto task = m_tasks.back();
			m_tasks.pop_back();
			m_access.unlock();
			task();
			m_access.lock();
			m_taskCount--;
			m_access.unlock();
		}
	}

	void QueueTask(const GenericTask& task)
	{
		m_access.lock();
		m_tasks.push_back(task);
		m_taskCount++;
		m_access.unlock();
		m_semaphore.release();
	}

	void WaitForTasks()
	{
		while (m_taskCount > 0)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
	}

private:
	std::vector<std::thread *> m_processors;
	std::vector<GenericTask> m_tasks;
	std::counting_semaphore<> m_semaphore{ 0 };
	std::mutex m_access;

	volatile int m_taskCount = 0;
};
