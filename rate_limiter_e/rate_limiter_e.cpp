//@author wyt

#include "rate_limiter_e.h"
#include <mutex>
#include <thread>
#include <float.h>

using namespace std::chrono;

#define RETRY_IMMEDIATELY_TIMES 100//不睡眠的最大重试获得令牌的次数

RateLimiter::RateLimiter(int64_t qps, uint64_t bucketSize) :
	m_bucketSize(bucketSize), 
	m_tokenLeft(0), 
	m_supplyUnitTime(NS_PER_SECOND / qps)
{ 
	assert(qps <= NS_PER_SECOND);
	assert(qps >= 0);
	m_lastAddTokenTime = _now();
}

nano_time_point RateLimiter::_now()
{
	auto tp = std::chrono::time_point_cast<
		std::chrono::nanoseconds>(
			std::chrono::system_clock::now());
	return tp;
}

//非阻塞
bool RateLimiter::tryAcquire(int64_t cnt, uint64_t overtime_ns)
{
	if (cnt > m_bucketSize) return false;
	nano_time_point starttime = _now();
	int64_t overtime = overtime_ns;
	std::shared_ptr<PriorityQueueElement> ele =
		std::make_shared<PriorityQueueElement>();

	ele->tokens = cnt;
	ele->endtime = starttime + std::chrono::nanoseconds(overtime_ns);
	ele->duration = std::chrono::nanoseconds(overtime_ns);
	ele->futureobj = ele->promiseobj.get_future();

	while (!m_queueLock.try_lock_for(100us)) {
		if (_now() >= ele->endtime)
			return false;
	}
	m_waitQueue.push_back(ele);
	m_queueLock.unlock();

	auto res = ele->futureobj.wait_until(ele->endtime);
	if (res != std::future_status::ready) {
		return false;
	}
	//获取成功
	return true;
}

//阻塞
#include <iostream>
bool RateLimiter::acquire(int64_t cnt)
{
	if (cnt > m_bucketSize) return false;
	auto starttime = _now();
	std::shared_ptr<PriorityQueueElement> ele =
		std::make_shared<PriorityQueueElement>();

	ele->tokens = cnt;
	ele->endtime = starttime - starttime.time_since_epoch();
	ele->duration = std::chrono::nanoseconds::max();
	ele->futureobj = ele->promiseobj.get_future();

	m_queueLock.lock();
	m_waitQueue.push_back(ele);
	m_queueLock.unlock();

	ele->futureobj.wait();
	//获取成功
	return true;
}

bool RateLimiter::runService()
{
	m_ctl = true;
	m_threadId = new std::thread(&RateLimiter::_thread, this, nullptr);
	if (m_threadId == nullptr) {
		return false;
	}
	return true;
}

bool RateLimiter::stopService()
{
	m_ctl = false;
	if (m_threadId->joinable())
		m_threadId->join();
	return true;
}

bool RateLimiter::_tryGetToken(int64_t cnt)
{
	//_supplyTokens();

	//获得一个令牌
	auto token = m_tokenLeft.fetch_add(-cnt);
	if (token <= 0) {//已经没有令牌了，归还透支的令牌
		m_tokenLeft.fetch_add(cnt);
		return false;
	}

	return true;
}

bool RateLimiter::_tryGetToken(int64_t cnt, int64_t overtime_ns)
{
	assert(overtime_ns >= 0);

	bool isGetToken = false;
	auto endtime = _now() + std::chrono::nanoseconds(overtime_ns);
	for (int i = 0; i < RETRY_IMMEDIATELY_TIMES; ++i) {
		isGetToken = _tryGetToken(cnt);
		if (isGetToken) {
			return true;
		}
		if (_now() > endtime) {
			return false;
		}
	}

	while (1)
	{
		isGetToken = _tryGetToken(cnt);
		if (isGetToken) {
			return true;
		}
		else {
			//让出CPU
			if (_now() > endtime) {
				break;
			}
			std::this_thread::sleep_for(100us);
		}
	}
	return false;
}

void RateLimiter::_mustGetToken(int64_t cnt)
{
	bool isGetToken = false;

	for(int i = 0; i < RETRY_IMMEDIATELY_TIMES; ++i) {
		isGetToken =  _tryGetToken(cnt);
		if(isGetToken) {
			return;
		}
	}

	while(1) {
		isGetToken =  _tryGetToken(cnt);
		if(isGetToken) {
			return;
		}
		else {
			//让出CPU
			std::this_thread::sleep_for(100us);
		}
	}
}

void RateLimiter::_thread(void* param)
{
	while (m_ctl)
	{
		if (m_waitQueue.empty()) {
			std::this_thread::sleep_for(100us);
			continue;
		}

		while (!m_queueLock.try_lock_for(100us))
			continue;

		_clearQueue();
		_supplyTokens();

		auto ele = m_waitQueue.front();
		if (ele->tokens <= m_tokenLeft) {
			m_waitQueue.pop_front();
			m_queueLock.unlock();
			_tryGetToken(ele->tokens);
			ele->promiseobj.set_value(1);
		}
		else {
			m_queueLock.unlock();
			std::this_thread::sleep_for(100us);
		}
	}
}

void RateLimiter::_supplyTokens()
{
	auto cur = _now();
	if (cur - m_lastAddTokenTime < m_supplyUnitTime) {
		return;
	}

	{
		std::lock_guard<std::mutex> lockguard(m_lock);
		//等待自旋锁期间可能已经补充过令牌了
		int64_t newTokens = (cur - m_lastAddTokenTime) / m_supplyUnitTime;
		if (newTokens <= 0) {
			return;
		}
		
		//更新补充时间,不能直接=cur，否则会导致时间丢失
		m_lastAddTokenTime += (newTokens * m_supplyUnitTime);
		
		auto freeRoom = m_bucketSize - m_tokenLeft.load();
		if(newTokens > freeRoom || newTokens > m_bucketSize) {
			newTokens = freeRoom > m_bucketSize ?
				m_bucketSize : freeRoom;
		}
		
		m_tokenLeft.fetch_add(newTokens);
	}
}

void RateLimiter::_clearQueue()
{
	while (!m_waitQueue.empty()) {
		auto element = m_waitQueue.front();
		if (element->duration != std::chrono::nanoseconds::max()
			&& _now() > element->endtime) {
			m_waitQueue.pop_front();
			continue;
		}
		break;
	}
}