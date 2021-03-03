//@author wyt
#include <iostream>
#include <thread>
#include "rate_limiter_e.h"

int64_t now()
{
    auto tp = std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now());
    auto millsec = tp.time_since_epoch().count();
    int64_t seconds = millsec / 1000;
    return seconds * NS_PER_SECOND + (millsec % 1000) * 1000 * NS_PER_USECOND;
}

RateLimiter* r;

void myThread(void* param)
{
	for (int i = 0; i < 500; ++i)
	{
		r->acquire(1);
	}
}

int main()
{
	r = new RateLimiter(10000, 100);//100qps限流器

	r->runService();
	auto start = now();

	std::thread* threadId[100];
	for (auto& it : threadId)
	{
		it = new std::thread(&myThread, nullptr);
	}

	for (auto it : threadId)
	{
		if (it->joinable())
			it->join();
	}

	auto end = now();
	std::cout << (end - start) / 1000000 << "ms" << std::endl;//此时打印的时间为5000ms
	r->stopService();
	return 0;
}
