#include "cstdio"

void testBitstream();
void testStorage();
void testGeneralSettings();
void testCsvSettings();
void testSensorSettings();
void testSensorTimeConfig();
void testSensorBasicOperations();

int main(int, const char*[])
{
	testBitstream();
	testStorage();
    testGeneralSettings();
    testCsvSettings();
    testSensorSettings();
    testSensorTimeConfig();
    testSensorBasicOperations();

    return 0;
}
