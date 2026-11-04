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

#define await(x) for (auto gen = (x); gen; co_await std::suspend_always{}, gen())

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

	template <std::convertible_to<T> U>
	void return_value(U&& value)
	{
		m_Value = value;
	}

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

class TaskScheduler
{
public:
public:
	template <class Task>
	static const std::vector<Task>& GetTasks()
	{
		return Singleton<std::vector<Task>>::Get();
	}

	template <class Task>
	static size_t Schedule(Task&& task)
	{
		static std::atomic<size_t> nextID = 0;

		Singleton<std::vector<Task>>::Get().emplace_back(std::forward<Task>(task)).m_ID = ++nextID;
		ScheduleUpdate<Task>();

		return nextID;
	}

	template <class Task>
	static void Deschedule(size_t id)
	{
		std::vector<Task>& tasks = Singleton<std::vector<Task>>::Get();
		tasks.erase(std::ranges::find_if(tasks, [id](Task& task) { return task.GetID() == id; }));
	}

	template <class Task>
	static bool TrySchedule(size_t& id, Task&& task)
	{
		bool done = Done<Task>(id);

		if (done)
		{
			id = Schedule<Task>(std::forward<Task>(task));
		}

		return done;
	}

	template <class Task>
	static bool Done(size_t id)
	{
		std::vector<Task>& tasks = Singleton<std::vector<Task>>::Get();
		return std::ranges::find_if(tasks, [id](Task& task) { return task.GetID() == id; }) == tasks.end();
	}

	static void Update();
private:
	template <class Task>
	static void ScheduleUpdate()
	{
		Singleton<TaskScheduler>::Get().m_Schedulers[typeid(Task).hash_code()] = [] { Update<Task>(); };
	}

	template <class Task>
	static void DescheduleUpdate()
	{
		Singleton<TaskScheduler>::Get().m_Schedulers.erase(typeid(Task).hash_code());
	}

	template <class Task>
	static void Update()
	{
		std::vector<Task>& tasks = Singleton<std::vector<Task>>::Get();

		while (!std::ranges::all_of(tasks, [](Task& task) -> bool { return task; }))
		{
			tasks.erase(std::ranges::find_if_not(tasks, [](Task& task) -> bool { return task; }));
		}

		std::ranges::for_each(tasks, [](Task& task) { task(); });

		if (tasks.size() <= 0)
		{
			DescheduleUpdate<Task>();
		}
	}
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

	Task& Join()
	{
		while (*this)
		{
			(*this)();
		}

		return *this;
	}

	constexpr size_t GetID() const { return m_ID; }

	template <bool Move = true, std::convertible_to<T> U = T>
	std::conditional_t<Move, U&&, U> GetValue() const
	{
		return Move ? std::move(m_Coro.promise().get_value()) : m_Coro.promise().get_value();
	}

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
private:
	friend class TaskScheduler;

	explicit Task(handle_type coro) : m_Coro(coro) {}
private:
	size_t m_ID = 0;
	handle_type m_Coro{};
};