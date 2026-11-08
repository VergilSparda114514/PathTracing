#include <Task.h>

void TaskScheduler::Update()
{
	for (const auto& [type, scheduler] : Singleton<TaskScheduler>::Get().m_Schedulers)
	{
		scheduler();
	}
}