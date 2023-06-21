#ifndef FDOEM_H
#define FDOEM_H

#include "globe.h"

void fdoem_init(fdoem_device device);
void fdoem_calibrate_o2_low(float calib_temp);
void fdoem_calibrate_o2_high(float calib_temp);
void fdoem_calibrate_ph_low(float calib_temp, float calib_ph, float calib_salinity);
void fdoem_calibrate_ph_high(float calib_temp, float calib_ph, float calib_salinity);
void fdoem_calibrate_ph_offset(float calib_temp, float calib_ph, float calib_salinity);

struct fdoem_readings {
    /* Datasheet specifies readings as being scaled by a factor of 1000. i.e.
     * umolar for oxygen is actualy in units of nmol/L. The units are provided
     * here in their actual values. */

    int nmolar;       /* Oxygen level in units of nmol/L (valid only in liquids).
                       */
    int ubar;         /* Oxygen level in units of ubar (valid in gases and in
                       * liquids) */
    int air_sat;      /* Oxygen level in units of % air saturation (valid only
                       * in liquids). Units of % 10^-3. */
    int temp_optical; /* Optical temperature (measured by the optical
                       * sensor) */
    int ph;           /* pH * 10^-3. */
    int dphi;
};

/* The FDOEM requires a temperature reading for compensation during
 * calculation */
struct fdoem_readings fdoem_read(fdoem_device device, float temp_c,
                                 float salinity_g_per_l);

/* See datasheet for details on register blocks */
struct fdoem_o2_calibration_register_block {
    int dphi0;
    int dphi100;
    int temp0;
    int temp100;
    int pressure;
    int humidity;
    int f;
    int m;
    int calFreq;
    int tt;
    int kt;
    int bkgdAmpl;
    int bkgdDphi;
    int useKsv;
    int ksv;
    int ft;
    int mt;
    int reserved0;
    int precentO2;
    int reserved1[11];
};

void fdoem_read_o2_calibration_register_block(
    struct fdoem_o2_calibration_register_block *block);
void fdoem_write_o2_calibration_register_block(
    const struct fdoem_o2_calibration_register_block *block);

struct fdoem_ph_calibration_register_block {
    int pka;
    int slope;
    int dPHi_ref;
    int pka_t;
    int dyn_t;
    int bottom_t;
    int slope_t;
    int f;
    int lambda_std;
    int pka_is1;
    int pka_is2;
    int pkgdAmpl;
    int pkgdDphi;
    int offset;
    int dPhi1;
    int pH1;
    int temp1;
    int salinity1;
    int ldev1;
    int dPhi2;
    int pH2;
    int temp2;
    int salinity2;
    int ldev2;
    int Aon;
    int Aoff;
    int reserved[4];
};

void fdoem_read_ph_calibration_register_block(
    struct fdoem_ph_calibration_register_block *block);
void fdoem_write_ph_calibration_register_block(
    const struct fdoem_ph_calibration_register_block *block);

void fdoem_save_all_registers_to_flash(fdoem_device device);
void fdoem_load_all_registers_from_flash(fdoem_device device);

#endif
