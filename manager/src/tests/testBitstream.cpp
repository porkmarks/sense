#include "cstdio"
#include "Logger.h"
#include <iostream>
#include "testUtils.h"
#include <set>

#define BITSTREAM_CAPACITY 1024 * 8
#include "ex/Bitstream.h"

void testBitstream()
{
	std::cout << "Testing Bitstream\n";


	srand(11);

	uint8_t data[Bitstream::CAPACITY / 8];
	std::vector<uint32_t> values;
	for (volatile size_t t = 0; t < 10000; t++)
	{
		uint16_t size = 0;
		{
			Bitstream bs(data, 0, true);
			for (volatile uint8_t i = 0; i < 32; i++)
			{
				uint32_t value = rand() % (1 << i);
				CHECK_TRUE(bs.write(value, i));
				size += i;
				CHECK_EQUALS(bs.size(), size);
				values.push_back(value);
			}
			for (volatile uint8_t i = 31; i > 0; i--)
			{
				uint32_t value = rand() % (1 << i);
				CHECK_TRUE(bs.write(value, i));
				size += i;
				CHECK_EQUALS(bs.size(), size);
				values.push_back(value);
			}

			//check reading
			size_t valueIndex = 0;
			uint16_t pos = 0;
			for (volatile uint8_t i = 0; i < 32; i++)
			{
				uint16_t oldPos = pos;
				uint32_t value = bs.read(pos, i);
				CHECK_EQUALS(value, values[valueIndex++]);
				CHECK_EQUALS(pos, oldPos + i);
			}
			for (volatile uint8_t i = 31; i > 0; i--)
			{
				uint16_t oldPos = pos;
				uint32_t value = bs.read(pos, i);
				CHECK_EQUALS(value, values[valueIndex++]);
				CHECK_EQUALS(pos, oldPos + i);
			}
		}

		{
			//check reading from another stream
			Bitstream bs(data, size, true);
			size_t valueIndex = 0;
			uint16_t pos = 0;
			for (volatile uint8_t i = 0; i < 32; i++)
			{
				uint16_t oldPos = pos;
				uint32_t value = bs.read(pos, i);
				CHECK_EQUALS(value, values[valueIndex++]);
				CHECK_EQUALS(pos, oldPos + i);
			}
			for (volatile uint8_t i = 31; i > 0; i--)
			{
				uint16_t oldPos = pos;
				uint32_t value = bs.read(pos, i);
				CHECK_EQUALS(value, values[valueIndex++]);
				CHECK_EQUALS(pos, oldPos + i);
			}
			bs.clear();
			CHECK_EQUALS(bs.size(), 0);
			uint32_t total = 0;
			for (size_t i = 0; i < Bitstream::CAPACITY / 8; i++)
				total += data[i];
			CHECK_EQUALS(total, 0);
		}

		values.clear();
	}
}

