/* CMSIS-DAP Interface Firmware
 * Copyright (c) 2009-2013 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
 
#include "sam3u.h"
#include "RTL.h"
#include "debug_cm.h"
#include "target_reset.h"
#include "swd_host.h"
#include "gpio.h"
#include "target_flash.h"
#include "target_config.h"
#include "flash_blob.h"

#include "string.h"

#define MIN_FLASH_ADDRESS   0x00000000
#define MAX_FLASH_ADDRESS   0x00020000
#define MIN_RAM_ADDRESS     0x20000000
#define MAX_RAM_ADDRESS     0x20004000

static /*inline*/ uint32_t test_range(const uint32_t test, const uint32_t min, const uint32_t max)
{
    return ((test < min) || (test > max)) ? 0 : 1;
}

uint8_t validate_bin_nvic(uint8_t *buf)
{
    // test for known required NVIC entries
    //  0 is stack pointer (RAM address)
    //  1 is Reset vector  (FLASH address)
    uint32_t nvic_sp = 0;
    uint32_t nvic_rv = 0;
    // test the initial SP value
    memcpy(&nvic_sp, buf, sizeof(nvic_sp));
    if (0 == test_range(nvic_sp, MIN_RAM_ADDRESS, MAX_RAM_ADDRESS)) {
        return 0;
    }
    // test the initial reset vector
    memcpy(&nvic_rv, buf+4, sizeof(nvic_rv));
    if (0 == test_range(nvic_rv, MIN_FLASH_ADDRESS, MAX_FLASH_ADDRESS)) {
        return 0;
    }
    
    return 1;
}


target_flash_status_t target_flash_init(void) {
    // Download flash programming algorithm to target and initialise.
    if (!swd_write_memory(flash.algo_start, (uint8_t *)flash.image, flash.algo_size)) {
        return TARGET_FAIL_ALGO_DL;
    }

    if (!swd_flash_syscall_exec(&flash.sys_call_param, flash.init, 0, 0 /* clk value is not used */, 0, 0)) {
        return TARGET_FAIL_INIT;
    }

    return TARGET_OK;
}

target_flash_status_t target_flash_erase_sector(unsigned int sector) {
    if (!swd_flash_syscall_exec(&flash.sys_call_param, flash.erase_sector, sector*FLASH_SECTOR_SIZE, 0, 0, 0)) {
        return TARGET_FAIL_ERASE_SECTOR;
    }

    return TARGET_OK;
}

target_flash_status_t target_flash_erase_chip(void) {
    //
	// 1 == O.K.
	// 0 == Error
	//
   if (!swd_flash_syscall_exec(&flash.sys_call_param, flash.erase_chip, 0, 0, 0, 0)) {   // 1 == O.K., 0 == Error
        return TARGET_FAIL_ERASE_ALL;
    }
    
    target_set_state(RESET_PROGRAM);
    target_flash_init();    
    
    return TARGET_OK;
}

target_flash_status_t target_flash_program_page(uint32_t addr, uint8_t * buf, uint32_t size)
{
    uint32_t bytes_written = 0;
	  // Program a page in target flash.
    if (!swd_write_memory(flash.program_buffer, buf, size)) {
        return TARGET_FAIL_ALGO_DATA_SEQ;
    }
    while(bytes_written < size) {
        if (!swd_flash_syscall_exec(&flash.sys_call_param,
                                    flash.program_page,
                                    addr,                                     // arg1
                                    flash.ram_to_flash_bytes_to_be_written,   // arg2
                                    flash.program_buffer + bytes_written, 0)) { // arg3, arg4
            return TARGET_FAIL_WRITE;
        }
        bytes_written += flash.ram_to_flash_bytes_to_be_written;
        addr += flash.ram_to_flash_bytes_to_be_written;
    }

    return TARGET_OK;
}

