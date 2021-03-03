//@author wyt

#include "rate_limiter.h"
#include <mutex>
#include <thread>
#include <float.h>
using namespace std::chrono;

#define RETRY_IMMEDIATELY_TIMES 100//��˯�ߵ�������Ի�����ƵĴ���

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

//������
bool RateLimiter::tryAcquire(int64_t cnt, uint64_t overtime_ns)
{
	if (cnt > m_bucketSize) return false;
	int64_t overtime = overtime_ns;

	//non-block to get token
	return _tryGetToken(cnt, overtime);
}

//����
bool RateLimiter::acquire(int64_t cnt)
{
	if (cnt > m_bucketSize) return false;
	_mustGetToken(cnt);
	return true;
}

bool RateLimiter::_tryGetToken(int64_t cnt)
{
	_supplyTokens();

	//���һ������
	auto token = m_tokenLeft.fetch_add(-cnt);
	if (token <= 0) {//�Ѿ�û�������ˣ��黹͸֧������
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
			//�ó�CPU
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
			//�ó�CPU
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
		//�ȴ��������ڼ�����Ѿ������������
		int64_t newTokens = (cur - m_lastAddTokenTime) / m_supplyUnitTime;
		if (newTokens <= 0) {
			return;
		}
		
		//���²���ʱ��,����ֱ��=cur������ᵼ��ʱ�䶪ʧ
		m_lastAddTokenTime += (newTokens * m_supplyUnitTime);
		
		auto freeRoom = m_bucketSize - m_tokenLeft.load();
		if(newTokens > freeRoom || newTokens > m_bucketSize) {
			newTokens = freeRoom > m_bucketSize ?
				m_bucketSize : freeRoom;
		}
		
		m_tokenLeft.fetch_add(newTokens);
	}
}