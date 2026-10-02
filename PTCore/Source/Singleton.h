#pragma once

template <typename T>
class Singleton
{
public:
	Singleton(Singleton&&) = delete;
	Singleton(const Singleton&) = delete;

	static T& Get()
	{
		static T instance;
		return instance;
	}

	void operator=(Singleton&&) = delete;
	void operator=(const Singleton&) = delete;
private:
	Singleton() = default;
	~Singleton() = default;
};