/*
    bsi-read.c
    Copyright (C) 2011-2012  Pali Rohár <pali.rohar@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdio.h>

/* TWL_4030_MADC definitions from kernel module twl4030-madc.c */
#define TWL4030_MADC_PATH              "/dev/twl4030-adc"
#define TWL4030_MADC_IOC_MAGIC         '`'
#define TWL4030_MADC_IOCX_ADC_RAW_READ _IO(TWL4030_MADC_IOC_MAGIC, 0)

struct twl4030_madc_user_parms {
    int channel;
    int average;
    int status;
    unsigned short int result;
};

int main(void) {

	struct twl4030_madc_user_parms par;
	int fd = open(TWL4030_MADC_PATH, O_RDONLY);

	if ( fd < 0 ) {

		fd = open("/tmp" TWL4030_MADC_PATH, O_RDONLY);

		if ( fd < 0 ) {

			perror("Could not open " TWL4030_MADC_PATH);
			return 1;

		}

	}

	par.channel = 4;
	par.average = 1;

	if ( ioctl(fd, TWL4030_MADC_IOCX_ADC_RAW_READ, &par) != 0 || par.status != 0 )
		par.result = -1;

	printf("raw BSI ADC reading: %d\n", par.result);

	/* TODO: How to calculate BSI ?? */
	printf("BSI: %d\n", -1); /* in mAh */

	par.channel = 12;
	par.average = 1;

	if ( ioctl(fd, TWL4030_MADC_IOCX_ADC_RAW_READ, &par) != 0 || par.status != 0 )
		par.result = -1;

	printf("raw battery level: %d\n", par.result);

	/* FIXME: Is this correct ? */
	/* In disassembled armel binary is this calculation: */
	/* (((((-2145384445) * level * 6000) >> 32) + (level * 6000) - ((level * 6000) >> 31)) >> 9) */
	/* (((((2099203 - (1LL<<31)) * level * 6000) >> 32) - ((level * 6000 << 1) >> 32) + (level * 6000)) >> 9) */
	/* and it could be similar as: */
	printf("battery level: %d\n", par.result * 3003 / 512); /* in mV */

	return 0;

}
