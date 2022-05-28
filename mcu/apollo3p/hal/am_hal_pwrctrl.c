//*****************************************************************************
//
//  am_hal_pwrctrl.c
//! @file
//!
//! @brief Functions for enabling and disabling power domains.
//!
//! @addtogroup pwrctrl3p Power Control
//! @ingroup apollo3phal
//! @{
//
//*****************************************************************************

//*****************************************************************************
//
// Copyright (c) 2021, Ambiq Micro, Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
// contributors may be used to endorse or promote products derived from this
// software without specific prior written permission.
//
// Third party software included in this distribution is subject to the
// additional license terms as defined in the /docs/licenses directory.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// This is part of revision release_sdk_3_0_0-742e5ac27c of the AmbiqSuite Development Package.
//
//*****************************************************************************

#include <stdint.h>
#include <stdbool.h>
#include "am_mcu_apollo.h"

//*****************************************************************************
//
//  Defines
//
//*****************************************************************************
//
// Maximum number of checks to memory power status before declaring error.
//
#define AM_HAL_PWRCTRL_MAX_WAIT     20

//*****************************************************************************
//
//  Globals
//
//*****************************************************************************
bool g_bSimobuckTrimsDone = false;

//
//    Mask of HCPA Enables from the PWRCTRL->DEVPWRSTATUS register mapped to the
//        PWRCTRL->DEVPWREN register
//
#define HCPA_MASK       ( \
    _VAL2FLD(PWRCTRL_DEVPWREN_PWRIOS, PWRCTRL_DEVPWREN_PWRIOS_EN) | \
    _VAL2FLD(PWRCTRL_DEVPWREN_PWRUART0, PWRCTRL_DEVPWREN_PWRUART0_EN) | \
    _VAL2FLD(PWRCTRL_DEVPWREN_PWRUART1, PWRCTRL_DEVPWREN_PWRUART1_EN) | \
    _VAL2FLD(PWRCTRL_DEVPWREN_PWRSCARD, PWRCTRL_DEVPWREN_PWRSCARD_EN))

//
//    Mask of HCPB Enables from the PWRCTRL->DEVPWRSTATUS register mapped to the
//        PWRCTRL->DEVPWREN register
//
#define HCPB_MASK       ( \
    _VAL2FLD(PWRCTRL_DEVPWREN_PWRIOM0, PWRCTRL_DEVPWREN_PWRIOM0_EN) | \
    _VAL2FLD(PWRCTRL_DEVPWREN_PWRIOM1, PWRCTRL_DEVPWREN_PWRIOM1_EN) | \
    _VAL2FLD(PWRCTRL_DEVPWREN_PWRIOM2, PWRCTRL_DEVPWREN_PWRIOM2_EN))

//
//    Mask of HCPC Enables from the PWRCTRL->DEVPWRSTATUS register mapped to the
//        PWRCTRL->DEVPWREN register
//
#define HCPC_MASK       ( \
    _VAL2FLD(PWRCTRL_DEVPWREN_PWRIOM3, PWRCTRL_DEVPWREN_PWRIOM3_EN) | \
    _VAL2FLD(PWRCTRL_DEVPWREN_PWRIOM4, PWRCTRL_DEVPWREN_PWRIOM4_EN) | \
    _VAL2FLD(PWRCTRL_DEVPWREN_PWRIOM5, PWRCTRL_DEVPWREN_PWRIOM5_EN))

//
//    Mask of MSPI Enables from the PWRCTRL->DEVPWRSTATUS register mapped to the
//        PWRCTRL->DEVPWREN register
//
#define MSPI_MASK       ( \
    _VAL2FLD(PWRCTRL_DEVPWREN_PWRMSPI0, PWRCTRL_DEVPWREN_PWRMSPI0_EN) | \
    _VAL2FLD(PWRCTRL_DEVPWREN_PWRMSPI1, PWRCTRL_DEVPWREN_PWRMSPI1_EN) | \
    _VAL2FLD(PWRCTRL_DEVPWREN_PWRMSPI2, PWRCTRL_DEVPWREN_PWRMSPI2_EN))

//
// Define the peripheral control structure.
//
const struct
{
    uint32_t      ui32PeriphEnable;
    uint32_t      ui32PeriphStatus;
    uint32_t      ui32PeriphEvent;
}

am_hal_pwrctrl_peripheral_control[AM_HAL_PWRCTRL_PERIPH_MAX] =
{
    {0, 0, 0},                                  //  AM_HAL_PWRCTRL_PERIPH_NONE
    {_VAL2FLD(PWRCTRL_DEVPWREN_PWRIOS, PWRCTRL_DEVPWREN_PWRIOS_EN),
     PWRCTRL_DEVPWRSTATUS_HCPA_Msk,
     _VAL2FLD(PWRCTRL_DEVPWREVENTEN_HCPAEVEN, PWRCTRL_DEVPWREVENTEN_HCPAEVEN_EN)},  // AM_HAL_PWRCTRL_PERIPH_IOS
    {_VAL2FLD(PWRCTRL_DEVPWREN_PWRIOM0, PWRCTRL_DEVPWREN_PWRIOM0_EN),
     PWRCTRL_DEVPWRSTATUS_HCPB_Msk,
     _VAL2FLD(PWRCTRL_DEVPWREVENTEN_HCPBEVEN, PWRCTRL_DEVPWREVENTEN_HCPBEVEN_EN)},  //  AM_HAL_PWRCTRL_PERIPH_IOM0
    {_VAL2FLD(PWRCTRL_DEVPWREN_PWRIOM1, PWRCTRL_DEVPWREN_PWRIOM1_EN),
     PWRCTRL_DEVPWRSTATUS_HCPB_Msk,
     _VAL2FLD(PWRCTRL_DEVPWREVENTEN_HCPBEVEN, PWRCTRL_DEVPWREVENTEN_HCPBEVEN_EN)},  //  AM_HAL_PWRCTRL_PERIPH_IOM1
    {_VAL2FLD(PWRCTRL_DEVPWREN_PWRIOM2, PWRCTRL_DEVPWREN_PWRIOM2_EN),
     PWRCTRL_DEVPWRSTATUS_HCPB_Msk,
     _VAL2FLD(PWRCTRL_DEVPWREVENTEN_HCPBEVEN, PWRCTRL_DEVPWREVENTEN_HCPBEVEN_EN)},  //  AM_HAL_PWRCTRL_PERIPH_IOM2
    {_VAL2FLD(PWRCTRL_DEVPWREN_PWRIOM3, PWRCTRL_DEVPWREN_PWRIOM3_EN),
     PWRCTRL_DEVPWRSTATUS_HCPC_Msk,
     _VAL2FLD(PWRCTRL_DEVPWREVENTEN_HCPCEVEN, PWRCTRL_DEVPWREVENTEN_HCPCEVEN_EN)},  //  AM_HAL_PWRCTRL_PERIPH_IOM3
    {_VAL2FLD(PWRCTRL_DEVPWREN_PWRIOM4, PWRCTRL_DEVPWREN_PWRIOM4_EN),
     PWRCTRL_DEVPWRSTATUS_HCPC_Msk,
     _VAL2FLD(PWRCTRL_DEVPWREVENTEN_HCPCEVEN, PWRCTRL_DEVPWREVENTEN_HCPCEVEN_EN)},  //  AM_HAL_PWRCTRL_PERIPH_IOM4
    {_VAL2FLD(PWRCTRL_DEVPWREN_PWRIOM5, PWRCTRL_DEVPWREN_PWRIOM5_EN),
     PWRCTRL_DEVPWRSTATUS_HCPC_Msk,
     _VAL2FLD(PWRCTRL_DEVPWREVENTEN_HCPCEVEN, PWRCTRL_DEVPWREVENTEN_HCPCEVEN_EN)},  //  AM_HAL_PWRCTRL_PERIPH_IOM5
    {_VAL2FLD(PWRCTRL_DEVPWREN_PWRUART0, PWRCTRL_DEVPWREN_PWRUART0_EN),
     PWRCTRL_DEVPWRSTATUS_HCPA_Msk,
     _VAL2FLD(PWRCTRL_DEVPWREVENTEN_HCPAEVEN, PWRCTRL_DEVPWREVENTEN_HCPAEVEN_EN)},  //  AM_HAL_PWRCTRL_PERIPH_UART0
    {_VAL2FLD(PWRCTRL_DEVPWREN_PWRUART1, PWRCTRL_DEVPWREN_PWRUART1_EN),
     PWRCTRL_DEVPWRSTATUS_HCPA_Msk,
     _VAL2FLD(PWRCTRL_DEVPWREVENTEN_HCPAEVEN, PWRCTRL_DEVPWREVENTEN_HCPAEVEN_EN)},  //  AM_HAL_PWRCTRL_PERIPH_UART1
    {_VAL2FLD(PWRCTRL_DEVPWREN_PWRADC, PWRCTRL_DEVPWREN_PWRADC_EN),
     PWRCTRL_DEVPWRSTATUS_PWRADC_Msk,
     _VAL2FLD(PWRCTRL_DEVPWREVENTEN_ADCEVEN, PWRCTRL_DEVPWREVENTEN_ADCEVEN_EN)},    //  AM_HAL_PWRCTRL_PERIPH_ADC
    {_VAL2FLD(PWRCTRL_DEVPWREN_PWRSCARD, PWRCTRL_DEVPWREN_PWRSCARD_EN),
     PWRCTRL_DEVPWRSTATUS_HCPA_Msk,
     _VAL2FLD(PWRCTRL_DEVPWREVENTEN_HCPAEVEN, PWRCTRL_DEVPWREVENTEN_HCPAEVEN_EN)},  //  AM_HAL_PWRCTRL_PERIPH_SCARD
    {_VAL2FLD(PWRCTRL_DEVPWREN_PWRMSPI0, PWRCTRL_DEVPWREN_PWRMSPI0_EN),
     PWRCTRL_DEVPWRSTATUS_PWRMSPI_Msk,
     _VAL2FLD(PWRCTRL_DEVPWREVENTEN_MSPIEVEN, PWRCTRL_DEVPWREVENTEN_MSPIEVEN_EN)},  //  AM_HAL_PWRCTRL_PERIPH_MSPI0
    {_VAL2FLD(PWRCTRL_DEVPWREN_PWRMSPI1, PWRCTRL_DEVPWREN_PWRMSPI1_EN),
     PWRCTRL_DEVPWRSTATUS_PWRMSPI_Msk,
     _VAL2FLD(PWRCTRL_DEVPWREVENTEN_MSPIEVEN, PWRCTRL_DEVPWREVENTEN_MSPIEVEN_EN)},  //  AM_HAL_PWRCTRL_PERIPH_MSPI1
    {_VAL2FLD(PWRCTRL_DEVPWREN_PWRMSPI2, PWRCTRL_DEVPWREN_PWRMSPI2_EN),
     PWRCTRL_DEVPWRSTATUS_PWRMSPI_Msk,
     _VAL2FLD(PWRCTRL_DEVPWREVENTEN_MSPIEVEN, PWRCTRL_DEVPWREVENTEN_MSPIEVEN_EN)},  //  AM_HAL_PWRCTRL_PERIPH_MSPI2
    {_VAL2FLD(PWRCTRL_DEVPWREN_PWRPDM, PWRCTRL_DEVPWREN_PWRPDM_EN),
     PWRCTRL_DEVPWRSTATUS_PWRPDM_Msk,
     _VAL2FLD(PWRCTRL_DEVPWREVENTEN_PDMEVEN, PWRCTRL_DEVPWREVENTEN_PDMEVEN_EN)},    //  AM_HAL_PWRCTRL_PERIPH_PDM
    {_VAL2FLD(PWRCTRL_DEVPWREN_PWRBLEL, PWRCTRL_DEVPWREN_PWRBLEL_EN),
     PWRCTRL_DEVPWRSTATUS_BLEL_Msk,
     _VAL2FLD(PWRCTRL_DEVPWREVENTEN_BLELEVEN, PWRCTRL_DEVPWREVENTEN_BLELEVEN_EN)}   //  AM_HAL_PWRCTRL_PERIPH_BLEL
};

//
// Define the memory control structure.
//
const struct
{
    uint32_t      ui32MemoryEnable;
    uint32_t      ui32MemoryStatus;
    uint32_t      ui32MemoryEvent;
    uint32_t      ui32MemoryMask;
    uint32_t      ui32StatusMask;
    uint32_t      ui32PwdSlpEnable;
}

am_hal_pwrctrl_memory_control[AM_HAL_PWRCTRL_MEM_MAX] =
{
    {0, 0, 0, 0, 0, 0},
    {AM_HAL_PWRCTRL_MEMEN_SRAM_8K_DTCM,
     AM_HAL_PWRCTRL_PWRONSTATUS_SRAM_8K_DTCM,
     AM_HAL_PWRCTRL_MEMPWREVENTEN_SRAM_8K_DTCM,
     AM_HAL_PWRCTRL_MEM_REGION_SRAM_MASK,
     AM_HAL_PWRCTRL_MEM_REGION_SRAM_MASK,
     AM_HAL_PWRCTRL_MEMPWDINSLEEP_SRAM_8K_DTCM},
    {AM_HAL_PWRCTRL_MEMEN_SRAM_32K_DTCM,
     AM_HAL_PWRCTRL_PWRONSTATUS_SRAM_32K_DTCM,
     AM_HAL_PWRCTRL_MEMPWREVENTEN_SRAM_32K_DTCM,
     AM_HAL_PWRCTRL_MEM_REGION_SRAM_MASK,
     AM_HAL_PWRCTRL_MEM_REGION_SRAM_MASK,
     AM_HAL_PWRCTRL_MEMPWDINSLEEP_SRAM_32K_DTCM},
    {AM_HAL_PWRCTRL_MEMEN_SRAM_64K_DTCM,
     AM_HAL_PWRCTRL_PWRONSTATUS_SRAM_64K_DTCM,
     AM_HAL_PWRCTRL_MEMPWREVENTEN_SRAM_64K_DTCM,
     AM_HAL_PWRCTRL_MEM_REGION_SRAM_MASK,
     AM_HAL_PWRCTRL_MEM_REGION_SRAM_MASK,
     AM_HAL_PWRCTRL_MEMPWDINSLEEP_SRAM_64K_DTCM},
    {AM_HAL_PWRCTRL_MEMEN_SRAM_128K,
     AM_HAL_PWRCTRL_PWRONSTATUS_SRAM_128K,
     AM_HAL_PWRCTRL_MEMPWREVENTEN_SRAM_128K,
     AM_HAL_PWRCTRL_MEM_REGION_SRAM_MASK,
     AM_HAL_PWRCTRL_MEM_REGION_SRAM_MASK,
     AM_HAL_PWRCTRL_MEMPWDINSLEEP_SRAM_128K},
    {AM_HAL_PWRCTRL_MEMEN_SRAM_192K,
     AM_HAL_PWRCTRL_PWRONSTATUS_SRAM_192K,
     AM_HAL_PWRCTRL_MEMPWREVENTEN_SRAM_192K,
     AM_HAL_PWRCTRL_MEM_REGION_SRAM_MASK,
     AM_HAL_PWRCTRL_MEM_REGION_SRAM_MASK,
     AM_HAL_PWRCTRL_MEMPWDINSLEEP_SRAM_192K},
    {AM_HAL_PWRCTRL_MEMEN_SRAM_256K,
     AM_HAL_PWRCTRL_PWRONSTATUS_SRAM_256K,
     AM_HAL_PWRCTRL_MEMPWREVENTEN_SRAM_256K,
     AM_HAL_PWRCTRL_MEM_REGION_SRAM_MASK,
     AM_HAL_PWRCTRL_MEM_REGION_SRAM_MASK,
     AM_HAL_PWRCTRL_MEMPWDINSLEEP_SRAM_256K},
    {AM_HAL_PWRCTRL_MEMEN_SRAM_320K,
     AM_HAL_PWRCTRL_PWRONSTATUS_SRAM_320K,
     AM_HAL_PWRCTRL_MEMPWREVENTEN_SRAM_320K,
     AM_HAL_PWRCTRL_MEM_REGION_SRAM_MASK,
     AM_HAL_PWRCTRL_MEM_REGION_SRAM_MASK,
     AM_HAL_PWRCTRL_MEMPWDINSLEEP_SRAM_320K},
    {AM_HAL_PWRCTRL_MEMEN_SRAM_384K,
     AM_HAL_PWRCTRL_PWRONSTATUS_SRAM_384K,
     AM_HAL_PWRCTRL_MEMPWREVENTEN_SRAM_384K,
     AM_HAL_PWRCTRL_MEM_REGION_SRAM_MASK,
     AM_HAL_PWRCTRL_MEM_REGION_SRAM_MASK,
     AM_HAL_PWRCTRL_MEMPWDINSLEEP_SRAM_384K},
    {AM_HAL_PWRCTRL_MEMEN_SRAM_448K,
     AM_HAL_PWRCTRL_PWRONSTATUS_SRAM_448K,
     AM_HAL_PWRCTRL_MEMPWREVENTEN_SRAM_448K,
     AM_HAL_PWRCTRL_MEM_REGION_SRAM_MASK,
     AM_HAL_PWRCTRL_MEM_REGION_SRAM_MASK,
     AM_HAL_PWRCTRL_MEMPWDINSLEEP_SRAM_448K},
    {AM_HAL_PWRCTRL_MEMEN_SRAM_512K,
     AM_HAL_PWRCTRL_PWRONSTATUS_SRAM_512K,
     AM_HAL_PWRCTRL_MEMPWREVENTEN_SRAM_512K,
     AM_HAL_PWRCTRL_MEM_REGION_SRAM_MASK,
     AM_HAL_PWRCTRL_MEM_REGION_SRAM_MASK,
     AM_HAL_PWRCTRL_MEMPWDINSLEEP_SRAM_512K},
    {AM_HAL_PWRCTRL_MEMEN_SRAM_576K,
     AM_HAL_PWRCTRL_PWRONSTATUS_SRAM_576K,
     AM_HAL_PWRCTRL_MEMPWREVENTEN_SRAM_576K,
     AM_HAL_PWRCTRL_MEM_REGION_SRAM_MASK,
     AM_HAL_PWRCTRL_MEM_REGION_SRAM_MASK,
     AM_HAL_PWRCTRL_MEMPWDINSLEEP_SRAM_576K},
    {AM_HAL_PWRCTRL_MEMEN_SRAM_672K,
     AM_HAL_PWRCTRL_PWRONSTATUS_SRAM_672K,
     AM_HAL_PWRCTRL_MEMPWREVENTEN_SRAM_672K,
     AM_HAL_PWRCTRL_MEM_REGION_SRAM_MASK,
     AM_HAL_PWRCTRL_MEM_REGION_SRAM_MASK,
     AM_HAL_PWRCTRL_MEMPWDINSLEEP_SRAM_672K},
    {AM_HAL_PWRCTRL_MEMEN_SRAM_768K,
     AM_HAL_PWRCTRL_PWRONSTATUS_SRAM_768K,
     AM_HAL_PWRCTRL_MEMPWREVENTEN_SRAM_768K,
     AM_HAL_PWRCTRL_MEM_REGION_SRAM_MASK,
     AM_HAL_PWRCTRL_MEM_REGION_SRAM_MASK,
     AM_HAL_PWRCTRL_MEMPWDINSLEEP_SRAM_768K},
    {AM_HAL_PWRCTRL_MEMEN_FLASH_1M,
     AM_HAL_PWRCTRL_PWRONSTATUS_FLASH_1M,
     AM_HAL_PWRCTRL_MEMPWREVENTEN_FLASH_1M,
     AM_HAL_PWRCTRL_MEM_REGION_FLASH_MASK,
     AM_HAL_PWRCTRL_MEM_REGION_FLASH_MASK,
     AM_HAL_PWRCTRL_MEMPWDINSLEEP_FLASH_1M},
    {AM_HAL_PWRCTRL_MEMEN_FLASH_2M,
     AM_HAL_PWRCTRL_PWRONSTATUS_FLASH_2M,
     AM_HAL_PWRCTRL_MEMPWREVENTEN_FLASH_2M,
     AM_HAL_PWRCTRL_MEM_REGION_FLASH_MASK,
     AM_HAL_PWRCTRL_MEM_REGION_FLASH_MASK,
     AM_HAL_PWRCTRL_MEMPWDINSLEEP_FLASH_2M},
    {AM_HAL_PWRCTRL_MEMEN_CACHE,
     0,
     AM_HAL_PWRCTRL_MEMPWREVENTEN_CACHE,
     AM_HAL_PWRCTRL_MEM_REGION_CACHE_MASK,
     0,
     AM_HAL_PWRCTRL_MEMPWDINSLEEP_CACHE},
    {AM_HAL_PWRCTRL_MEMEN_ALL,
     AM_HAL_PWRCTRL_PWRONSTATUS_ALL,
     AM_HAL_PWRCTRL_MEMPWREVENTEN_ALL,
     AM_HAL_PWRCTRL_MEM_REGION_ALL_MASK,
     AM_HAL_PWRCTRL_MEM_REGION_ALT_ALL_MASK,
     AM_HAL_PWRCTRL_MEMPWDINSLEEP_ALL}
};

// ****************************************************************************
//
//  am_hal_pwrctrl_periph_enable()
//  Enable power for a peripheral.
//
// ****************************************************************************
uint32_t
am_hal_pwrctrl_periph_enable(am_hal_pwrctrl_periph_e ePeripheral)
{
    //
    // Enable power control for the given device.
    //
    AM_CRITICAL_BEGIN
    PWRCTRL->DEVPWREN |= am_hal_pwrctrl_peripheral_control[ePeripheral].ui32PeriphEnable;
    AM_CRITICAL_END

    for (uint32_t wait_usecs = 0; wait_usecs < AM_HAL_PWRCTRL_MAX_WAIT; wait_usecs += 10)
    {
        am_hal_flash_delay(FLASH_CYCLES_US(10));

        if ((PWRCTRL->DEVPWRSTATUS & am_hal_pwrctrl_peripheral_control[ePeripheral].ui32PeriphStatus) > 0)
        {
            break;
        }
    }

    //
    // Check the device status.
    //
    if ((PWRCTRL->DEVPWRSTATUS & am_hal_pwrctrl_peripheral_control[ePeripheral].ui32PeriphStatus) > 0)
    {
        return AM_HAL_STATUS_SUCCESS;
    }
    else
    {
        //
        // IF the Power Enable fails, make sure the DEVPWREN is Low
        //
        AM_CRITICAL_BEGIN
        PWRCTRL->DEVPWREN &= ~am_hal_pwrctrl_peripheral_control[ePeripheral].ui32PeriphEnable;
        AM_CRITICAL_END

        return AM_HAL_STATUS_FAIL;
    }
} // am_hal_pwrctrl_periph_enable()

// ****************************************************************************
//
//  am_hal_pwrctrl_periph_disable_msk_check()
//  Function checks the PWRCTRL->DEVPWREN since the PWRCTRL->DEVPWRSTATUS
//        register alone cannot tell the user if a peripheral is enabled when
//        and HCPx register is being used.
//
// ****************************************************************************
static uint32_t
pwrctrl_periph_disable_msk_check(am_hal_pwrctrl_periph_e ePeripheral)
{
    uint32_t retVal = AM_HAL_STATUS_FAIL;
    uint32_t HCPxMSPIxMask = PWRCTRL->DEVPWREN;

    switch (am_hal_pwrctrl_peripheral_control[ePeripheral].ui32PeriphStatus)
    {
        case (PWRCTRL_DEVPWRSTATUS_HCPA_Msk):
            if (((HCPxMSPIxMask & HCPA_MASK) > 0) && ((HCPxMSPIxMask & am_hal_pwrctrl_peripheral_control[ePeripheral].ui32PeriphEnable) == 0))
            {
                retVal = AM_HAL_STATUS_SUCCESS;
            }
            break;

        case (PWRCTRL_DEVPWRSTATUS_HCPB_Msk):
            if (((HCPxMSPIxMask & HCPB_MASK) > 0) && ((HCPxMSPIxMask & am_hal_pwrctrl_peripheral_control[ePeripheral].ui32PeriphEnable) == 0))
            {
                retVal = AM_HAL_STATUS_SUCCESS;
            }
            break;

        case (PWRCTRL_DEVPWRSTATUS_HCPC_Msk):
            if (((HCPxMSPIxMask & HCPC_MASK) > 0) && ((HCPxMSPIxMask & am_hal_pwrctrl_peripheral_control[ePeripheral].ui32PeriphEnable) == 0))
            {
                retVal = AM_HAL_STATUS_SUCCESS;
            }
            break;

        case (PWRCTRL_DEVPWRSTATUS_PWRMSPI_Msk):
            if (((HCPxMSPIxMask & MSPI_MASK) > 0) && ((HCPxMSPIxMask & am_hal_pwrctrl_peripheral_control[ePeripheral].ui32PeriphEnable) == 0))
            {
                retVal = AM_HAL_STATUS_SUCCESS;
            }
            break;

        default:
            break;
    }

    return retVal;
}

// ****************************************************************************
//
//  am_hal_pwrctrl_periph_disable()
//  Disable power for a peripheral.
//
// ****************************************************************************
uint32_t
am_hal_pwrctrl_periph_disable(am_hal_pwrctrl_periph_e ePeripheral)
{
    //
    // Disable power domain for the given device.
    //
    AM_CRITICAL_BEGIN
    PWRCTRL->DEVPWREN &= ~am_hal_pwrctrl_peripheral_control[ePeripheral].ui32PeriphEnable;
    AM_CRITICAL_END

    for (uint32_t wait_usecs = 0; wait_usecs < AM_HAL_PWRCTRL_MAX_WAIT; wait_usecs += 10)
    {
        am_hal_flash_delay(FLASH_CYCLES_US(10));

        if ((PWRCTRL->DEVPWRSTATUS & am_hal_pwrctrl_peripheral_control[ePeripheral].ui32PeriphStatus) == 0)
        {
            break;
        }
    }

    //
    // Check the device status.
    //
    if ((PWRCTRL->DEVPWRSTATUS & am_hal_pwrctrl_peripheral_control[ePeripheral].ui32PeriphStatus) == 0)
    {
        return AM_HAL_STATUS_SUCCESS;
    }
    else
    {
        return pwrctrl_periph_disable_msk_check(ePeripheral);
    }

} // am_hal_pwrctrl_periph_disable()

//*****************************************************************************
//
// am_hal_pwrctrl_periph_enabled()
// This function determines to the caller whether a given peripheral is
// currently enabled or disabled.
//
//*****************************************************************************
uint32_t
am_hal_pwrctrl_periph_enabled(am_hal_pwrctrl_periph_e ePeripheral,
                              uint32_t *pui32Enabled)
{
    uint32_t ui32Mask = am_hal_pwrctrl_peripheral_control[ePeripheral].ui32PeriphStatus;
    uint32_t ui32Enabled = 0;

    if (pui32Enabled == NULL)
    {
        return AM_HAL_STATUS_INVALID_ARG;
    }

    if (ui32Mask != 0)
    {
        ui32Enabled = PWRCTRL->DEVPWRSTATUS & ui32Mask ? 1 : 0;
        ui32Enabled = ui32Enabled && (PWRCTRL->DEVPWREN & am_hal_pwrctrl_peripheral_control[ePeripheral].ui32PeriphEnable);
    }

    *pui32Enabled = ui32Enabled;

    return AM_HAL_STATUS_SUCCESS;

} // am_hal_pwrctrl_periph_enabled()

// ****************************************************************************
//
//  am_hal_pwrctrl_memory_enable()
//  Enable a configuration of memory.
//
// ****************************************************************************
uint32_t
am_hal_pwrctrl_memory_enable(am_hal_pwrctrl_mem_e eMemConfig)
{
    uint32_t ui32MemEnMask, ui32MemDisMask, ui32MemRegionMask, ui32MemStatusMask;

    ui32MemEnMask     =  am_hal_pwrctrl_memory_control[eMemConfig].ui32MemoryEnable;
    ui32MemDisMask    = ~am_hal_pwrctrl_memory_control[eMemConfig].ui32MemoryEnable;
    ui32MemRegionMask = am_hal_pwrctrl_memory_control[eMemConfig].ui32MemoryMask;
    ui32MemStatusMask = am_hal_pwrctrl_memory_control[eMemConfig].ui32StatusMask;

    //
    // Disable unneeded memory. If nothing to be disabled, skip to save time.
    //
    // Note that a deliberate disable step using a disable mask is taken here
    // for 2 reasons: 1) To only affect the specified type of memory, and 2)
    // To avoid inadvertently disabling any memory currently being depended on.
    //
    if ( ui32MemDisMask != 0 )
    {
        PWRCTRL->MEMPWREN &=
            ~(ui32MemDisMask & ui32MemRegionMask)                                   |
             (_VAL2FLD(PWRCTRL_MEMPWREN_DTCM, PWRCTRL_MEMPWREN_DTCM_GROUP0DTCM0)    |
              _VAL2FLD(PWRCTRL_MEMPWREN_FLASH0, PWRCTRL_MEMPWREN_FLASH0_EN));
        am_hal_flash_delay(FLASH_CYCLES_US(1));
    }

    //
    // Enable the required memory.
    //
    if ( ui32MemEnMask != 0 )
    {
        PWRCTRL->MEMPWREN |= ui32MemEnMask;

        for (uint32_t wait_usecs = 0; wait_usecs < AM_HAL_PWRCTRL_MAX_WAIT; wait_usecs += 10)
        {
            am_hal_flash_delay(FLASH_CYCLES_US(10));

            if ( (PWRCTRL->MEMPWRSTATUS & ui32MemStatusMask) ==
                  am_hal_pwrctrl_memory_control[eMemConfig].ui32MemoryStatus )
            {
                break;
            }
        }
    }

    //
    // Return status based on whether the power control memory status has reached the desired state.
    //
    if ( ( PWRCTRL->MEMPWRSTATUS & ui32MemStatusMask) ==
           am_hal_pwrctrl_memory_control[eMemConfig].ui32MemoryStatus )
    {
        return AM_HAL_STATUS_SUCCESS;
    }
    else
    {
        return AM_HAL_STATUS_FAIL;
    }

} // am_hal_pwrctrl_memory_enable()

// ****************************************************************************
//
//  am_hal_pwrctrl_memory_deepsleep_powerdown()
//  Power down respective memory.
//
// ****************************************************************************
uint32_t
am_hal_pwrctrl_memory_deepsleep_powerdown(am_hal_pwrctrl_mem_e eMemConfig)
{
    if ( eMemConfig >= AM_HAL_PWRCTRL_MEM_MAX )
    {
        return AM_HAL_STATUS_FAIL;
    }

    //
    // Power down the required memory.
    //
    PWRCTRL->MEMPWDINSLEEP |= am_hal_pwrctrl_memory_control[eMemConfig].ui32PwdSlpEnable;

    return AM_HAL_STATUS_SUCCESS;

} // am_hal_pwrctrl_memory_deepsleep_powerdown()

// ****************************************************************************
//
//  am_hal_pwrctrl_memory_deepsleep_retain()
//  Apply retention voltage to respective memory.
//
// ****************************************************************************
uint32_t
am_hal_pwrctrl_memory_deepsleep_retain(am_hal_pwrctrl_mem_e eMemConfig)
{
    if ( eMemConfig >= AM_HAL_PWRCTRL_MEM_MAX )
    {
        return AM_HAL_STATUS_FAIL;
    }

    //
    // Retain the required memory.
    //
    PWRCTRL->MEMPWDINSLEEP &= ~am_hal_pwrctrl_memory_control[eMemConfig].ui32PwdSlpEnable;

    return AM_HAL_STATUS_SUCCESS;

} // am_hal_pwrctrl_memory_deepsleep_retain()

// ****************************************************************************
//  simobuck_updates()
// ****************************************************************************
static void
simobuck_updates(void)
{
    //
    // Adjust the SIMOBUCK LP settings.
    //
    if ( APOLLO3_GE_B0 )
    {
        if ( !g_bSimobuckTrimsDone )
        {
            uint32_t ui32LPTRIM;
            g_bSimobuckTrimsDone = true;
            ui32LPTRIM = MCUCTRL->SIMOBUCK1_b.SIMOBUCKMEMLPTRIM;
            ui32LPTRIM = ui32LPTRIM > 12 ? ui32LPTRIM - 12 : 0;
            MCUCTRL->SIMOBUCK1_b.SIMOBUCKMEMLPTRIM = ui32LPTRIM;
            MCUCTRL->SIMOBUCK2_b.SIMOBUCKCORELPLOWTONTRIM   = 8;
            MCUCTRL->SIMOBUCK2_b.SIMOBUCKCORELPHIGHTONTRIM  = 5;
            MCUCTRL->SIMOBUCK4_b.SIMOBUCKMEMLPLOWTONTRIM    = 8;
            MCUCTRL->SIMOBUCK3_b.SIMOBUCKMEMLPHIGHTONTRIM   = 6;
            MCUCTRL->SIMOBUCK4_b.SIMOBUCKCOMP2LPEN          = 1;
            MCUCTRL->SIMOBUCK4_b.SIMOBUCKCOMP2TIMEOUTEN     = 0;
            MCUCTRL->SIMOBUCK2_b.SIMOBUCKCORELEAKAGETRIM    = 3;
            MCUCTRL->SIMOBUCK2_b.SIMOBUCKTONGENTRIM         = 31;
        }
    }

    //
    // Adjust the SIMOBUCK Timeout settings.
    //
    if ( APOLLO3_GE_A1 )
    {
        MCUCTRL->SIMOBUCK4_b.SIMOBUCKCOMP2TIMEOUTEN = 0;
    }

} // simobuck_updates()

// ****************************************************************************
//
//  am_hal_pwrctrl_low_power_init()
//  Initialize system for low power configuration.
//
// ****************************************************************************
uint32_t
am_hal_pwrctrl_low_power_init(void)
{
    uint32_t      ui32Status;

    //
    // Take a snapshot of the reset status, if not done already
    //
    if ( !gAmHalResetStatus )
    {
        gAmHalResetStatus = RSTGEN->STAT;
    }

    //
    // Adjust the SIMOBUCK LP settings.
    //
    simobuck_updates();

    //
    // Configure cache for low power and performance.
    //
    am_hal_cachectrl_control(AM_HAL_CACHECTRL_CONTROL_LPMMODE_RECOMMENDED, 0);

    //
    // Check if the BLE is already enabled.
    //
    if ( PWRCTRL->DEVPWRSTATUS_b.BLEL == 0)
    {
        am_hal_pwrctrl_blebuck_trim();

        //
        // First request the BLE feature and check that it was available and acknowledged.
        //
        MCUCTRL->FEATUREENABLE_b.BLEREQ = 1;
        ui32Status = am_hal_flash_delay_status_check(10000,
                        (uint32_t)&MCUCTRL->FEATUREENABLE,
                        (MCUCTRL_FEATUREENABLE_BLEAVAIL_Msk |
                         MCUCTRL_FEATUREENABLE_BLEACK_Msk   |
                         MCUCTRL_FEATUREENABLE_BLEREQ_Msk),
                        (MCUCTRL_FEATUREENABLE_BLEAVAIL_Msk |
                         MCUCTRL_FEATUREENABLE_BLEACK_Msk   |
                         MCUCTRL_FEATUREENABLE_BLEREQ_Msk),
                         true);

        if (AM_HAL_STATUS_SUCCESS != ui32Status)
        {
            return AM_HAL_STATUS_TIMEOUT;
        }

        //
        // Next, enable the BLE Buck.
        //
        PWRCTRL->SUPPLYSRC |= _VAL2FLD(PWRCTRL_SUPPLYSRC_BLEBUCKEN,
                                       PWRCTRL_SUPPLYSRC_BLEBUCKEN_EN);

        //
        // Allow the buck to go to low power mode in BLE sleep.
        //
        PWRCTRL->MISC |= _VAL2FLD(PWRCTRL_MISC_MEMVRLPBLE,
                                  PWRCTRL_MISC_MEMVRLPBLE_EN);

    }

    return AM_HAL_STATUS_SUCCESS;
} // am_hal_pwrctrl_low_power_init()

// ****************************************************************************
//
//  am_hal_pwrctrl_blebuck_trim()
//
// ****************************************************************************
void am_hal_pwrctrl_blebuck_trim(void)
{
  //
  // Enable the BLE buck trim values
  //
  if ( APOLLO3_GE_A1 )
  {
    AM_CRITICAL_BEGIN
    MCUCTRL->BLEBUCK2_b.BLEBUCKTONHITRIM = 0x19;
    MCUCTRL->BLEBUCK2_b.BLEBUCKTONLOWTRIM = 0xC;
    CLKGEN->BLEBUCKTONADJ_b.TONADJUSTEN = CLKGEN_BLEBUCKTONADJ_TONADJUSTEN_DIS;
    AM_CRITICAL_END
  }
} // am_hal_pwrctrl_blebuck_trim()

//*****************************************************************************
//
// End Doxygen group.
//! @}
//
//*****************************************************************************
