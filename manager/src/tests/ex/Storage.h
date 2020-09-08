#pragma once

#include <inttypes.h>
#include <stddef.h>
#include "Bitstream.h"

class Storage
{
public:
	Storage();

	struct Data
	{
		float temperature;
		float humidity;
	};

	struct iterator
	{
		Data data;
		uint16_t bit_offset = uint16_t(-1);
		int16_t group_idx = -1;
	};

	bool unpack_next(iterator& it) const;

	size_t get_data_count() const;
	uint16_t get_group_count() const;
	uint16_t get_first_group_idx() const;
	uint16_t get_last_group_idx() const;

	bool push_back(const Data& data);
	bool pop_front();

	void clear();
	bool empty() const;

private:

#ifndef __AVR__
#   pragma pack(push, 1) // exact fit - no padding
#endif

	struct Group
	{
		friend class Storage;
	protected:
		uint8_t m_is_initialized : 1;
		uint8_t m_skip_count : 7;
		int16_t m_qtemperature = 0;
		int16_t m_qhumidity = 0;

		uint16_t get_data_count() const;
		bool unpack_next(Storage::iterator& it) const;

		static constexpr uint16_t DATA_SIZE = 121;
		uint16_t m_bit_size = 0;
		uint8_t m_data[DATA_SIZE];
	};

#ifndef __AVR__
#   pragma pack(pop)
#endif

	static_assert(sizeof(Group) == 128);

// 	const Group& get_first_group() const;
// 	Group& get_first_group();
// 	const Group& get_last_group() const;
// 	Group& get_last_group();
	bool pop_first_group();
	bool add_group();
	bool load_group(uint16_t group_idx) const;

	bool _unpack_next(iterator& it) const;

	Data m_last_data;

	Group m_last_group;
	mutable Group m_iteration_group;
	mutable uint16_t m_iteration_group_idx = uint16_t(-1);

	uint16_t m_first_group_idx = 0;
	uint16_t m_group_count = 0;
	uint16_t m_data_count = 0;
};


