#pragma once
#include <vector>
#include <queue>
#include <stack>




/*
	uint64_t handle pool which utilizes 32-bit key + 32-bit generational counter.
	stores contiguous data of user choice (ResourceT) which requires:

	struct YourStruct
	{
		...
		...
		uint64_t handle;		**
		void destroy() {}		**
	}

*/

template <typename ResourceT, uint32_t MAX_ELEMENTS = std::numeric_limits<uint32_t>::max()>
class HandlePool
{
public:
	using full_key = uint64_t;
	using half_key = uint32_t;
	static constexpr uint8_t INVALID_HANDLE = 0;

	struct Allocation
	{
		full_key handle;
		ResourceT* resource;
	};

public:
	HandlePool();

	Allocation get_next_free_handle();
	void free_handle(full_key key);
	ResourceT* get_resource(full_key key);

private:
	static constexpr full_key INDEX_SHIFT = std::numeric_limits<half_key>::digits;
	static constexpr full_key SLOT_MASK = ((full_key)1 << INDEX_SHIFT) - 1;
	static constexpr full_key GENERATION_MASK = ~SLOT_MASK;

	/*
		Limited queue implementation specifically for this class:
		This new iteration HandlePool didn't hold up to my previous iteration in terms of performance (which worked on pre-allocated memory, with no resizing support).
		It turned out to be std::queue, which probably doesn't use the memory effectively, so I decided to wrap a vector and provide the needed utilities.
		This did in turn increase the performance and it is now on par with my previous C implementation.

		Drawback is that the queue is as large as its achieved maximum size in its lifetime. It doesnt
	*/
	template <typename T>
	struct PrivateQueue
	{
		std::vector<T> m_reusable_keys;
		uint32_t m_tail{ 0 };
		uint32_t m_head{ 0 };

		PrivateQueue()
		{
			m_reusable_keys.resize(1);
		}

		void push(T value)
		{
			const uint32_t placement = m_head++;
			if (m_reusable_keys.size() < (m_head - m_tail))			// manual resizing
				m_reusable_keys.resize(m_reusable_keys.size() * 2);
			m_reusable_keys[placement] = value;
		}

		T front()
		{
			//return m_reusable_keys[m_tail++];
			return m_reusable_keys[m_tail];
		}

		void pop()
		{
			++m_tail;
		}

		bool empty()
		{
			auto ret = m_head == m_tail;
			// if empty, re-use memory
			if (ret)
				m_head = m_tail = 0;
			return ret;
		}
	};

	template <typename T>
	class PrivateStack
	{
	public:
		void push(T value)
		{
			const uint32_t placement = m_end++;

			// manual resizing if needed
			if (m_reusable_keys.size() < m_end)
				m_reusable_keys.resize(m_reusable_keys.size() * 2);

			m_reusable_keys[placement] = value;
		}

		T top()
		{
			assert(!empty());
			return m_reusable_keys[m_end - 1];
		}

		void pop()
		{
			assert(!empty());
			--m_end;
		}

		bool empty()
		{
			return m_end == 0;
		}

	private:
		std::vector<T> m_reusable_keys{ 0 };
		uint32_t m_end{ 0 };
	};

private:
	std::vector<ResourceT> m_resources;								// Only as large as handles 'in play' + invalid
	std::vector<half_key> m_generational_counters{ INVALID_HANDLE };				// Only as large as handles 'in play' + invalid

	//PrivateQueue<half_key> m_reusable_keys;			// Fast
	//std::queue<half_key> m_reusable_keys;			// Slow			
	//std::stack<half_key> m_reusable_keys;
	PrivateStack<half_key> m_reusable_keys;

};

template<typename ResourceT, uint32_t MAX_ELEMENTS>
inline HandlePool<ResourceT, MAX_ELEMENTS>::HandlePool()
{
	m_resources.resize(1);
}

template<typename ResourceT, uint32_t MAX_ELEMENTS>
inline typename HandlePool<ResourceT, MAX_ELEMENTS>::Allocation HandlePool<ResourceT, MAX_ELEMENTS>::get_next_free_handle()
{
	assert(m_resources.size() <= MAX_ELEMENTS);

	half_key curr_gen{};
	half_key new_key{};

	if (m_reusable_keys.empty())
	{
		// grow container since there are no reusable keys left
		new_key = (half_key)m_resources.size();
		m_resources.push_back(ResourceT{});

		// grow generational counter too for accomodating the new usable key
		m_generational_counters.push_back(curr_gen);
	}
	else
	{
		// grab from reusable keys
		//new_key = m_reusable_keys.front();
		new_key = m_reusable_keys.top();
		m_reusable_keys.pop();

		// grab current generational counter for that key
		curr_gen = m_generational_counters[new_key];
	}
	auto& resource = m_resources[new_key];

	// construct full key
	const full_key handle = (((full_key)curr_gen) << INDEX_SHIFT) | (((full_key)new_key) & SLOT_MASK);
	resource.handle = handle;

	const HandlePool<ResourceT, MAX_ELEMENTS>::Allocation alloc{ handle, &resource };
	return alloc;
}

template<typename ResourceT, uint32_t MAX_ELEMENTS>
inline void HandlePool<ResourceT, MAX_ELEMENTS>::free_handle(full_key handle)
{
	const half_key key = (half_key)(handle & SLOT_MASK);
	const half_key gen = (half_key)((handle & GENERATION_MASK) >> INDEX_SHIFT);
	auto& resource = m_resources[key];


	// most recent generation
	const half_key curr_gen = m_generational_counters[key];

	// tackle double-free
	// we check that the handle passed in has the most updated version
	assert(handle == resource.handle);
	assert(curr_gen == gen);

	resource.destroy();
	//resource.handle = 0;

	// increase generational counter for next-reuse
	// also handle overflow, which in this case skips pushing it to reusable queue
	const half_key next_gen = ++m_generational_counters[key];
	if (next_gen < curr_gen)
		return;

	m_reusable_keys.push(key);
}

template<typename ResourceT, uint32_t MAX_ELEMENTS>
inline ResourceT* HandlePool<ResourceT, MAX_ELEMENTS>::get_resource(full_key handle)
{
	const half_key key = (half_key)(handle & SLOT_MASK);
	const half_key gen = (half_key)((handle & GENERATION_MASK) >> INDEX_SHIFT);
	assert(key <= m_resources.size());
	assert(key != INVALID_HANDLE);

	// we check that the handle passed in has the most updated version
	//assert(m_resources[key].handle == handle);
	const half_key curr_gen = m_generational_counters[key];
	assert(curr_gen == gen);

	return &m_resources[key];
}
