#include "unity.h"
#include <stdbool.h>
#include <stdlib.h>
#include "../../examples/autotest-validate/autotest-validate.h"
#include "../../assignment-autotest/test/assignment1/username-from-conf-file.h"

void test_validate_my_username()
{
    const char* result = my_username();
    char* expected = malloc_username_from_conf_file();

    TEST_ASSERT_EQUAL_STRING_MESSAGE(expected, result, "username should be equal");
    free(expected);
}
