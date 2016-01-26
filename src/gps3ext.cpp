#define PGDLLIMPORT "C"

#include <cstdlib>

#include "postgres.h"
#include "funcapi.h"

#include "access/extprotocol.h"
#include "catalog/pg_proc.h"
#include "utils/array.h"
#include "utils/builtins.h"
#include "utils/memutils.h"
#include "fmgr.h"

#include "S3ExtWrapper.h"
#include "S3Common.h"
#include "S3Log.h"
#include "utils.h"

#include "gps3ext.h"
#include "gps3conf.h"

/* Do the module magic dance */

PG_MODULE_MAGIC;
PG_FUNCTION_INFO_V1(s3_export);
PG_FUNCTION_INFO_V1(s3_import);
PG_FUNCTION_INFO_V1(s3_validate_urls);

extern "C" {
Datum s3_export(PG_FUNCTION_ARGS);
Datum s3_import(PG_FUNCTION_ARGS);
Datum s3_validate_urls(PG_FUNCTION_ARGS);
}

/*
 * Import data into GPDB.
 */
Datum s3_import(PG_FUNCTION_ARGS) {
    S3ExtBase *myData;
    char *data;
    int data_len;
    size_t nread = 0;

    /* Must be called via the external table format manager */
    if (!CALLED_AS_EXTPROTOCOL(fcinfo))
        elog(ERROR,
             "extprotocol_import: not called by external protocol manager");

    /* Get our internal description of the protocol */
    myData = (S3ExtBase *)EXTPROTOCOL_GET_USER_CTX(fcinfo);

    if (EXTPROTOCOL_IS_LAST_CALL(fcinfo)) {
        if (myData) {
            if (!myData->Destroy()) {
                ereport(ERROR, (0, errmsg("Cleanup S3 extention failed")));
            }
            delete myData;
        }
        PG_RETURN_INT32(0);
    }

    if (myData == NULL) {
        /* first call. do any desired init */

        const char *p_name = "s3";
        char *url_with_options = EXTPROTOCOL_GET_URL(fcinfo);

        // truncate url
        const char *delimiter = " ";
        char *options = strstr(url_with_options, delimiter);
        int url_len = strlen(url_with_options);
        if (options) {
            url_len = strlen(url_with_options) - strlen(options);
        }
        char url[url_len + 1];
        memcpy(url, url_with_options, url_len);
        url[url_len] = 0;

        char *config_path = get_opt_s3(options, "config");
        if (!config_path) {
            // no config path in url, use default value
            // data_folder/gpseg0/s3/s3.conf
            config_path = strdup("./s3/s3.conf");
        }

        bool result = InitConfig(config_path, NULL);
        if (!result) {
            ereport(ERROR,
                    (0, errmsg("Can't find config file %s", config_path)));
            free(config_path);
        } else {
            ClearConfig();
            free(config_path);
        }

        InitLog();

        if (s3ext_accessid == "") {
            ereport(ERROR, (0, errmsg("ERROR: access id is empty")));
        }

        if (s3ext_secret == "") {
            ereport(ERROR, (0, errmsg("ERROR: secret is empty")));
        }

        if ((s3ext_segnum == -1) || (s3ext_segid == -1)) {
            ereport(ERROR, (0, errmsg("ERROR: segment id is invalid")));
        }

        myData = CreateExtWrapper(url);

        if (!myData ||
            !myData->Init(s3ext_segid, s3ext_segnum, s3ext_chunksize)) {
            if (myData) delete myData;
            ereport(ERROR,
                    (0, errmsg("Failed to init S3 extension, segid = %d, "
                               "segnum = %d, please check your net connection",
                               s3ext_segid, s3ext_segnum)));
        }
        /*
                  if(strcasecmp(parsed_url->protocol, p_name) != 0) {
                  elog(ERROR, "internal error: s3prot called with a different
                  protocol
                  (%s)",
                  parsed_url->protocol);
                  }
        */

        EXTPROTOCOL_SET_USER_CTX(fcinfo, myData);
    }

    /* =======================================================================
     *                            DO THE IMPORT
     * =======================================================================
     */

    data = EXTPROTOCOL_GET_DATABUF(fcinfo);
    data_len = EXTPROTOCOL_GET_DATALEN(fcinfo);
    uint64_t readlen = 0;
    if (data_len > 0) {
        readlen = data_len;
        if (!myData->TransferData(data, readlen))
            ereport(ERROR, (0, errmsg("s3_import: could not read data")));
        nread = (size_t)readlen;
        // S3DEBUG("read %d data from S3", nread);
    }

    PG_RETURN_INT32((int)nread);
}

/*
 * Export data out of GPDB.
 */
Datum s3_export(PG_FUNCTION_ARGS) { PG_RETURN_INT32(0); }

Datum s3_validate_urls(PG_FUNCTION_ARGS) {
    int nurls;
    int i;
    ValidatorDirection direction;
    PG_RETURN_VOID();
}
