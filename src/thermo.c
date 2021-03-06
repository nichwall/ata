#include "thermo.h"
#include <stdio.h>
#include <stdint.h>
#include <math.h>

// Color coding
#define BLKFG "\x1B[30m"
#define REDBG "\x1B[41m"
#define GRNBG "\x1B[42m"
#define RESET "\x1B[0m"

int8_t setup_thermo_daq() {
    uint8_t address;
    uint8_t channel;
    uint8_t tc_type = TC_TYPE_T;    // change this to desired thermocouple typehow to print the last 10 characters of a string in C

    int result = RESULT_SUCCESS;

    for(int i = THERMO_MIN_ADDR; i < THERMO_MAX_ADDR; i++)
    {
        address = (uint8_t)i;
        result  = mcc134_open(address);
        if (result != RESULT_SUCCESS)
            return result;
        for (channel = THERMO_LOW_CHANNEL; channel <= THERMO_HIGH_CHANNEL; channel++)
        {
            result = mcc134_tc_type_write(address, channel, tc_type);
            if (result != RESULT_SUCCESS)
                return result;
        }
    }
    return result;
}

/*
 * Function: get_thermo
 * 
 * Function which iterates over all MCC 134 boards (thermo)
 * and writes the line to a file.
 * 
 * fp: file pointer for logging data
 * 
 * returns: error condition (0 is no error)
 */
int8_t get_thermo(FILE *fp, uint8_t print) {
    // Static Variable declaration
    static double   bins        [THERMO_ADDRS][THERMO_CHANNELS][MAX_BIN_SIZE];
    static uint32_t bin_index   [THERMO_ADDRS][THERMO_CHANNELS];
    static uint32_t can_deviate [THERMO_ADDRS][THERMO_CHANNELS];

    // Variables
    uint8_t address;
    uint8_t channel;
    double value;
    double valueF;

    uint8_t result;
    double deviation = 1.0;

    // Iterate over the boards
    for (address = 0; address < THERMO_ADDRS; ++address) {
        result = mcc134_open(address + THERMO_MIN_ADDR);
        // Check for error
        if (result != RESULT_SUCCESS) {
            return result;
        }

        // Iterate over each channel
        for (channel = THERMO_LOW_CHANNEL; channel <= THERMO_HIGH_CHANNEL; channel++) {
            result = mcc134_t_in_read(address + THERMO_MIN_ADDR, channel, &value);
            if (result != RESULT_SUCCESS) {
                return result;
            }

            valueF = value*1.8 + 32.0; // Convert to Fahrenheit
            if (value == OPEN_TC_VALUE) {
                fprintf(fp,"Open,");
            } else if (value == OVERRANGE_TC_VALUE) {
                fprintf(fp,"OverRange,");
            } else if (value == COMMON_MODE_TC_VALUE) {
                fprintf(fp,"Common Mode,");
            } else {
                fprintf(fp, "%3.2f,", valueF);
            }

            // Check for Steady State or Transient value

            // Get current index in circular buffer
            uint32_t j = bin_index[address][channel];
            // Update circular buffer
            bins[address][channel][j] = valueF;
            bin_index[address][channel]++;

            // If we have filled the buffer, it can then "deviate"
            if (bin_index[address][channel] == MAX_BIN_SIZE) {
                bin_index  [address][channel] = 0;
                can_deviate[address][channel] = 1;
            }

            // Determine whether Steady State (SS) or Transient (T)
            if (can_deviate[address][channel]) {
                deviation = calc_deviation(bins[address][channel]);
            }

            // If deviation is less than the threshold, then SS
            if (deviation < 0.33) {
                fprintf(fp, "Y,");
                if (print) {
                    printf(GRNBG BLKFG " %3.2f" RESET, valueF);
                    printf(" |");
                }
            } else {
                // Otherwise, transient
                fprintf(fp, "N,");
                if (print) {
                    printf(REDBG BLKFG " %3.2f" RESET, valueF);
                    printf(" |");
                }
            }
        }
    }

    fprintf(fp, "\n");

    // Flush the buffer to keep it up to date
    fflush(fp);

    return result;
}

double calc_deviation(double bin[MAX_BIN_SIZE])
{
    double deviation = 0, sum = 0, mean = 0, temp = 0;
    for(int i = 0; i < MAX_BIN_SIZE; i++)
    {
        sum += bin[i];
    }
    mean = sum/(double)MAX_BIN_SIZE;
    for(int i = 0; i < MAX_BIN_SIZE; i++)
    {
        temp += pow(bin[i]-mean, 2);
    }
    deviation = sqrt(temp/(double)MAX_BIN_SIZE);
    return deviation;
}
