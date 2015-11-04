#pragma once


namespace storage
{

bool push_back(float temperature, float humidity, float vcc);

size_t get_count();
bool front(float& temperature, float& humidity, float& vcc);
bool pop_front();

void clear();

}

