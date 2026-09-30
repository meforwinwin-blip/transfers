#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#define _DEBUG                    // Set debug mode, results in more verbose output on serial port
#include "Constants.h"

//#define DUMMYDATA       // Uncomment to enable transmission of fake random data for testing with no sensors needed

#define RR_USE_PREFERENCES

// -- Basic device configuration, see Constants.h
// Auto-select the plain Seeed XIAO nRF52840 when compiled with the Seeed nRF52 BSP.
// Existing ESP32 Feather builds remain the default for all other targets.
#if defined(ARDUINO_Seeed_XIAO_nRF52840)
  #define BOARD            BOARD_NRF52_XIAO
#else
  #define BOARD            BOARD_ESP32_FEATHER
#endif
#define DISP_DEVICE        DISP_NONE

// -- Distance Sensor related settings

#if BOARD == BOARD_NRF52_XIAO
  #define DIST_SENSOR      DIST_NONE
#else
  #define DIST_SENSOR      DIST_VL53L0X
#endif
#define DIST_SENSOR2       DIST_NONE // Device to use for second sensor on second I2C hardware bus (ESP32 only), see Constants.h

#define DISTANCEOFFSET 0         // Write distance to tire in mm here to get logged distance data value centered around zero
                                 // If you leave this value here at 0 the distance value in the logs will always be positive numbers

// -- Far Infrared Sensor related settings

#if BOARD == BOARD_NRF52_XIAO
  #define FIS_SENSOR       FIS_AMG8833     // XIAO tyre node default
#else
  #define FIS_SENSOR       FIS_MLX90640    // Device to use, see Constants.h
#endif    

#if BOARD == BOARD_ESP32_LOLIND32
  #define FIS_SENSOR2_PRESENT 0            // Set to 1 if second sensor on second I2C hardware bus is present (ESP32 only)
#else
  #define FIS_SENSOR2_PRESENT 0
#endif
#if BOARD == BOARD_NRF52_XIAO
  #define FIS_REFRESHRATE  10            // AMG8833 supports 10 FPS
#else
  #define FIS_REFRESHRATE  16            // Sets the FIS refresh rate in Hz
#endif

#if FIS_SENSOR == FIS_MLX90621
  #define IGNORE_TOP_ROWS    0     // Ignore this many rows from the top of the sensor
  #define IGNORE_BOTTOM_ROWS 0     // Ignore this many rows from the bottom of the sensor
#elif FIS_SENSOR == FIS_MLX90640
  #define IGNORE_TOP_ROWS    10
  #define IGNORE_BOTTOM_ROWS 10
#elif FIS_SENSOR == FIS_DUMMY
  #define IGNORE_TOP_ROWS    0
  #define IGNORE_BOTTOM_ROWS 0
#elif FIS_SENSOR == FIS_AMG8833
  #define IGNORE_TOP_ROWS    2
  #define IGNORE_BOTTOM_ROWS 2
#elif FIS_SENSOR == FIS_MLX90614
  #define IGNORE_TOP_ROWS    0
  #define IGNORE_BOTTOM_ROWS 0
#endif

#define COLUMN_AGGREGATE   COLUMN_AGGREGATE_MAX  // Set column aggregation algorhytm, see Constants.h

//#define FIS_AUTOZOOM          // Comment to disable autozooming
#define AUTOZOOM_MINIMUM_TIRE_WIDTH 16 // Minimum tire width for autozooming

#define TEMPSCALING        1.00  // Default = 1.00
#define TEMPOFFSET         0     // Default = 0      NOTE: in TENTHS of degrees Celsius --> TEMPOFFSET 10 --> 1 degree
                  
#define MIRRORTIRE        0      // 0 = default
                                 // 1 = Mirror the tire (A), making the outside edge temps the inside edge temps
#if FIS_SENSOR2_PRESENT == 1
  #define MIRRORTIRE2     0      // 0 = default
                                 // 1 = Mirror tire B, making the outside edge temps the inside edge temps
#endif
                                 // NOTE!!! THESE CAN BE OVERRIDDEN WITH HARDWARE CODING WITH A GPIO PIN
                          
// -- General settings

#define DEFAULT_WHEEL_POS "FR" // Fallback when no persisted wheel position is stored
#define DEVICENAMECODE 7      // DEFAULT is 7
                          // 7 = "RejsaRubber" + four last bytes from the bluetooth MAC address

                          // 0 = "RejsaRubberFL" + three last bytes from the bluetooth MAC address
                          // 1 = "RejsaRubberFR" + three last bytes from the bluetooth MAC address
                          // 2 = "RejsaRubberRL" + three last bytes from the bluetooth MAC address
                          // 3 = "RejsaRubberRR" + three last bytes from the bluetooth MAC address
                          // 5 = "RejsaRubberF" + one space + three last bytes from the bluetooth MAC address
                          // 6 = "RejsaRubberR" + one space + three last bytes from the bluetooth MAC address
                        
                          // NOTE!!! THIS CAN BE OVERRIDDEN WITH HARDWARE CODING WITH GPIO PINS

#define SERIAL_UPDATERATE  10    // Update rate for the serial monitor in seconds
#define BATTERY_UPDATERATE   60     // Update rate for the battery in seconds


// -- Board-specific settings

#if BOARD == BOARD_ESP32_LOLIND32
  #define MILLIVOLTFULLSCALE  3300
  #define STEPSFULLSCALE      4096
  #define BATRESISTORCOMP     2.000 // Compensation for a resistor voltage divider between battery and ADC input pin
  #define VBAT_PIN            35
  #define GPIOLEDDIST         -1 
  #define GPIOLEDTEMP         -1 
  #define GPIOSDA             21 // set I2C bus GPIO pins in case of ESP32 explicitly to specific pins (different board designs have the I2C pins assigned differently)
  #define GPIOSCL             22
  #define GPIOSDA2            33 // set second I2C bus GPIO pins in case of ESP32 explicitly to specific pins (different board designs have the I2C pins assigned differently)
  #define GPIOSCL2            32
  #define GPIODISTSENSORXSHUT 15  // GPIO pin: A+B = Distance Sensor Shutdown
  #define GPIOLEFT            12  // GPIO pin: A+B Axis orientation
  #define GPIOFRONT           14  // GPIO pin: A+B Axis 1(0)/Axis 2(1)
  #define GPIOCAR             27  // GPIO pin: A+B = Car(1)
  #define GPIOMIRR            13  // GPIO pin: Mirr A
  #define GPIOMIRR2           26  // GPIO pin: Mirr B
  #define GPIOUNUSEDA2        17  // GPIO pin: Unused A2 (NOTE: works only on non-Pro LOLIN D32, because pin is NC on Pro)
  #define GPIOUNUSEDB1        25  // GPIO pin: Unused B1
  #define BUTTON_PIN          38  // GPIO pin for boot button
#elif BOARD == BOARD_ESP32_FEATHER
  #define MILLIVOLTFULLSCALE  3300
  #define STEPSFULLSCALE      4096
  #define BATRESISTORCOMP     2.100 // Compensation for a resistor voltage divider between battery and ADC input pin
  #define VBAT_PIN            A13
  #define GPIOLEDDIST         -1
  #define GPIOLEDTEMP         -1
  #define GPIOSDA             SDA // set I2C bus GPIO pins to default pins
  #define GPIOSCL             SCL
  #define GPIOSDA2            -1 // second I2C bus only available with LOLIN D32-based PCB
  #define GPIOSCL2            -1
  #define GPIODISTSENSORXSHUT 12  // GPIO pin number
  #define GPIOCAR             28  // GPIO pin number
  #define GPIOFRONT           29  // GPIO pin number
  #define GPIOLEFT            13  // GPIO pin number
  #define GPIOMIRR            14  // GPIO pin number
  #define BUTTON_PIN          38  // GPIO pin for boot button
#elif BOARD == BOARD_NRF52_XIAO
  #define MILLIVOLTFULLSCALE  3600
  #define STEPSFULLSCALE      4096
  #define BATRESISTORCOMP     (1510.0 / 510.0) // 1M + 510k / 510k divider
  #define VBAT_PIN            PIN_VBAT
  #define GPIOLEDDIST         LED_RED
  #define GPIOLEDTEMP         LED_BLUE
  #define GPIOSDA             PIN_WIRE_SDA
  #define GPIOSCL             PIN_WIRE_SCL
  #define GPIOSDA2            -1
  #define GPIOSCL2            -1
  #define GPIODISTSENSORXSHUT -1
  #define GPIOCAR             -1
  #define GPIOFRONT           -1
  #define GPIOLEFT            -1
  #define GPIOMIRR            -1
  #define BUTTON_PIN          -1
#elif BOARD == BOARD_NRF52_FEATHER
  #define MILLIVOLTFULLSCALE  3600
  #define STEPSFULLSCALE      1024
  #define BATRESISTORCOMP     1.403 // Compensation for a resistor voltage divider between battery and ADC input pin
  #define VBAT_PIN            A7
  #define GPIOLEDDIST         LED_RED
  #define GPIOLEDTEMP         LED_BLUE
  #define GPIOSDA             PIN_WIRE_SDA // set I2C bus GPIO pins to default pins
  #define GPIOSCL             PIN_WIRE_SCL
  #define GPIOSDA2            -1 // second I2C bus only available for ESP32
  #define GPIOSCL2            -1
  #define GPIODISTSENSORXSHUT 12  // GPIO pin number
  #define GPIOCAR             28  // GPIO pin number
  #define GPIOFRONT           29  // GPIO pin number
  #define GPIOLEFT            13  // GPIO pin number
  #define GPIOMIRR            14  // GPIO pin number
  #define BUTTON_PIN          -1  // Not supported
#endif


// -- Dynamically calculated configuration values

#if FIS_SENSOR == FIS_MLX90621
  #define FIS_X           16  // Far Infrared Sensor columns
  #define FIS_Y            4  // Far Infrared Sensor rows
#elif FIS_SENSOR == FIS_MLX90640
  #define FIS_X           32
  #define FIS_Y           24
#elif FIS_SENSOR == FIS_DUMMY
  #define FIS_X           16
  #define FIS_Y            4
#elif FIS_SENSOR == FIS_AMG8833
  #define FIS_X            8
  #define FIS_Y            8
#elif FIS_SENSOR == FIS_MLX90614
  #define FIS_X            1
  #define FIS_Y            1
#endif

#define EFFECTIVE_ROWS ( FIS_Y - IGNORE_TOP_ROWS - IGNORE_BOTTOM_ROWS )

extern const char* settingsNamespace;

#endif /* CONFIGURATION_H */
