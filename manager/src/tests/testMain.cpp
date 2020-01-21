#include "cstdio"

void testGeneralSettings();
void testCsvSettings();
void testSensorSettings();
void testSensorTimeConfig();
void testSensorBasicOperations();

int main(int, const char*[])
{
    testGeneralSettings();
    testCsvSettings();
    testSensorSettings();
    testSensorTimeConfig();
    testSensorBasicOperations();

    return 0;
}
