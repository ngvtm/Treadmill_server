#pragma once

// Continuous (back-to-back) ranging on top of the vendored kerikun11
// VL53L0X wrapper — Phase 1, item 1.2. The wrapper itself only offers
// single-shot reads (readWithStatus -> PerformSingleRangingMeasurement,
// ~one timing budget per sensor, ~1.6 s for 8 sensors in sequence).
// This derived class exposes the ST API continuous primitives WITHOUT
// modifying vendored code: vl53l0x_dev is protected, so a subclass can
// reach it. Sequence per ST API (verified against the vendored
// vl53l0x_api.c):
//   SetDeviceMode(CONTINUOUS_RANGING) -> StartMeasurement (back-to-back)
//   poll: GetMeasurementDataReady -> GetRangingMeasurementData ->
//         ClearInterruptMask (re-arms the next sample)
//   budget change: StopMeasurement -> setTimingBudget -> StartMeasurement
// RangeStatus values follow the wrapper's readWithStatus scale
// (0 valid, 4 out-of-range real reading, 253 interface error).

#include "VL53L0X.h"
#include "vl53l0x_api.h"

class VL53L0XContinuous : public VL53L0X {
public:
    using VL53L0X::VL53L0X;

    bool startContinuous()
    {
        if (VL53L0X_SetDeviceMode(&vl53l0x_dev,
                                  VL53L0X_DEVICEMODE_CONTINUOUS_RANGING) !=
            VL53L0X_ERROR_NONE) {
            return false;
        }
        return VL53L0X_StartMeasurement(&vl53l0x_dev) == VL53L0X_ERROR_NONE;
    }

    bool stopContinuous()
    {
        if (VL53L0X_StopMeasurement(&vl53l0x_dev) != VL53L0X_ERROR_NONE) {
            return false;
        }
        // Back to single-shot mode so any later single-shot call (or a
        // re-init path that assumes it) behaves as before.
        return VL53L0X_SetDeviceMode(&vl53l0x_dev,
                                     VL53L0X_DEVICEMODE_SINGLE_RANGING) ==
               VL53L0X_ERROR_NONE;
    }

    bool setContinuousBudget(uint32_t timing_budget_us)
    {
        bool ok = true;
        if (!stopContinuous()) {
            ok = false;
        }
        if (!setTimingBudget(timing_budget_us)) {
            ok = false;
        }
        if (!startContinuous()) {
            // One retry: a sensor mid-measurement sometimes refuses the
            // first restart; a second attempt usually succeeds.
            if (!startContinuous()) {
                ok = false;
            }
        }
        return ok;
    }

    // Non-blocking poll. Returns false when no new sample is ready yet
    // (caller keeps the previous sample, bounded by the aging window).
    // Returns true with a fresh (distance, status) pair otherwise; an
    // interface error surfaces as status 253, exactly like the
    // wrapper's readWithStatus maps VL53L0X_ERROR_CONTROL_INTERFACE.
    bool pollContinuous(uint16_t *pDistanceMm, uint8_t *pRangeStatus)
    {
        uint8_t ready = 0;
        if (VL53L0X_GetMeasurementDataReady(&vl53l0x_dev, &ready) !=
            VL53L0X_ERROR_NONE) {
            *pDistanceMm = 0;
            *pRangeStatus = 253;
            return true;
        }
        if (!ready) {
            return false;
        }
        VL53L0X_RangingMeasurementData_t data = {};
        if (VL53L0X_GetRangingMeasurementData(&vl53l0x_dev, &data) !=
            VL53L0X_ERROR_NONE) {
            *pDistanceMm = 0;
            *pRangeStatus = 253;
            return true;
        }
        // Re-arm the next sample (same call the wrapper's interrupt
        // read path makes after fetching data).
        VL53L0X_ClearInterruptMask(&vl53l0x_dev, 0);
        *pDistanceMm = data.RangeMilliMeter;
        *pRangeStatus = data.RangeStatus;
        return true;
    }
};
