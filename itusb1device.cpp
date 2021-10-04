/* ITUSB1 device class - Version 1.0.0
   Requires CP2130 class version 1.0.0 or later
   Copyright (c) 2020-2021 Samuel Lourenço

   This library is free software: you can redistribute it and/or modify it
   under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation, either version 3 of the License, or (at your
   option) any later version.

   This library is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
   License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this library.  If not, see <https://www.gnu.org/licenses/>.


   Please feel free to contact me via e-mail: samuel.fmlourenco@gmail.com */


// Includes
#include <unistd.h>
#include <vector>
#include "itusb1device.h"

// Definitions
const size_t N_SAMPLES = 5;  // Number of samples per measurement, applicable to getCurrent()

// Private convenience function that is used to get the raw current measurement reading from the LTC2312 ADC
uint16_t ITUSB1Device::getRawCurrent(int &errcnt, std::string &errstr)
{
    std::vector<uint8_t> read = cp2130_.spiRead(2, 0x82, 0x01, errcnt, errstr);
    return read.size() == 2 ? static_cast<uint16_t>(read[0] << 4 | read[1] >> 4) : 0;  // It is important to check if the size of the returned vector matches the number of expected bytes - If not, return zero!
}

ITUSB1Device::ITUSB1Device() :
    cp2130_()
{
}

// Diagnostic function used to verify if the device has been disconnected
bool ITUSB1Device::disconnected() const
{
    return cp2130_.disconnected();
}

// Checks if the device is open
bool ITUSB1Device::isOpen() const
{
    return cp2130_.isOpen();
}

// Attaches the DUT (device under test) to the HUT (host under test)
void ITUSB1Device::attach(int &errcnt, std::string &errstr)
{
    if (getUSBPowerStatus(errcnt, errstr) != getUSBDataStatus(errcnt, errstr)) {  // If true, this condition indicates an unusual state
        switchUSB(false, errcnt, errstr);  // Switch VBUS off and disconnect the data lines
        usleep(100000);  // Wait 100ms to allow for device shutdown
    }
    if (!getUSBPowerStatus(errcnt, errstr) && !getUSBDataStatus(errcnt, errstr)) {  // If both VBUS and data lines are disconnected
        switchUSBPower(true, errcnt, errstr);  // Switch VBUS on
        usleep(100000);  // Wait 100ms in order to emulate a manual attachment of the device
        switchUSBData(true, errcnt, errstr);  // Connect the data lines
        usleep(100000);  // Wait 100ms so that device enumeration process can, at least, start (this is not enough to guarantee enumeration, though)
    }
}

// Closes the device safely, if open
void ITUSB1Device::close()
{
    cp2130_.close();
}

// Detaches the DUT (device under test) to the HUT (host under test)
void ITUSB1Device::detach(int &errcnt, std::string &errstr)
{
    if (getUSBDataStatus(errcnt, errstr)) {  // If the data lines are connected
        switchUSBData(false, errcnt, errstr);  // Disconnect the data lines
        usleep(100000);  // Wait 100ms in order to emulate a manual detachment of the device
    }
    if (getUSBPowerStatus(errcnt, errstr)) {  // If VBUS is switched on
        switchUSBPower(false, errcnt, errstr);  // Switch VBUS off
        usleep(100000);  // Wait 100ms to allow for device shutdown
    }
}

// Gets the VBUS current
// Important: SPI mode should be configured for channel 0, before using this function!
float ITUSB1Device::getCurrent(int &errcnt, std::string &errstr)
{
    cp2130_.selectCS(0, errcnt, errstr);  // Enable the chip select corresponding to channel 0, and disable any others
    getRawCurrent(errcnt, errstr);  // Discard this reading, as it will reflect a past measurement
    size_t currentCodeSum = 0;
    for (size_t i = 0; i < N_SAMPLES; ++i) {
        currentCodeSum += getRawCurrent(errcnt, errstr);  // Read the raw value (from the LTC2312 on channel 0) and add it to the sum
    }
    usleep(100);  // Wait 100us, in order to prevent possible errors while disabling the chip select (workaround)
    cp2130_.disableCS(0, errcnt, errstr);  // Disable the previously enabled chip select
    return currentCodeSum / (4.0 * N_SAMPLES);  // Return the average current out of "N_SAMPLES" [5] for each measurement (currentCode / 4.0 for a single reading)
}

// Gets the manufacturer descriptor from the device
std::u16string ITUSB1Device::getManufacturerDesc(int &errcnt, std::string &errstr)
{
    return cp2130_.getManufacturerDesc(errcnt, errstr);
}

// Gets OC flag
bool ITUSB1Device::getOvercurrentStatus(int &errcnt, std::string &errstr)
{
    return !cp2130_.getGPIO3(errcnt, errstr);  // Return the current state of the negated !UDOC signal
}

// Gets the product descriptor from the device
std::u16string ITUSB1Device::getProductDesc(int &errcnt, std::string &errstr)
{
    return cp2130_.getProductDesc(errcnt, errstr);
}

// Gets the serial descriptor from the device
std::u16string ITUSB1Device::getSerialDesc(int &errcnt, std::string &errstr)
{
    return cp2130_.getSerialDesc(errcnt, errstr);
}

// Gets the USB configuration of the device
CP2130::USBConfig ITUSB1Device::getUSBConfig(int &errcnt, std::string &errstr)
{
    return cp2130_.getUSBConfig(errcnt, errstr);
}

// Gets the status of the data lines
bool ITUSB1Device::getUSBDataStatus(int &errcnt, std::string &errstr)
{
    return !cp2130_.getGPIO2(errcnt, errstr);  // Return the current state of the negated !UDEN signal
}

// Gets the status of VBUS
bool ITUSB1Device::getUSBPowerStatus(int &errcnt, std::string &errstr)
{
    return !cp2130_.getGPIO1(errcnt, errstr);  // Return the current state of the negated !UPEN signal
}

// Opens the device having the given serial number, and assigns its handle
int ITUSB1Device::open(const std::string &serial)
{
    return cp2130_.open(VID, PID, serial);
}

// Issues a reset to the CP2130, which in effect resets the entire device
void ITUSB1Device::reset(int &errcnt, std::string &errstr)
{
    cp2130_.reset(errcnt, errstr);
}

// Sets up and prepares the device
void ITUSB1Device::setup(int &errcnt, std::string &errstr)
{
    CP2130::SPIMode mode;
    mode.csmode = CP2130::CSMODEPP;  // Chip select pin mode regarding channel 0 is push-pull
    mode.cfrq = CP2130::CFRQ1500K;  // SPI clock frequency set to 1.5MHz
    mode.cpol = CP2130::CPOL0;  // SPI clock polarity is active high (CPOL = 0)
    mode.cpha = CP2130::CPHA0;  // SPI data is valid on each rising edge (CPHA = 0)
    cp2130_.configureSPIMode(0, mode, errcnt, errstr);  // Configure SPI mode for channel 0, using the above settings
    cp2130_.disableSPIDelays(0, errcnt, errstr);  // Disable all SPI delays for channel 0
    cp2130_.selectCS(0, errcnt, errstr);  // Enable the chip select corresponding to channel 0, and disable any others
    getRawCurrent(errcnt, errstr);  // Discard this first reading - This also wakes up the LTC2312, if in nap or sleep mode!
    usleep(1100);  // Wait 1.1ms to ensure that the LTC2312 is awake, and also to prevent possible errors while disabling the chip select (workaround)
    cp2130_.disableCS(0, errcnt, errstr);  // Disable the previously enabled chip select
}

// Switches both VBUS and the data lines on or off
void ITUSB1Device::switchUSB(bool value, int &errcnt, std::string &errstr)
{
    cp2130_.setGPIOs(CP2130::BMGPIOS * !value, CP2130::BMGPIO1 | CP2130::BMGPIO2 , errcnt, errstr);  // This operates GPIO.1 and GPIO.2 simultaneously
}

// Switches the USB data lines on or off
void ITUSB1Device::switchUSBData(bool value, int &errcnt, std::string &errstr)
{
    cp2130_.setGPIO2(!value, errcnt, errstr);  // GPIO.2 corresponds to the !UDEN signal
}

// Switches VBUS on or off
void ITUSB1Device::switchUSBPower(bool value, int &errcnt, std::string &errstr)
{
    cp2130_.setGPIO1(!value, errcnt, errstr);  // GPIO.1 corresponds to the !UPEN signal
}

// Helper function to list devices
std::list<std::string> ITUSB1Device::listDevices(int &errcnt, std::string &errstr)
{
    return CP2130::listDevices(VID, PID, errcnt, errstr);
}
