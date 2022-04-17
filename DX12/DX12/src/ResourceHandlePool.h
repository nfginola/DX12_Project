#pragma once
#include <stdint.h>
#include <limits>
#include <vector>
#include <assert.h>

template <typename T, uint64_t total_elements = std::numeric_limits<uint16_t>::max()>
class ResourceHandlePool
{
	static_assert(total_elements <= std::numeric_limits<uint32_t>::max());
	static_assert(total_elements > 3);

public:
	static constexpr uint8_t INVALID_HANDLE = 0;

private:
	using half_key = uint32_t;
	using full_key = uint64_t;

	static constexpr full_key INDEX_SHIFT = std::numeric_limits<half_key>::digits;
	static constexpr full_key SLOT_MASK = ((full_key)1 << INDEX_SHIFT) - 1;

	/*
		Resource is REQUIRED to have a
			- .destroy() function for deallocating the underlying resource
			- A stored "handle" which is used to validate for uniqueness upon usage
				- handle = 0 indicates INVALID resource!
				- Should be as large as uin64_t

		struct SomeResource
		{
			...
			...

			uint64_t handle;
			void destroy() { resource freeing impl. }
		}

		Refer to the called functions on the underlying resource in the 'free_handle(..)' function.
	*/

public:
	ResourceHandlePool()
	{
		// check static constexprs ( breakpoint here to check, they are not visible in debugger :/ )
		//auto tmp1 = INDEX_SHIFT;
		//auto tmp2 = SLOT_MASK;

		m_resources.resize(3);
		m_free_indices.resize(3);

		// Reserve 0 for invalid handle
		//for (half_key i = 1; i < total_elements; ++i)
		//	m_free_indices[i] = i;

		m_gen_counter.resize(total_elements);
		std::fill(m_gen_counter.begin(), m_gen_counter.end(), 0);

		m_slots_enabled.resize(total_elements);
		m_slots_enabled[0] = false;	// reserved
		std::fill(m_slots_enabled.begin() + 1, m_slots_enabled.end(), true);
	}
	~ResourceHandlePool()
	{
		// Automatically free all remaining m_resources on destruction
		for (auto& res : m_resources)
		{
			if (res.handle == 0)
				continue;
			res.destroy();
			res.handle = 0;
		}
	};

	// Get next free handle
	std::pair<full_key, T*> get_next_free_handle()
	{
		//assert(top < total_elements);
		if (top >= total_elements)	// out of memory since top is beyond the stack now (index out of range)
			return { 0, nullptr };

		// get next free index from top of stack
		// should be a guarantee that no disabled slots are ever popped from stack and received here
		// (due to the fact that we never push a disabled slot onto the stack upon freeing)
		half_key idx = m_free_indices[top++];

		// get next generation counter for this index
		half_key ctr = m_gen_counter[idx]++;

		// disable slot when there are no unique patterns left (overflow)
		if (m_gen_counter[idx] < ctr)
			m_slots_enabled[idx] = false;

		// calculate handle (higher bits reserved for generational counter, lower bits for index)
		full_key hdl = (((full_key)ctr) << INDEX_SHIFT) | (((full_key)idx) & SLOT_MASK);

		m_resources[idx].handle = hdl;

		return { hdl, &m_resources[idx] };
	}

	// Free the handle AND free the underlying resource
	void free_handle(full_key hdl)
	{
		half_key idx = (half_key)(hdl & SLOT_MASK);

		// handle free-after-free
		assert(m_resources[idx].handle == hdl);

		// free resource
		m_resources[idx].destroy();
		m_resources[idx].handle = 0;

		/*
			observe that we always prioritize re-using freed indices over new indices using the stack.
			we always try to exhaust a slots unique pattern.
		*/

		// put back index to top of stack only if slot is still enabled
		if (m_slots_enabled[idx])
			m_free_indices[--top] = idx;
		/*
			otherwise, top position is kept thus valid indices are kept on top.
			there will never be a disabled slot in the stack of free indices.
		*/
	}

	// Get the underlying resource
	T* look_up(full_key hdl)
	{
		half_key idx = (half_key)(hdl & SLOT_MASK);

		// check that we are in range
		assert(idx < total_elements && hdl > 0);

		// check that the buffer handle is identical to the one at index
		// handle use-after-free
		assert(m_resources[idx].handle == hdl);

		return &m_resources[idx];
	}

	//uint64_t get_memory_footprint()
	//{
	//	return m_resources.size() * sizeof(m_resources[0]) +
	//		m_free_indices.size() * sizeof(m_free_indices[0]) +
	//		m_gen_counter.size() * sizeof(m_gen_counter[0]) +
	//		m_slots_enabled.size() * sizeof(m_slots_enabled[0]);
	//}

private:
	const uint64_t max_usable_elements = total_elements - 1;

	// Total number of elements including the reserved 0 index
	// Stored as a member variable just so we can easily check size in debugger, otherwise static constexpr
	//const uint64_t total_resource_bytes = total_elements * sizeof(T);
	//const uint64_t total_bookkeeping_bytes = total_elements * (sizeof(half_key) * 2 + sizeof(bool));

	// Always assumes that 0 is an invalid handle
	half_key top = 1;

	std::vector<T> m_resources;
	std::vector<half_key> m_free_indices;
	std::vector<half_key> m_gen_counter;
	std::vector<bool> m_slots_enabled;
};
