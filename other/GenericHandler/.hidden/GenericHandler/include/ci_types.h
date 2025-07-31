
#ifndef CI_TYPES_H
#define CI_TYPES_H

#define CI_MAX_COMMENT_LEN      (450000)

#define CI_MAX_CMD_LEN          (256)

#define CI_CPI_MAX_PATH_LEN        (1024 + 2 + 1)

#define CI_SITE_NOT_INSERTED 		(0)
#define CI_SITE_INSERTED_TO_TEST	(1)
#define CI_SITE_INSERTED_TO_BYPASS	(2)

#define CI_SITE_NOT_TO_REPROBE	(0)
#define CI_SITE_TO_REPROBE	(1)

typedef enum {
        CI_CALL_PASS          = 0,
        CI_TEST_PASS          = 0,
        CI_TEST_FAIL          = 1,
        CI_CALL_ERROR         = 2,
        CI_CALL_BREAKD        = 3,
        CI_UNDEFINED_RETURN   = 4,
        CI_CMD_NOEXIST        = 5,
        CI_CMD_START_FAILURE  = 6,
        CI_PROCESS_ABORT      = 7,
        CI_CALL_JAM           = 8,
        CI_CALL_LOT_START     = 9,
        CI_CALL_LOT_DONE      = 10,
        CI_CALL_DEVICE_START  = 11,
        CI_CALL_TIMEOUT       = 12
} CI_RETURN_STATUS;

#endif
