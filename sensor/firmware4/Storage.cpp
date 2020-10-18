#include "Storage.h"
#include <math.h>
#include <assert.h>
#include "BackingStore.h"

///////////////////////////////////////////////////////////////////////////////

namespace detail
{
template<typename T> T min(T a, T b) { return a < b ? a : b; }
template<typename T> T max(T a, T b) { return a > b ? a : b; }
template<typename T> T clamp(T a, T min, T max) { return (a < min) ? min : ((a < max) ? a : max); }
}


static constexpr int32_t ABS_T_Q_BIT_COUNT = 15; //precision for 15 bits: 0.0061037018951994. Max error is half this.
static constexpr int32_t ABS_T_MAX_QVALUE = (1ULL << ABS_T_Q_BIT_COUNT) - 1;
static constexpr float ABS_T_MIN_VALUE = -100.f;
static constexpr float ABS_T_MAX_VALUE = 100.f;
static constexpr float ABS_T_SCALE = (ABS_T_MAX_QVALUE - 1) / (ABS_T_MAX_VALUE - ABS_T_MIN_VALUE); //we leave some slack because of rounding paranoia
static constexpr float ABS_T_INV_SCALE = 1.f / ABS_T_SCALE;

static constexpr size_t REL_T_FLAGS_BIT_COUNT = 2; //how many bits for the flags
static constexpr size_t REL_T_FLAGS_MAX_BIT_COUNT_VALUE = 2; //what is the max value that represents a bit count. This is multiplied by 4 to get the actual bit count
static constexpr int32_t REL_T_Q_BIT_COUNT = 10;
static_assert(REL_T_FLAGS_MAX_BIT_COUNT_VALUE * 4 < REL_T_Q_BIT_COUNT);
static constexpr int32_t REL_T_MAX_QVALUE = (1 << REL_T_Q_BIT_COUNT) - 1;
static constexpr float REL_T_MAX_VALUE1 = 10.f; //precision for 10 bits + sign: 0.0097751710654936. Max error is half this.
static constexpr float REL_T_SCALE1 = REL_T_MAX_QVALUE / REL_T_MAX_VALUE1;
static constexpr float REL_T_INV_SCALE1 = 1.f / REL_T_SCALE1;
static constexpr float REL_T_MAX_VALUE2 = 100.f; //precision for 10 bits + sign: 0.097703957010259. Max error is half this.
static constexpr float REL_T_SCALE2 = REL_T_MAX_QVALUE / REL_T_MAX_VALUE2;
static constexpr float REL_T_INV_SCALE2 = 1.f / REL_T_SCALE2;

static constexpr int32_t ABS_H_Q_BIT_COUNT = 14;
static constexpr int32_t ABS_H_MAX_QVALUE = (1 << ABS_H_Q_BIT_COUNT) - 1;
static constexpr float ABS_H_MIN_VALUE = 0.f;
static constexpr float ABS_H_MAX_VALUE = 100.f; //precision for 14 bits: 0.0061038881767686. Max error is half this.
static constexpr float ABS_H_SCALE = (ABS_H_MAX_QVALUE - 1) / (ABS_H_MAX_VALUE - ABS_H_MIN_VALUE); //we leave some slack because of rounding paranoia
static constexpr float ABS_H_INV_SCALE = 1.f / ABS_H_SCALE;

static constexpr size_t REL_H_FLAGS_BIT_COUNT = 2;
static constexpr size_t REL_H_FLAGS_MAX_BIT_COUNT_VALUE = 2;
static constexpr int32_t REL_H_Q_BIT_COUNT = 10;
static_assert(REL_H_FLAGS_MAX_BIT_COUNT_VALUE * 4 < REL_H_Q_BIT_COUNT);
static constexpr int32_t REL_H_MAX_QVALUE = (1 << REL_H_Q_BIT_COUNT) - 1;
static constexpr float REL_H_MAX_VALUE1 = 20.f; //precision for 10 bits + sign: 0.0195503421309873. Max error is half this.
static constexpr float REL_H_SCALE1 = REL_H_MAX_QVALUE / REL_H_MAX_VALUE1;
static constexpr float REL_H_INV_SCALE1 = 1.f / REL_H_SCALE1;
static constexpr float REL_H_MAX_VALUE2 = 100.f; //precision for 11 bits: 0.0488519785051295. Max error is half this.
static constexpr float REL_H_SCALE2 = REL_H_MAX_QVALUE / REL_H_MAX_VALUE2;
static constexpr float REL_H_INV_SCALE2 = 1.f / REL_H_SCALE2;

static uint16_t quantize(float value, float min_value, float scale, uint16_t max_qvalue)
{
	int32_t q = static_cast<int32_t>(lround((value - min_value) * scale));
	return static_cast<uint16_t>(detail::clamp<int32_t>(q, 0, max_qvalue));
}

static int16_t quantize_signed(float value, float scale, uint16_t max_qvalue)
{
	int32_t q = static_cast<int32_t>(lround(value * scale));
	return static_cast<int16_t>(detail::clamp<int32_t>(q, -max_qvalue, max_qvalue));
}

static float dequantize(uint16_t qvalue, float min_value, float inv_scale)
{
	return static_cast<float>(qvalue) * inv_scale + min_value;
}

static float dequantize_signed(int16_t qvalue, float inv_scale)
{
	return static_cast<float>(qvalue) * inv_scale;
}

bool Storage::Group::unpack_next(Storage::iterator& it) const
{
	if (!m_is_initialized)
		return false;

	if (it.bit_offset == uint16_t(-1))
	{
		it.data.temperature = dequantize(m_qtemperature, ABS_T_MIN_VALUE, ABS_T_INV_SCALE);
		it.data.humidity = dequantize(m_qhumidity, ABS_H_MIN_VALUE, ABS_H_INV_SCALE);
		it.bit_offset = 0;
		return true;
	}
	if (it.bit_offset >= m_bit_size)
		return false;

	Bitstream bs((uint8_t*)m_data, m_bit_size, false);

	//temperature
	{
		uint8_t flags = bs.read(it.bit_offset, REL_T_FLAGS_BIT_COUNT);
		if (flags <= REL_T_FLAGS_MAX_BIT_COUNT_VALUE)
		{
			uint8_t bit_count = REL_T_Q_BIT_COUNT - REL_T_FLAGS_MAX_BIT_COUNT_VALUE * 4 + flags * 4;
			uint32_t bits = bs.read(it.bit_offset, bit_count + 1); //+1 sign
			if (bits & (1 << bit_count)) //negative?
				bits |= 0xFFFFFFFF & (~((1 << bit_count) - 1));

			it.data.temperature += dequantize_signed(*(uint16_t*)&bits, REL_T_INV_SCALE1);
		}
		else
		{
			uint32_t bits = bs.read(it.bit_offset, REL_T_Q_BIT_COUNT + 1);
			if (bits & (1 << REL_T_Q_BIT_COUNT)) //negative?
				bits |= 0xFFFFFFFF & (~((1 << REL_T_Q_BIT_COUNT) - 1));

			it.data.temperature = dequantize_signed(*(uint16_t*)&bits, REL_T_INV_SCALE2);
		}
	}

	//humidity
	{
		uint8_t flags = bs.read(it.bit_offset, REL_H_FLAGS_BIT_COUNT);
		if (flags <= REL_H_FLAGS_MAX_BIT_COUNT_VALUE)
		{
			uint8_t bit_count = REL_H_Q_BIT_COUNT - REL_H_FLAGS_MAX_BIT_COUNT_VALUE * 4 + flags * 4;
			uint32_t bits = bs.read(it.bit_offset, bit_count + 1);
			if (bits & (1 << bit_count)) //negative?
				bits |= 0xFFFFFFFF & (~((1 << bit_count) - 1));

			it.data.humidity += dequantize_signed(*(uint16_t*)&bits, REL_H_INV_SCALE1);
		}
		else
		{
			uint32_t bits = bs.read(it.bit_offset, REL_H_Q_BIT_COUNT + 1);
			it.data.humidity = dequantize(*(uint16_t*)&bits, 0.f, REL_H_INV_SCALE2 * 0.5f);
		}
	}

	return true;
}

uint16_t Storage::Group::get_data_count() const
{
	uint16_t data_count = 0;
	Storage::iterator it;

	while (unpack_next(it))
		data_count++;

	return data_count;
}


Storage::Storage()
{
}

bool Storage::unpack_next(iterator& it) const
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

bool Storage::_unpack_next(iterator& it) const
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

		it.group_idx = (it.group_idx + 1) % (BackingStore::PAGE_COUNT + 1);
		it.bit_offset = -1;
		return unpack_next(it);
	}

	return true;
}

size_t Storage::get_data_count() const
{
	size_t data_count = 0;
	Storage::iterator it;

	while (unpack_next(it))
		data_count++;

	return data_count;
}

uint16_t Storage::get_group_count() const
{
	return m_group_count;
}

uint16_t Storage::get_first_group_idx() const
{
	return m_first_group_idx;
}

uint16_t Storage::get_last_group_idx() const
{
	return (m_first_group_idx + m_group_count - 1) % (BackingStore::PAGE_COUNT + 1);
}

// // Storage::Group& Storage::get_first_group()
// {
// 	return m_groups[get_first_group_idx()];
// }
// 
// // const Storage::Group& Storage::get_first_group() const
// {
// 	return m_groups[get_first_group_idx()];
// }
// 
// // Storage::Group& Storage::get_last_group()
// {
// 	return m_groups[get_last_group_idx()];
// }
// 
// // const Storage::Group& Storage::get_last_group() const
// {
// 	return m_groups[get_last_group_idx()];
// }

bool Storage::pop_first_group()
{
	if (m_group_count == 0)
		return false;

	m_first_group_idx = (m_first_group_idx + 1) % (BackingStore::PAGE_COUNT + 1);
	m_group_count--;
	return true;
}

bool Storage::add_group()
{
	if (m_group_count >= (BackingStore::PAGE_COUNT + 1))
		return false;

	if (m_group_count > 0)
	{
		if (!BackingStore::store(get_last_group_idx(), &m_last_group, sizeof(Group)))
			return false;
	}

	m_group_count++;
	Group& g = m_last_group;
	memset(&g, 0, sizeof(Group));
	return true;
}

bool Storage::load_group(uint16_t group_idx) const
{
	assert(group_idx < m_group_count);
	if (group_idx == m_iteration_group_idx) //already loaded
		return true;

	if (group_idx == get_last_group_idx())
		memcpy(&m_iteration_group, &m_last_group, sizeof(Group));
	else if (!BackingStore::load(group_idx, &m_iteration_group, sizeof(Group)))
		return false;

	m_iteration_group_idx = group_idx;
	return true;
}


void Storage::clear()
{
	m_first_group_idx = 0;
	m_group_count = 0;
	m_data_count = 0;

	memset(&m_last_group, 0, sizeof(Group));
	memset(&m_iteration_group, 0, sizeof(Group));
}

bool Storage::empty() const
{
	return m_group_count == 0;
}

bool Storage::pop_front()
{
	if (m_data_count == 0)
		return false;

// 	Group& g = get_first_group();
// 	g.skip_count++;
// 	if (g.skip_count >= g.get_data_count())
// 		pop_first_group();

	m_data_count--;

	return true;
}

bool Storage::push_back(const Data& data)
{
	if (m_group_count == 0 && !add_group())
		return false;

	assert(get_group_count() > 0);

	Group& g = m_last_group;
	if (g.m_is_initialized == 0)
	{
		g.m_is_initialized = 1;
		g.m_skip_count = 0;
		g.m_qtemperature = quantize(data.temperature, ABS_T_MIN_VALUE, ABS_T_SCALE, ABS_T_MAX_QVALUE);
		g.m_qhumidity = quantize(data.humidity, ABS_H_MIN_VALUE, ABS_H_SCALE, ABS_H_MAX_QVALUE);

		//unpack to set the last values AFTER the quantization
		m_last_data.temperature = dequantize(g.m_qtemperature, ABS_T_MIN_VALUE, ABS_T_INV_SCALE);
		m_last_data.humidity = dequantize(g.m_qhumidity, ABS_H_MIN_VALUE, ABS_H_INV_SCALE);
	}
	else
	{
		float new_temperature;
		float new_humidity;

		float dtemperature = data.temperature - m_last_data.temperature;
		float dhumidity = data.humidity - m_last_data.humidity;

		int16_t qtemperature;
		uint32_t tflags;
		uint8_t tbit_count;
		if (dtemperature >= -REL_T_MAX_VALUE1 && dtemperature <= REL_T_MAX_VALUE1)
		{
			qtemperature = quantize_signed(dtemperature, REL_T_SCALE1, REL_T_MAX_QVALUE);
			new_temperature = m_last_data.temperature + dequantize_signed(qtemperature, REL_T_INV_SCALE1);

			//dynamic bitrate
			uint16_t qabs = abs(qtemperature);
			uint8_t min_bit_count = REL_T_Q_BIT_COUNT - REL_T_FLAGS_MAX_BIT_COUNT_VALUE * 4;
			for (tbit_count = min_bit_count; tbit_count <= REL_T_Q_BIT_COUNT; tbit_count += 4)
				if (qabs < 1 << tbit_count)
					break;

			tflags = (tbit_count - min_bit_count) >> 2;
			assert(tflags <= REL_T_FLAGS_MAX_BIT_COUNT_VALUE);
			tbit_count++; //sign
		}
		else
		{
			qtemperature = quantize_signed(data.temperature, REL_T_SCALE2, REL_T_MAX_QVALUE);
			new_temperature = dequantize_signed(qtemperature, REL_T_INV_SCALE2);
			tflags = REL_T_FLAGS_MAX_BIT_COUNT_VALUE + 1; //mark fixed bitrate
			tbit_count = REL_T_Q_BIT_COUNT + 1; //max bitrate + sign
		}

		uint32_t hflags;
		uint8_t hbit_count;
		int16_t qhumidity;
		if (dhumidity >= -REL_H_MAX_VALUE1 && dhumidity <= REL_H_MAX_VALUE1)
		{
			qhumidity = quantize_signed(dhumidity, REL_H_SCALE1, REL_H_MAX_QVALUE);
			new_humidity = m_last_data.humidity + dequantize_signed(qhumidity, REL_H_INV_SCALE1);

			//dynamic bitrate
			uint16_t qabs = abs(qhumidity);
			uint8_t min_bit_count = REL_H_Q_BIT_COUNT - REL_H_FLAGS_MAX_BIT_COUNT_VALUE * 4;
			for (hbit_count = min_bit_count; hbit_count <= REL_H_Q_BIT_COUNT; hbit_count += 4)
				if (qabs < 1 << hbit_count)
					break;

			hflags = (hbit_count - min_bit_count) >> 2;
			assert(hflags <= REL_H_FLAGS_MAX_BIT_COUNT_VALUE);
			hbit_count++; //sign
		}
		else
		{
			qhumidity = quantize(data.humidity, 0.f, REL_H_SCALE2 * 2.0f, REL_H_MAX_QVALUE << 1); //use the bit sign for double range
			new_humidity = dequantize(qhumidity, 0.f, REL_H_INV_SCALE2 * 0.5f);
			hflags = REL_H_FLAGS_MAX_BIT_COUNT_VALUE + 1; //mark fixed bitrate
			hbit_count = REL_H_Q_BIT_COUNT + 1; //max bitrate + 1 for double range, since there is no bit sign
		}

		assert(tbit_count <= REL_T_Q_BIT_COUNT + 1);
		assert(hbit_count <= REL_H_Q_BIT_COUNT + 1);

		if (g.m_bit_size + REL_T_FLAGS_BIT_COUNT + REL_H_FLAGS_BIT_COUNT + tbit_count + hbit_count > Group::DATA_SIZE * 8)
		{
			if (!add_group())
				return false;
			return push_back(data); //retry, with the new group
		}

		Bitstream bs(g.m_data, g.m_bit_size, false);
		bs.write(tflags, REL_T_FLAGS_BIT_COUNT);
		bs.write(qtemperature, tbit_count);
		bs.write(hflags, REL_H_FLAGS_BIT_COUNT);
		bs.write(qhumidity, hbit_count);
		g.m_bit_size = bs.size();

		m_last_data.temperature = new_temperature;
		m_last_data.humidity = new_humidity;

		m_data_count++;
	}

	return true;
}
