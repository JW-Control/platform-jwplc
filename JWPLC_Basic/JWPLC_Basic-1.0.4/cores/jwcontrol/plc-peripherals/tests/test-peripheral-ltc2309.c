/**
 * Copyright (c) 2024 Industrial Shields. All rights reserved
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <plc-peripherals.h>

#if defined(PLC_ENVIRONMENT) && PLC_ENVIRONMENT == Linux

#include <string.h>
#include <unistd.h>
#include <errno.h>

#define LTC2309_ADDRESS 0x28
#define UNUSED_ADDRESS  0x30

#include <unity.h>
void setUp(void) {
    // set stuff up here
}

void tearDown(void) {
    // clean stuff up here
}

void ltc2309_init_sanity_check() {
	i2c_interface_t* i2c = i2c_init(1);
	TEST_ASSERT_NOT_NULL_MESSAGE(i2c, strerror(errno));


	TEST_ASSERT_EQUAL_MESSAGE(-1, ltc2309_init(NULL, 0xFF), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));
		
	TEST_ASSERT_EQUAL_MESSAGE(-1, ltc2309_init(NULL, UNUSED_ADDRESS), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));
	
	TEST_ASSERT_EQUAL_MESSAGE(-1, ltc2309_init(i2c, 0xFF), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EINVAL, errno, strerror(errno));
	
	TEST_ASSERT_EQUAL_MESSAGE(-1, ltc2309_init(i2c, UNUSED_ADDRESS), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EIO, errno, strerror(errno));


	TEST_ASSERT_EQUAL_MESSAGE(0, i2c_deinit(&i2c), strerror(errno));
	TEST_ASSERT_NULL_MESSAGE(i2c, strerror(errno));
}

void ltc2309_deinit_sanity_check() {
	i2c_interface_t* i2c = i2c_init(1);
	TEST_ASSERT_NOT_NULL_MESSAGE(i2c, strerror(errno));


	TEST_ASSERT_EQUAL_MESSAGE(-1, ltc2309_deinit(NULL, 0xFF), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));
		
	TEST_ASSERT_EQUAL_MESSAGE(-1, ltc2309_deinit(NULL, UNUSED_ADDRESS), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EFAULT, errno, strerror(errno));
	
	TEST_ASSERT_EQUAL_MESSAGE(-1, ltc2309_deinit(i2c, 0xFF), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(EINVAL, errno, strerror(errno));
	
	TEST_ASSERT_EQUAL_MESSAGE(0, ltc2309_deinit(i2c, UNUSED_ADDRESS), strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(0, errno, strerror(errno));


	TEST_ASSERT_EQUAL_MESSAGE(0, i2c_deinit(&i2c), strerror(errno));
	TEST_ASSERT_NULL_MESSAGE(i2c, strerror(errno));
}

void ltc2309_init_deinit_cycle() {
	i2c_interface_t* i2c = i2c_init(1);
        TEST_ASSERT_NOT_NULL_MESSAGE(i2c, strerror(errno));


        TEST_ASSERT_EQUAL_MESSAGE(0, ltc2309_init(i2c, LTC2309_ADDRESS), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, errno, strerror(errno));

        TEST_ASSERT_EQUAL_MESSAGE(0, ltc2309_deinit(i2c, LTC2309_ADDRESS), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, errno, strerror(errno));


        TEST_ASSERT_MESSAGE(i2c_deinit(&i2c) == 0, strerror(errno));
        TEST_ASSERT_NULL_MESSAGE(i2c, strerror(errno));
}

void ltc2309_non_existant_read_test() {
	i2c_interface_t* i2c = i2c_init(1);
	TEST_ASSERT_NOT_NULL_MESSAGE(i2c, strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(-1, ltc2309_init(i2c, 0x30), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(EIO, errno, strerror(errno));

	TEST_ASSERT_EQUAL_MESSAGE(0, i2c_deinit(&i2c), strerror(errno));
	TEST_ASSERT_NULL_MESSAGE(i2c, strerror(errno));
}

static void read_ltc_channel(i2c_interface_t* i2c, uint8_t channel, uint32_t lower_threshold, uint32_t upper_threshold) {
	for (size_t c = 0; c < 10; c++) {
		uint16_t result;
		TEST_ASSERT_EQUAL_MESSAGE(0, ltc2309_read(i2c, LTC2309_ADDRESS, channel, &result), strerror(errno));
		TEST_ASSERT_EQUAL_MESSAGE(0, errno, strerror(errno));
		TEST_ASSERT_GREATER_THAN(lower_threshold, result);
		TEST_ASSERT_LESS_THAN(upper_threshold, result);
	}
}
void ltc2309_read_test() {
	i2c_interface_t* i2c = i2c_init(1);
	TEST_ASSERT_NOT_NULL_MESSAGE(i2c, strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(0, ltc2309_init(i2c, LTC2309_ADDRESS), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, errno, strerror(errno));

        uint16_t result;
        ltc2309_read(i2c, LTC2309_ADDRESS, 0, &result);

        read_ltc_channel(i2c, 4, 850, 2000); // VPLC_OUT, it has to be between 850-2000 (12-24V)

        TEST_ASSERT_EQUAL_MESSAGE(0, ltc2309_deinit(i2c, LTC2309_ADDRESS), strerror(errno));
        TEST_ASSERT_EQUAL_MESSAGE(0, errno, strerror(errno));
	TEST_ASSERT_EQUAL_MESSAGE(0, i2c_deinit(&i2c), strerror(errno));
	TEST_ASSERT_NULL_MESSAGE(i2c, strerror(errno));
}

int main() {
	UNITY_BEGIN();

	RUN_TEST(ltc2309_init_sanity_check);
	RUN_TEST(ltc2309_deinit_sanity_check);
	RUN_TEST(ltc2309_init_deinit_cycle);

	RUN_TEST(ltc2309_non_existant_read_test);

	RUN_TEST(ltc2309_read_test);

        return UNITY_END();
}

#endif // PLC_ENVIRONMENT == Linux
