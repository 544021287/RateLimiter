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
 *	�����������qpsΪ1,000,000,000,��СΪ1
 *	ʹ�ã�
 *	һ������ʽ
 *		RateLimiter r(100);
 *		r.pass(1);
 *	����������ʽ
 *		RateLimiter r(100, 100);
 *		r.tryAcquire(1, 1000 * 1000);
 *	��ͨ��r.pass()�������ɱ�֤����
 *	�ص㣺
 *		1���ӿ�ʹ�ü򵥣���ҵ�����룬����ɱ�����
 *		2���̰߳�ȫ��CPU�Ѻã�����ǿ��
 *		3�������������Ĵ���150��
 *	ԭ��
 *	��������Ͱ�㷨ʵ��
 */
class RateLimiter
{
public:
/*
 *	����˵�� :	qps�������Ϊʮ�ڡ�
 *	<qps>	:	��ʼ������(QPS)��
 *	<bucketSize>	:	��ʼ��Ͱ��С��
 * 	����ֵ	:	��
 */
	RateLimiter(int64_t qps, uint64_t bucketSize = 1);

	DISALLOW_COPY_MOVE_AND_ASSIGN(RateLimiter);

/*
 *	����˵�� :	����ӿڣ�������ʽ�ȴ���
 *	<cnt>	:	Ҫ���ȡ��token������
 *	<overtime_ns>	:	��ȴ�ʱ��(����)��
 * 	����ֵ	:	true˵���������޶�ֵ�ڣ�false��ʾ�ڵȴ�ʱ�������������޶�ֵ��
 */
	bool tryAcquire(int64_t cnt, uint64_t overtime_ns);//����

/*
 *	����˵�� :	����ӿڣ�����ʽ�ȴ���
 *	<cnt>	:	Ҫ���ȡ��token������
 * 	����ֵ	:	�ޡ�
 */
	bool acquire(int64_t cnt);

	bool runService();

	bool stopService();

private:
/*
 *	����˵�� :	��ȡ��ǰϵͳʱ�䡣
 * 	����ֵ	:	��ǰϵͳʱ��(����)��
 */
	nano_time_point _now();

	//��������Ͱ�е�����
/*
 *	����˵�� :	��������Ͱ�е����ơ�
 * 	����ֵ	:	��
 */
	void _supplyTokens();

/*
 *	����˵�� :	�������ķ�ʽ���Ի�ȡtoken��
 *	<cnt>	:	Ҫ���ȡ��token������
 * 	����ֵ	:	true˵���ɹ���ȡ��Ҫ��������token��false��ʾ��ȡʧ�ܡ�
 */
	bool _tryGetToken(int64_t cnt);

/*
 *	����˵�� :	����<overtime_ns>ȷ������ʽ���Ƿ�����ʽ�ȴ���
 *	<cnt>	:	Ҫ���ȡ��token������
 *	<overtime_ns>	:	��ȴ�ʱ��(����)������Ϊ��ֵ��
 * 	����ֵ	:	true˵���������޶�ֵ�ڣ�false��ʾ�ڵȴ�ʱ�������������޶�ֵ��
 */
	bool _tryGetToken(int64_t cnt, int64_t overtime_ns);//����

/*
 *	����˵�� :	����ʽ�ȴ���ȡtoken��
 *	<cnt>	:	Ҫ���ȡ��token������
 * 	����ֵ	:	��
 */
	void _mustGetToken(int64_t cnt);
	
	void _thread(void* param);

	void _clearQueue();

private:
	const int64_t m_bucketSize;			//����Ͱ��С
	std::atomic<int64_t> m_tokenLeft;	////ʣ�µ�token��
	std::chrono::nanoseconds m_supplyUnitTime;		//�������Ƶĵ�λʱ��
	nano_time_point  m_lastAddTokenTime;			//�ϴβ������Ƶ�ʱ�䣬��λ����
	std::mutex m_lock;					//������
	std::deque<std::shared_ptr<PriorityQueueElement>> m_waitQueue;	//���ȼ��ȴ����У���ֹ���߳���Զ�ò���token
	std::timed_mutex m_queueLock;		//���ȼ��ȴ����е���
	std::thread* m_threadId;
	bool m_ctl;
};