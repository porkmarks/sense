#pragma once

#include <inttypes.h>
#include <stddef.h>
#include "Bitstream.h"

template<typename T>
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

		void quantize(const Data& data, int16_t& t, int16_t& h);
		Data dequantize(int16_t t, int16_t h) const;

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


///////////////////////////////////////////////////////////////////////////////

namespace detail
{
template<typename T> T min(T a, T b) { return a < b ? a : b; }
template<typename T> T max(T a, T b) { return a > b ? a : b; }
template<typename T> T clamp(T a, T min, T max) { return (a < min) ? min : ((a < max) ? a : max); }
}



//all data is signed
static constexpr size_t STORAGE_TEMPERATURE_FLAGS_BIT_COUNT = 3; //how many bits for the flags
static constexpr size_t STORAGE_TEMPERATURE_FLAGS_MAX_BIT_COUNT_VALUE = 6; //what is the max value that represents a bit count. This is multiplied by 2 to get the actual bit count
static constexpr int32_t STORAGE_TEMPERATURE_DATA_BIT_COUNT = 13;
static_assert(STORAGE_TEMPERATURE_FLAGS_MAX_BIT_COUNT_VALUE * 2 < STORAGE_TEMPERATURE_DATA_BIT_COUNT);
static constexpr int32_t STORAGE_TEMPERATURE_DATA_MAX_VALUE = (1 << STORAGE_TEMPERATURE_DATA_BIT_COUNT) - 1;
static constexpr float STORAGE_MAX_TEMPERATURE = 100.f;
static constexpr float STORAGE_TEMPERATURE_SCALE = (STORAGE_TEMPERATURE_DATA_MAX_VALUE - 10) / STORAGE_MAX_TEMPERATURE; //we leave some slack because of rounding paranoia
static constexpr float STORAGE_TEMPERATURE_INV_SCALE = 1.f / STORAGE_TEMPERATURE_SCALE;

static constexpr size_t STORAGE_HUMIDITY_FLAGS_BIT_COUNT = 3;
static constexpr size_t STORAGE_HUMIDITY_FLAGS_MAX_BIT_COUNT_VALUE = 6;
static constexpr int32_t STORAGE_HUMIDITY_DATA_BIT_COUNT = 13;
static_assert(STORAGE_HUMIDITY_FLAGS_MAX_BIT_COUNT_VALUE * 2 < STORAGE_HUMIDITY_DATA_BIT_COUNT);
static constexpr int32_t STORAGE_HUMIDITY_DATA_MAX_VALUE = (1 << STORAGE_HUMIDITY_DATA_BIT_COUNT) - 1;
static constexpr float STORAGE_MAX_HUMIDITY = 100.f;
static constexpr float STORAGE_HUMIDITY_SCALE = (STORAGE_HUMIDITY_DATA_MAX_VALUE - 10) / STORAGE_MAX_HUMIDITY; //we leave some slack because of rounding paranoia
static constexpr float STORAGE_HUMIDITY_INV_SCALE = 1.f / STORAGE_HUMIDITY_SCALE;

template<typename T>
void Storage<T>::Group::quantize(const Data& data, int16_t& t, int16_t& h)
{
	int32_t it = static_cast<int32_t>(data.temperature * STORAGE_TEMPERATURE_SCALE + 0.5f);
	t = static_cast<int16_t>(detail::clamp(it, -STORAGE_TEMPERATURE_DATA_MAX_VALUE, STORAGE_TEMPERATURE_DATA_MAX_VALUE));

	int32_t ih = static_cast<int32_t>(data.humidity * STORAGE_HUMIDITY_SCALE + 0.5f);
	h = static_cast<uint16_t>(detail::clamp(ih, -STORAGE_HUMIDITY_DATA_MAX_VALUE, STORAGE_HUMIDITY_DATA_MAX_VALUE));
}

template<typename T>
typename Storage<T>::Data Storage<T>::Group::dequantize(int16_t t, int16_t h) const
{
	Data data;
	data.temperature = static_cast<float>(t) * STORAGE_TEMPERATURE_INV_SCALE;
	data.humidity = static_cast<float>(h) * STORAGE_HUMIDITY_INV_SCALE;
	return data;
}

template<typename T>
bool Storage<T>::Group::unpack_next(Storage<T>::iterator& it) const
{
	if (!m_is_initialized)
		return false;

	if (it.bit_offset == uint16_t(-1))
	{
		it.data = dequantize(m_qtemperature, m_qhumidity);
		it.bit_offset = 0;
		return true;
	}
	if (it.bit_offset >= m_bit_size)
		return false;

	Bitstream bs((uint8_t*)m_data, m_bit_size, false);

	//temperature
	int16_t qtemperature;
	{
		uint8_t flags = bs.read(it.bit_offset, STORAGE_TEMPERATURE_FLAGS_BIT_COUNT);
		assert(flags <= STORAGE_TEMPERATURE_FLAGS_MAX_BIT_COUNT_VALUE);
		uint8_t bit_count = STORAGE_TEMPERATURE_DATA_BIT_COUNT - STORAGE_TEMPERATURE_FLAGS_MAX_BIT_COUNT_VALUE * 2 + flags * 2 + 1; //+1 bit sign
		uint32_t bits = bs.read(it.bit_offset, bit_count);
		qtemperature = *(int16_t*)&bits;
	}

	//humidity
	int16_t qhumidity;
	{
		uint8_t flags = bs.read(it.bit_offset, STORAGE_HUMIDITY_FLAGS_BIT_COUNT);
		assert(flags <= STORAGE_HUMIDITY_FLAGS_MAX_BIT_COUNT_VALUE);
		uint8_t bit_count = STORAGE_HUMIDITY_DATA_BIT_COUNT - STORAGE_HUMIDITY_FLAGS_MAX_BIT_COUNT_VALUE * 2 + flags * 2 + 1; //+1 bit sign
		uint32_t bits = bs.read(it.bit_offset, bit_count);
		qhumidity = *(int16_t*)&bits;
	}

	it.data = dequantize(qtemperature, qhumidity);
	return true;
}

template<typename T>
uint16_t Storage<T>::Group::get_data_count() const
{
	uint16_t data_count = 0;
	Storage<T>::iterator it;

	while (unpack_next(it))
		data_count++;

	return data_count;
}


template<typename T>
Storage<T>::Storage()
{
}

template<typename T>
bool Storage<T>::unpack_next(iterator& it) const
{
	if (m_group_count == 0)
		return false;

	if (it.group_idx < 0)
	{
		if (!load_group(0))
			return false;

		//skip the popped elements in the beginning and warm up the data
		const Group& g = m_iteration_group;
		for (uint8_t i = 0; i < g.m_skip_count; i++)
		{
			bool ok = _unpack_next(it);
			assert(ok);
		}
	}

	return _unpack_next(it);
}

template<typename T>
bool Storage<T>::_unpack_next(iterator& it) const
{
	if (m_group_count == 0)
		return false;

	if (it.group_idx < 0)
	{
		it.group_idx = m_first_group_idx;
		it.bit_offset = -1;
	}

	if (it.group_idx != m_iteration_group_idx)
	{
		if (!load_group(it.group_idx))
			return false;
	}

	const Group& g = m_iteration_group;
	if (!g.unpack_next(it))
	{
		if (it.group_idx == get_last_group_idx())
			return false;

		it.group_idx = (it.group_idx + 1) % (T::GROUP_COUNT + 1);
		it.bit_offset = -1;
		return unpack_next(it);
	}

	return true;
}

template<typename T>
size_t Storage<T>::get_data_count() const
{
	size_t data_count = 0;
	Storage<T>::iterator it;

	while (unpack_next(it))
		data_count++;

	return data_count;
}

template<typename T>
uint16_t Storage<T>::get_group_count() const
{
	return m_group_count;
}

template<typename T>
uint16_t Storage<T>::get_first_group_idx() const
{
	return m_first_group_idx;
}

template<typename T>
uint16_t Storage<T>::get_last_group_idx() const
{
	return (m_first_group_idx + m_group_count - 1) % (T::GROUP_COUNT + 1);
}

// template<typename T>
// Storage<T>::Group& Storage<T>::get_first_group()
// {
// 	return m_groups[get_first_group_idx()];
// }
// 
// template<typename T>
// const Storage<T>::Group& Storage<T>::get_first_group() const
// {
// 	return m_groups[get_first_group_idx()];
// }
// 
// template<typename T>
// Storage<T>::Group& Storage<T>::get_last_group()
// {
// 	return m_groups[get_last_group_idx()];
// }
// 
// template<typename T>
// const Storage<T>::Group& Storage<T>::get_last_group() const
// {
// 	return m_groups[get_last_group_idx()];
// }

template<typename T>
bool Storage<T>::pop_first_group()
{
	if (m_group_count == 0)
		return false;

	m_first_group_idx = (m_first_group_idx + 1) % (T::GROUP_COUNT + 1);
	m_group_count--;
	return true;
}

template<typename T>
bool Storage<T>::add_group()
{
	if (m_group_count >= (T::GROUP_COUNT + 1))
		return false;

	if (m_group_count > 0)
	{
		if (!T::store(get_last_group_idx(), &m_last_group, sizeof(Group)))
			return false;
	}

	m_group_count++;
	Group& g = m_last_group;
	memset(&g, 0, sizeof(Group));
	return true;
}

template<typename T>
bool Storage<T>::load_group(uint16_t group_idx) const
{
	assert(group_idx < m_group_count);
	if (group_idx == m_iteration_group_idx) //already loaded
		return true;

	if (group_idx == get_last_group_idx())
		memcpy(&m_iteration_group, &m_last_group, sizeof(Group));
	else if (!T::load(group_idx, &m_iteration_group, sizeof(Group)))
		return false;

	m_iteration_group_idx = group_idx;
	return true;
}


template<typename T>
void Storage<T>::clear()
{
	m_first_group_idx = 0;
	m_group_count = 0;
	m_data_count = 0;

	memset(m_last_group, 0, sizeof(Group));
	memset(m_iteration_group, 0, sizeof(Group));
}

template<typename T>
bool Storage<T>::empty() const
{
	return m_group_count == 0;
}

template<typename T>
bool Storage<T>::pop_front()
{
	if (m_data_count == 0)
		return false;

	Group& g = get_first_group();
	g.skip_count++;
	if (g.skip_count >= g.get_data_count())
		pop_first_group();

	m_data_count--;

	return true;
}

template<typename T>
bool Storage<T>::push_back(const Data& data)
{
	if (m_group_count == 0 && !add_group())
		return false;

	assert(get_group_count() > 0);

	Group& g = m_last_group;
	if (g.m_is_initialized == 0)
	{
		g.m_is_initialized = 1;
		g.m_skip_count = 0;
		g.quantize(data, g.m_qtemperature, g.m_qhumidity);

		//unpack to set the last values AFTER the quantization
		m_last_data = g.dequantize(g.m_qtemperature, g.m_qhumidity);
	}
	else
	{
		float dtemperature = data.temperature - m_last_data.temperature;
		float dhumidity = data.humidity - m_last_data.humidity;

		int16_t qdtemperature;
		int16_t qdhumidity;
		g.quantize(Data{dtemperature, dhumidity}, qdtemperature, qdhumidity);

		uint32_t tflags;
		uint8_t tbit_count;

		//temperature
		{
			uint8_t min_bit_count = STORAGE_TEMPERATURE_DATA_BIT_COUNT - STORAGE_TEMPERATURE_FLAGS_MAX_BIT_COUNT_VALUE * 2;
			int16_t qdabs = abs(qdtemperature);
			for (tbit_count = min_bit_count; tbit_count <= STORAGE_TEMPERATURE_DATA_BIT_COUNT; tbit_count += 2)
				if (qdabs < 1 << tbit_count)
					break;

			tflags = (tbit_count - min_bit_count) >> 1;
			assert(tflags <= STORAGE_TEMPERATURE_FLAGS_MAX_BIT_COUNT_VALUE);
			tbit_count++; //sign bit
		}
		assert(tbit_count <= STORAGE_TEMPERATURE_DATA_BIT_COUNT + 1);

		uint32_t hflags;
		uint8_t hbit_count;

		//humidity
		{
			uint8_t min_bit_count = STORAGE_HUMIDITY_DATA_BIT_COUNT - STORAGE_HUMIDITY_FLAGS_MAX_BIT_COUNT_VALUE * 2;
			int16_t qdabs = abs(qdhumidity);
			for (hbit_count = min_bit_count; hbit_count <= STORAGE_HUMIDITY_DATA_BIT_COUNT; hbit_count += 2)
				if (qdabs < 1 << hbit_count)
					break;

			hflags = (hbit_count - min_bit_count) >> 1;
			assert(hflags <= STORAGE_HUMIDITY_FLAGS_MAX_BIT_COUNT_VALUE);
			hbit_count++; //sign bit
		}
		assert(hbit_count <= STORAGE_HUMIDITY_DATA_BIT_COUNT + 1);

		if (g.m_bit_size + STORAGE_TEMPERATURE_FLAGS_BIT_COUNT + STORAGE_HUMIDITY_FLAGS_BIT_COUNT + tbit_count + hbit_count > Group::DATA_SIZE * 8)
		{
			if (!add_group())
				return false;
			return push_back(data); //retry, with the new group
		}

		Bitstream bs(g.m_data, g.m_bit_size, false);
		bool ok = bs.write(tflags, STORAGE_TEMPERATURE_FLAGS_BIT_COUNT);
		ok &= bs.write(qdtemperature, tbit_count);
		ok &= bs.write(hflags, STORAGE_HUMIDITY_FLAGS_BIT_COUNT);
		ok &= bs.write(qdhumidity, hbit_count);
		assert(ok);
		g.m_bit_size = bs.size();

		m_last_data = g.dequantize(qdtemperature, qdhumidity);
		m_data_count++;
	}

	return true;
}
