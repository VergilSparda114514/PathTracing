#pragma once

#include <coroutine>
#include <exception>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <atomic>
#include <functional>
#include <typeinfo>

#include <Singleton.h>

#define await(x) do\
{\
	auto gen = x;\
	while (gen)\
	{\
		co_await std::suspend_always{};\
		gen();\
	}\
} while (false)

template <bool AwaitReady = false>
class awaitable
{
public:
	using fn_type = std::function<void()>;
public:
	explicit awaitable(fn_type&& fn) : m_Fn(std::forward<fn_type>(fn)) {}

	awaitable(const awaitable&) = default;
	awaitable(awaitable&&) noexcept = default;

	~awaitable() = default;

	constexpr bool await_ready() const { return AwaitReady; }
	constexpr void await_suspend(std::coroutine_handle<>) { m_Fn(); }
	constexpr void await_resume() const {}

	awaitable& operator=(const awaitable&) = default;
	awaitable& operator=(awaitable&&) noexcept = default;
private:
	fn_type m_Fn{};
};

template <typename T>
class promise_base
{
public:
	std::suspend_always initial_suspend() { return {}; }
	std::suspend_always final_suspend() noexcept { return {}; }

	template <std::convertible_to<T> U>
	std::suspend_always yield_value(U&& value)
	{
		m_Value = value;
		return {};
	}

	void return_void() {}
	void unhandled_exception()
	{
		m_Exception = std::current_exception();
	}

	const T& get_value() const { return m_Value; }
	std::exception_ptr get_exception() const { return m_Exception; }
private:
	T m_Value{};
	std::exception_ptr m_Exception = nullptr;
};

template <>
class promise_base<void>
{
public:
	std::suspend_always initial_suspend() { return {}; }
	std::suspend_always final_suspend() noexcept { return {}; }

	void return_void() {}
	void unhandled_exception()
	{
		m_Exception = std::current_exception();
	}

	std::exception_ptr get_exception() const { return m_Exception; }
private:
	std::exception_ptr m_Exception = nullptr;
};

class TaskUpdater
{
public:
	template <class T>
	static void ScheduleUpdate()
	{
		Singleton<TaskUpdater>::Get().m_Schedulers[typeid(T).hash_code()] = [] { T::Update(); };
	}

	template <class T>
	static void DescheduleUpdate()
	{
		Singleton<TaskUpdater>::Get().m_Schedulers.erase(typeid(T).hash_code());
	}

	static void Update();
private:
	std::unordered_map<size_t, std::function<void()>> m_Schedulers{};
};

template <typename T = void>
class Task
{
public:
	class promise_type : public promise_base<T>
	{
	public:
		Task get_return_object() noexcept
		{
			return Task{ std::coroutine_handle<promise_type>::from_promise(*this) };
		}
	};

	using handle_type = std::coroutine_handle<promise_type>;
public:
	Task(const Task&) = delete;
	Task(Task&& other) noexcept : m_Coro(std::exchange(other.m_Coro, {})) {}

	~Task()
	{
		if (m_Coro)
		{
			m_Coro.destroy();
		}
	}

	constexpr size_t GetID() const { return m_ID; }

	void operator()()
	{
		m_Coro();
	}

	constexpr operator bool() const { return !m_Coro.done(); }

	Task& operator=(const Task&) = delete;
	Task& operator=(Task&& other) noexcept
	{
		m_Coro = std::exchange(other.m_Coro, {});
		return *this;
	}
public:
	using scheduler_type = std::vector<Task>;
public:
	static const scheduler_type& GetTasks()
	{
		return Singleton<scheduler_type>::Get();
	}

	static size_t Schedule(Task&& task)	
	{
		static std::atomic<size_t> nextID = 0;

		Singleton<scheduler_type>::Get().emplace_back(std::forward<Task>(task)).m_ID = ++nextID;
		TaskUpdater::ScheduleUpdate<Task>();

		return nextID;
	}

	static bool TrySchedule(size_t& id, Task&& task)
	{
		bool done = Done(id);

		if (done)
		{
			id = Schedule(std::forward<Task>(task));
		}

		return done;
	}

	static void Update()
	{
		scheduler_type& scheduler = Singleton<scheduler_type>::Get();

		while (!std::ranges::all_of(scheduler, [](Task& task) -> bool { return task; }))
		{
			scheduler.erase(std::ranges::find_if_not(scheduler, [](Task& task) -> bool { return task; }));
		}

		std::ranges::for_each(scheduler, [](Task& task) { task(); });

		if (scheduler.size() <= 0)
		{
			TaskUpdater::DescheduleUpdate<Task>();
		}
	}

	static bool Done(size_t id)
	{
		scheduler_type& scheduler = Singleton<scheduler_type>::Get();
		return std::ranges::find_if(scheduler, [id](Task& task) { return task.GetID() == id; }) == scheduler.end();
	}
private:
	explicit Task(handle_type coro) : m_Coro(coro) {}
private:
	size_t m_ID = 0;
	handle_type m_Coro{};
};