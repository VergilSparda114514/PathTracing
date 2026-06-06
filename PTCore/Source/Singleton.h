#pragma once

template <typename T>
class Singleton
{
public:
	Singleton() = delete;
	Singleton(Singleton&&) = delete;
	Singleton(const Singleton&) = delete;

	~Singleton() = delete;

	static T& Get()
	{
		static T instance;
		return instance;
	}

	void operator=(Singleton&&) = delete;
	void operator=(const Singleton&) = delete;
};