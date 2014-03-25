// Adapted from BOINC sample_trivial_validator.

#include <cstdlib>
#include "config.h"
#include "boinc_db.h"
#include "validate_util.h"

using std::vector;

int init_result(RESULT& result, void*&) {
        DB_WORKUNIT wu;
        wu.lookup_id(result.workunitid);
        return 0;
}

int compare_results(RESULT&, void*, RESULT const&, void*, bool& match) {
        match = true;
        return 0;
}

int cleanup_result(RESULT const&, void*) {
        return 0;
}

