//@author wyt
#pragma once

#include <assert.h>
#include <mutex>
#include <atomic>
#include <queue>
#include <future>

#define DISALLOW_COPY_MOVE_AND_ASSIGN(TypeName) TypeName(const TypeName&) = delete; TypeName(const TypeName&&) = delete;  TypeName& operator=(const TypeName&) = delete

#define NS_PER_SECOND 1000000000
#define NS_PER_USECOND 1000

typedef std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds>
nano_time_point;

typedef struct _PriorityQueueElement{
	std::promise<int> promiseobj;
	std::future<int> futureobj;
	nano_time_point endtime;
	std::chrono::nanoseconds duration;
	int64_t tokens;
}PriorityQueueElement;

/*
 *	限流器，最大qps为1,000,000,000,最小为1
 *	使用：
 *	一：阻塞式
 *		RateLimiter r(100);
 *		r.pass(1);
 *	二：非阻塞式
 *		RateLimiter r(100, 100);
 *		r.tryAcquire(1, 1000 * 1000);
 *	能通过r.pass()函数即可保证流速
 *	特点：
 *		1、接口使用简单，无业务侵入，接入成本极低
 *		2、线程安全，CPU友好，性能强劲
 *		3、极轻量，核心代码150行
 *	原理：
 *	基于令牌桶算法实现
 */
class RateLimiter
{
public:
/*
 *	函数说明 :	qps限制最大为十亿。
 *	<qps>	:	初始化速率(QPS)。
 *	<bucketSize>	:	初始化桶大小。
 * 	返回值	:	无
 */
	RateLimiter(int64_t qps, uint64_t bucketSize = 1);

	DISALLOW_COPY_MOVE_AND_ASSIGN(RateLimiter);

/*
 *	函数说明 :	对外接口，非阻塞式等待。
 *	<cnt>	:	要求获取的token数量。
 *	<overtime_ns>	:	最长等待时间(纳秒)。
 * 	返回值	:	true说明流量在限定值内，false表示在等待时间内流量超过限定值。
 */
	bool tryAcquire(int64_t cnt, uint64_t overtime_ns);//纳秒

/*
 *	函数说明 :	对外接口，阻塞式等待。
 *	<cnt>	:	要求获取的token数量。
 * 	返回值	:	无。
 */
	bool acquire(int64_t cnt);

	bool runService();

	bool stopService();

private:
/*
 *	函数说明 :	获取当前系统时间。
 * 	返回值	:	当前系统时间(纳秒)。
 */
	nano_time_point _now();

	//更新令牌桶中的令牌
/*
 *	函数说明 :	更新令牌桶中的令牌。
 * 	返回值	:	无
 */
	void _supplyTokens();

/*
 *	函数说明 :	非阻塞的方式尝试获取token。
 *	<cnt>	:	要求获取的token数量。
 * 	返回值	:	true说明成功获取到要求数量的token，false表示获取失败。
 */
	bool _tryGetToken(int64_t cnt);

/*
 *	函数说明 :	根据<overtime_ns>确定阻塞式还是非阻塞式等待。
 *	<cnt>	:	要求获取的token数量。
 *	<overtime_ns>	:	最长等待时间(纳秒)，必须为正值。
 * 	返回值	:	true说明流量在限定值内，false表示在等待时间内流量超过限定值。
 */
	bool _tryGetToken(int64_t cnt, int64_t overtime_ns);//纳秒

/*
 *	函数说明 :	阻塞式等待获取token。
 *	<cnt>	:	要求获取的token数量。
 * 	返回值	:	无
 */
	void _mustGetToken(int64_t cnt);
	
	void _thread(void* param);

	void _clearQueue();

private:
	const int64_t m_bucketSize;			//令牌桶大小
	std::atomic<int64_t> m_tokenLeft;	////剩下的token数
	std::chrono::nanoseconds m_supplyUnitTime;		//补充令牌的单位时间
	nano_time_point  m_lastAddTokenTime;			//上次补充令牌的时间，单位纳秒
	std::mutex m_lock;					//自旋锁
	std::deque<std::shared_ptr<PriorityQueueElement>> m_waitQueue;	//优先级等待队列，防止有线程永远得不到token
	std::timed_mutex m_queueLock;		//优先级等待队列的锁
	std::thread* m_threadId;
	bool m_ctl;
};