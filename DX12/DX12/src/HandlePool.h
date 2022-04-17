#pragma once
#include <vector>
#include <queue>

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
	Allocation get_next_free_handle();
	void free_handle(full_key key);
	ResourceT* get_resource(full_key key);

private:
	static constexpr full_key INDEX_SHIFT = std::numeric_limits<half_key>::digits;
	static constexpr full_key SLOT_MASK = ((full_key)1 << INDEX_SHIFT) - 1;

private:
	std::vector<ResourceT> m_resources{ ResourceT{} };							// Only as large as handles 'in play' + invalid
	std::vector<half_key> m_generational_counters{ INVALID_HANDLE };				// Only as large as handles 'in play' + invalid
	std::queue<half_key> m_reusable_keys;				

};

template<typename ResourceT, uint32_t MAX_ELEMENTS>
inline typename HandlePool<ResourceT, MAX_ELEMENTS>::Allocation HandlePool<ResourceT, MAX_ELEMENTS>::get_next_free_handle()
{
	assert(m_resources.size() <= MAX_ELEMENTS);
	
	HandlePool<ResourceT, MAX_ELEMENTS>::Allocation alloc{};

	if (m_reusable_keys.empty())
	{
		// grow container since there are no reusable keys left
		const half_key new_key = (half_key)m_resources.size();
		m_resources.push_back(ResourceT{});

		// grow generational counter too for accomodating the new usable key
		static constexpr half_key fresh_gen = 0;
		m_generational_counters.push_back(fresh_gen);

		// construct full key
		full_key handle = (((full_key)fresh_gen) << INDEX_SHIFT) | (((full_key)new_key) & SLOT_MASK);
		m_resources[new_key].handle = handle;
		
		alloc.handle = handle;
		alloc.resource = &m_resources[new_key];
	}
	else
	{
		// grab from reusable keys
		const half_key new_key = m_reusable_keys.front();
		m_reusable_keys.pop();

		// grab current generational counter for that key
		const half_key curr_gen = m_generational_counters[new_key];

		// construct full key
		full_key handle = (((full_key)curr_gen) << INDEX_SHIFT) | (((full_key)new_key) & SLOT_MASK);
		m_resources[new_key].handle = handle;

		alloc.handle = handle;
		alloc.resource = &m_resources[new_key];
	}
	return alloc;
}

template<typename ResourceT, uint32_t MAX_ELEMENTS>
inline void HandlePool<ResourceT, MAX_ELEMENTS>::free_handle(full_key handle)
{
	const half_key key = (half_key)(handle & SLOT_MASK);
	auto& resource = m_resources[key];

	// tackle double-free
	assert(handle == resource.handle);

	resource.destroy();
	resource.handle = 0;

	// increase generational counter for next-reuse
	// also handle overflow, which in this case skips pushing it to reusable queue
	const half_key prev_gen = m_generational_counters[key];
	const half_key next_gen = ++m_generational_counters[key];
	if (next_gen < prev_gen)
		return;

	m_reusable_keys.push(key);
}

template<typename ResourceT, uint32_t MAX_ELEMENTS>
inline ResourceT* HandlePool<ResourceT, MAX_ELEMENTS>::get_resource(full_key handle)
{
	const half_key key = (half_key)(handle & SLOT_MASK);
	assert(key <= m_resources.size());
	assert(key != INVALID_HANDLE);

	return &m_resources[key];
}
