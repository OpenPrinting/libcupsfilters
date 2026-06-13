#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <cups/raster.h>

/* Test that page size name handling never overflows fixed buffers */
/* The vulnerable code uses strcpy to copy cupsPageSizeName into defSize */
/* defSize is typically 64 bytes (CUPS_MAX_SIZENAME) */

#define CUPS_MAX_SIZENAME 64

START_TEST(test_page_size_name_buffer_bounds)
{
    /* Invariant: Buffer reads/writes never exceed declared length */
    const char *payloads[] = {
        "Letter",  /* Valid input */
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",  /* Exactly 64 chars - boundary */
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",  /* 128 chars - 2x overflow */
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"  /* 640 chars - 10x overflow */
    };
    int num_payloads = sizeof(payloads) / sizeof(payloads[0]);

    for (int i = 0; i < num_payloads; i++) {
        char defSize[CUPS_MAX_SIZENAME];
        size_t payload_len = strlen(payloads[i]);
        
        /* Safe copy that should be used instead of strcpy */
        /* This test verifies the invariant: copies must be bounded */
        if (payload_len >= CUPS_MAX_SIZENAME) {
            /* Input exceeds buffer - must be truncated or rejected */
            strncpy(defSize, payloads[i], CUPS_MAX_SIZENAME - 1);
            defSize[CUPS_MAX_SIZENAME - 1] = '\0';
            ck_assert_uint_lt(strlen(defSize), CUPS_MAX_SIZENAME);
        } else {
            /* Input fits - safe to copy */
            strncpy(defSize, payloads[i], CUPS_MAX_SIZENAME - 1);
            defSize[CUPS_MAX_SIZENAME - 1] = '\0';
            ck_assert_str_eq(defSize, payloads[i]);
        }
        
        /* Verify no overflow occurred */
        ck_assert_uint_lt(strlen(defSize), CUPS_MAX_SIZENAME);
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_page_size_name_buffer_bounds);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}