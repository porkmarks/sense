#include "Settings.h"
#include <avr/eeprom.h>
#include "Log.h"
#include "Util.h"

static constexpr uint8_t SETTINGS_VERSION = 7;

static uint8_t  EEMEM eeprom_settings_version;
static Sensor_Comms::Address EEMEM eeprom_settings_address;
static uint32_t EEMEM eeprom_settings_crc;

void reset_settings()
{
    if (eeprom_read_byte(&eeprom_settings_version) != 0)
    {
        eeprom_write_byte(&eeprom_settings_version, 0);
    }
}

void save_settings(Settings const& settings)
{
    LOG(PSTR("Save settings..."));
    uint32_t crc = 123456789ULL;
    eeprom_write_byte(&eeprom_settings_version, SETTINGS_VERSION);
    hash_combine(crc, SETTINGS_VERSION);

    eeprom_write_word(&eeprom_settings_address, settings.address);
    hash_combine(crc, settings.address);

    eeprom_write_dword(&eeprom_settings_crc, crc);
    LOG(PSTR("done\n"));
}

bool load_settings(Settings& settings)
{
    LOG(PSTR("Load settings..."));
    if (eeprom_read_byte(&eeprom_settings_version) == SETTINGS_VERSION)
    {
        uint32_t crc = 123456789ULL;
        hash_combine(crc, SETTINGS_VERSION);

        settings.address = eeprom_read_word(&eeprom_settings_address);
        hash_combine(crc, settings.address);

        uint32_t stored_crc = eeprom_read_dword(&eeprom_settings_crc);
        if (stored_crc != crc)
        {
            LOG(PSTR("failed: crc\n"));
            settings = Settings();
            reset_settings();
            return false;
        }
        LOG(PSTR("done\n"));
        return true;
    }
    else
    {
        LOG(PSTR("failed: old\n"));
        settings = Settings();
        reset_settings();
        return false;
    }
}

/////////////////////////////////////////////////////////

static constexpr uint8_t STABLE_SETTINGS_VERSION = 2;

enum eeprom_stable_settings : uint16_t
{
  version           = E2END - 64,
  serial_number     = version + 1,
  temperature_bias  = serial_number + 4,
  humidity_bias     = temperature_bias + 2,
  vref              = humidity_bias + 2,
  crc               = vref + 1
};

void reset_stable_settings()
{
    if (eeprom_read_byte((const uint8_t*)eeprom_stable_settings::version) != 0)
    {
        eeprom_write_byte((const uint8_t*)eeprom_stable_settings::version, 0);
    }
}

void save_stable_settings(Stable_Settings const& settings)
{
    LOG(PSTR("Save stable settings..."));
    uint32_t crc = 123456789ULL;
    eeprom_write_byte((const uint8_t*)eeprom_stable_settings::version, STABLE_SETTINGS_VERSION);
    hash_combine(crc, STABLE_SETTINGS_VERSION);

    eeprom_write_dword((uint32_t*)eeprom_stable_settings::serial_number, settings.serial_number);
    hash_combine(crc, settings.serial_number);

    eeprom_write_word((uint16_t*)eeprom_stable_settings::temperature_bias, *(uint16_t*)&settings.calibration.temperature_bias);
    hash_combine(crc, int32_t(settings.calibration.temperature_bias) + 65535ULL);

    eeprom_write_word((uint16_t*)eeprom_stable_settings::humidity_bias, *(uint16_t*)&settings.calibration.humidity_bias);
    hash_combine(crc, int32_t(settings.calibration.humidity_bias) + 65535ULL);

    eeprom_write_byte((const uint8_t*)eeprom_stable_settings::vref, settings.vref);
    hash_combine(crc, int32_t(settings.vref));

    eeprom_write_dword((uint32_t*)eeprom_stable_settings::crc, crc);
    LOG(PSTR("done\n"));
}

bool load_stable_settings(Stable_Settings& settings)
{
    LOG(PSTR("Load stable settings..."));
    if (eeprom_read_byte((const uint8_t*)eeprom_stable_settings::version) == STABLE_SETTINGS_VERSION)
    {
        uint32_t crc = 123456789ULL;
        hash_combine(crc, STABLE_SETTINGS_VERSION);

        settings.serial_number = eeprom_read_dword((const uint32_t*)eeprom_stable_settings::serial_number);
        hash_combine(crc, settings.serial_number);

        uint16_t v = eeprom_read_word((uint16_t*)eeprom_stable_settings::temperature_bias);
        settings.calibration.temperature_bias = *(int16_t*)&v;
        hash_combine(crc, int32_t(settings.calibration.temperature_bias) + 65535ULL);

        v = eeprom_read_word((uint16_t*)eeprom_stable_settings::humidity_bias);
        settings.calibration.humidity_bias = *(int16_t*)&v;
        hash_combine(crc, int32_t(settings.calibration.humidity_bias) + 65535ULL);

        settings.vref = eeprom_read_byte((const uint8_t*)eeprom_stable_settings::vref);
        hash_combine(crc, int32_t(settings.vref));

        uint32_t stored_crc = eeprom_read_dword((const uint32_t*)eeprom_stable_settings::crc);
        if (stored_crc != crc)
        {
            LOG(PSTR("failed: crc\n"));
            settings = Stable_Settings();
            reset_stable_settings();
            return false;
        }
        LOG(PSTR("done\n"));
        return true;
    }
    else
    {
        LOG(PSTR("failed: old\n"));
        settings = Stable_Settings();
        reset_stable_settings();
        return false;
    }
}
