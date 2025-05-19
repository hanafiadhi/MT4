#ifndef SQLITE_INTEGRATED_H
#define SQLITE_INTEGRATED_H

#ifdef _WIN32
#  define DLL_EXPORT __declspec(dllexport)
#else
#  define DLL_EXPORT
#endif

#include <sqlite3.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

// #pragma pack(push,1)
// Struktur untuk data trade dalam format binary
typedef struct {
    int32_t account;
    int32_t ticket;
    char symbol[32];
    char type[16];
    double lots;
    double open_price;
    double stop_loss;
    double take_profit;
    double profit;
    char open_time[24];
    char close_time[24];
} TradeData;
// #pragma pack(pop)


static char lastErrMsg[512] = {0};

// Struktur untuk data exposure dalam format binary
typedef struct {
    char snapshot_time[24];
    char currency[8];
    double amount;
    double rate_to_usd;
    double usd_value;
} ExposureData;

DLL_EXPORT bool __stdcall openDatabase(const char* dbName, sqlite3** outDb) {
    int rc = sqlite3_open(dbName, outDb);
    if (rc) {
        fprintf(stderr, "Error opening DB: %s\n", sqlite3_errmsg(*outDb));
        return false;
    }
    return true;
}

DLL_EXPORT bool __stdcall closeDatabase(sqlite3* db) {
    if(db == nullptr) return false;
    int rc = sqlite3_close(db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to close DB: %s\n", sqlite3_errmsg(db));
        return false;
    }
    return true;
}

DLL_EXPORT bool __stdcall createTable(sqlite3* db) {
    const char* sql =
        // Tabel trades
        "CREATE TABLE IF NOT EXISTS trades ("
        " account       INTEGER NOT NULL,"
        " ticket        INTEGER PRIMARY KEY,"
        " symbol        TEXT    NOT NULL,"
        " type          TEXT    NOT NULL,"
        " lots          REAL    NOT NULL,"
        " open_price    REAL    NOT NULL,"
        " stop_loss     REAL    NOT NULL,"
        " take_profit   REAL    NOT NULL,"
        " profit        REAL    NOT NULL,"
        " open_time     TEXT    NOT NULL,"
        " close_time    TEXT"  // boleh NULL
        ");"

        // Tabel exposure_log
        "CREATE TABLE IF NOT EXISTS exposure_log ("
        " id             INTEGER PRIMARY KEY AUTOINCREMENT,"
        " snapshot_time  TEXT    NOT NULL,"
        " currency       TEXT    NOT NULL,"
        " amount         REAL    NOT NULL,"
        " rate_to_usd    REAL    NOT NULL,"
        " usd_value      REAL    NOT NULL,"
        " UNIQUE(currency)"
        ");";

    char* err = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to create table: %s\n", err);
        sqlite3_free(err);
        return false;
    }
    return true;
}


DLL_EXPORT bool __stdcall upsertTradeBinary(sqlite3* db, const void* binaryData, size_t dataSize) {
    if (!binaryData || dataSize % sizeof(TradeData) != 0) {
        return false;
    }

    const char* sqlUpsert =
      "INSERT INTO trades "
      "(account,ticket,symbol,type,lots,open_price,stop_loss,take_profit,profit,open_time,close_time) "
      "VALUES (?,?,?,?,?,?,?,?,?,?,?) "
      "ON CONFLICT(ticket) DO UPDATE SET "
      "  account     = excluded.account,"
      "  symbol      = excluded.symbol,"
      "  type        = excluded.type,"
      "  lots        = excluded.lots,"
      "  open_price  = excluded.open_price,"
      "  stop_loss   = excluded.stop_loss,"
      "  take_profit = excluded.take_profit,"
      "  profit      = excluded.profit,"
      "  open_time   = excluded.open_time,"
      "  close_time  = excluded.close_time;";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(db, sqlUpsert, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        return false;
    }

    // Begin transaction untuk kinerja
    sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, NULL);

    const TradeData* trades = (const TradeData*)binaryData;
    size_t tradeCount = dataSize / sizeof(TradeData);

    for (size_t i = 0; i < tradeCount; i++) {
        const TradeData* trade = &trades[i];

        sqlite3_bind_int(stmt,    1, trade->account);
        sqlite3_bind_int(stmt,    2, trade->ticket);
        sqlite3_bind_text(stmt,   3, trade->symbol,    -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt,   4, trade->type,      -1, SQLITE_STATIC);
        sqlite3_bind_double(stmt, 5, trade->lots);
        sqlite3_bind_double(stmt, 6, trade->open_price);
        sqlite3_bind_double(stmt, 7, trade->stop_loss);
        sqlite3_bind_double(stmt, 8, trade->take_profit);
        sqlite3_bind_double(stmt, 9, trade->profit);
        sqlite3_bind_text(stmt,  10, trade->open_time, -1, SQLITE_STATIC);

        if (trade->close_time[0] != '\0')
            sqlite3_bind_text(stmt, 11, trade->close_time, -1, SQLITE_STATIC);
        else
            sqlite3_bind_null(stmt, 11);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
            return false;
        }
        sqlite3_reset(stmt);
    }

    sqlite3_finalize(stmt);
    sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
    return true;
}


// Upsert trade data dalam format binary
// DLL_EXPORT bool __stdcall upsertTradeBinary(sqlite3* db, const void* binaryData, size_t dataSize) {
//     if (!binaryData || dataSize % sizeof(TradeData) != 0) {
//         fprintf(stderr, "Invalid binary data size\n");
//         return false;
//     }

//     const char* sqlUpsert =
//       "INSERT INTO trades "
//       "(account,ticket,symbol,type,lots,open_price,stop_loss,take_profit,profit,open_time,close_time) "
//       "VALUES (?,?,?,?,?,?,?,?,?,?,?) "
//       "ON CONFLICT(ticket) DO UPDATE SET "
//       "  account     = excluded.account,"
//       "  symbol      = excluded.symbol,"
//       "  type        = excluded.type,"
//       "  lots        = excluded.lots,"
//       "  open_price  = excluded.open_price,"
//       "  stop_loss   = excluded.stop_loss,"
//       "  take_profit = excluded.take_profit,"
//       "  profit      = excluded.profit,"
//       "  open_time   = excluded.open_time,"
//       "  close_time  = excluded.close_time;";

//     sqlite3_stmt* stmt;
//     if(sqlite3_prepare_v2(db, sqlUpsert, -1, &stmt, NULL) != SQLITE_OK) {
//         return false;
//     }

//     // Begin transaction untuk kinerja
//     sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, NULL);

//     const TradeData* trades = (const TradeData*)binaryData;
//     size_t tradeCount = dataSize / sizeof(TradeData);

//     for (size_t i = 0; i < tradeCount; i++) {
//         const TradeData* trade = &trades[i];

//         sqlite3_bind_int(stmt,    1, trade->account);
//         sqlite3_bind_int(stmt,    2, trade->ticket);
//         sqlite3_bind_text(stmt,   3, trade->symbol,    -1, SQLITE_STATIC);
//         sqlite3_bind_text(stmt,   4, trade->type,      -1, SQLITE_STATIC);
//         sqlite3_bind_double(stmt, 5, trade->lots);
//         sqlite3_bind_double(stmt, 6, trade->open_price);
//         sqlite3_bind_double(stmt, 7, trade->stop_loss);
//         sqlite3_bind_double(stmt, 8, trade->take_profit);
//         sqlite3_bind_double(stmt, 9, trade->profit);
//         sqlite3_bind_text(stmt,  10, trade->open_time, -1, SQLITE_STATIC);

//         if (trade->close_time[0] != '\0')
//             sqlite3_bind_text(stmt, 11, trade->close_time, -1, SQLITE_STATIC);
//         else
//             sqlite3_bind_null(stmt, 11);

//         if (sqlite3_step(stmt) != SQLITE_DONE) {
//             sqlite3_finalize(stmt);
//             sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
//             return false;
//         }
//         sqlite3_reset(stmt);
//     }

//     sqlite3_finalize(stmt);
//     sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
//     return true;
// }

// Hapus seluruh data di exposure_log
DLL_EXPORT bool __stdcall clearExposureLog(sqlite3* db) {
    const char* sql = "DELETE FROM exposure_log;";
    char* err = nullptr;
    int rc = sqlite3_exec(db, sql, nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to clear exposure_log: %s\n", err);
        sqlite3_free(err);
        return false;
    }
    return true;
}

// Insert exposure data dalam format binary
DLL_EXPORT bool __stdcall insertExposureBinary(sqlite3* db, const void* binaryData, size_t dataSize) {
    if (!binaryData || dataSize % sizeof(ExposureData) != 0) {
        fprintf(stderr, "Invalid exposure binary data size\n");
        return false;
    }

    const char* sqlIns =
        "INSERT INTO exposure_log "
        "(snapshot_time, currency, amount, rate_to_usd, usd_value) "
        "VALUES (?, ?, ?, ?, ?) "
        "ON CONFLICT(currency) DO UPDATE SET "
        "snapshot_time = excluded.snapshot_time, "
        "amount = excluded.amount, "
        "rate_to_usd = excluded.rate_to_usd, "
        "usd_value = excluded.usd_value;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db, sqlIns, -1, &stmt, NULL) != SQLITE_OK) {
        return false;
    }

    // Begin transaction
    sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, NULL);

    const ExposureData* exposures = (const ExposureData*)binaryData;
    size_t exposureCount = dataSize / sizeof(ExposureData);

    for (size_t i = 0; i < exposureCount; i++) {
        const ExposureData* exposure = &exposures[i];

        sqlite3_bind_text  (stmt, 1, exposure->snapshot_time, -1, SQLITE_STATIC);
        sqlite3_bind_text  (stmt, 2, exposure->currency,      -1, SQLITE_STATIC);
        sqlite3_bind_double(stmt, 3, exposure->amount);
        sqlite3_bind_double(stmt, 4, exposure->rate_to_usd);
        sqlite3_bind_double(stmt, 5, exposure->usd_value);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
            return false;
        }
        sqlite3_reset(stmt);
    }

    sqlite3_finalize(stmt);
    sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
    return true;
}

// Fungsi helper untuk membuat TradeData dari informasi trade
DLL_EXPORT void __stdcall createTradeData(
    int32_t account, int32_t ticket,
    const char* symbol, const char* type,
    double lots, double open_price, double stop_loss, double take_profit, double profit,
    const char* open_time, const char* close_time,
    TradeData* outTrade) {

    if (!outTrade) return;

    outTrade->account = account;
    outTrade->ticket = ticket;
    strncpy(outTrade->symbol, symbol, sizeof(outTrade->symbol) - 1);
    outTrade->symbol[sizeof(outTrade->symbol) - 1] = '\0';

    strncpy(outTrade->type, type, sizeof(outTrade->type) - 1);
    outTrade->type[sizeof(outTrade->type) - 1] = '\0';

    outTrade->lots = lots;
    outTrade->open_price = open_price;
    outTrade->stop_loss = stop_loss;
    outTrade->take_profit = take_profit;
    outTrade->profit = profit;

    strncpy(outTrade->open_time, open_time, sizeof(outTrade->open_time) - 1);
    outTrade->open_time[sizeof(outTrade->open_time) - 1] = '\0';

    if (close_time && *close_time) {
        strncpy(outTrade->close_time, close_time, sizeof(outTrade->close_time) - 1);
        outTrade->close_time[sizeof(outTrade->close_time) - 1] = '\0';
    } else {
        outTrade->close_time[0] = '\0';  // Empty string for NULL
    }
}

// Fungsi helper untuk membuat ExposureData
DLL_EXPORT void __stdcall createExposureData(
    const char* snapshot_time, const char* currency,
    double amount, double rate_to_usd, double usd_value,
    ExposureData* outExposure) {

    if (!outExposure) return;

    strncpy(outExposure->snapshot_time, snapshot_time, sizeof(outExposure->snapshot_time) - 1);
    outExposure->snapshot_time[sizeof(outExposure->snapshot_time) - 1] = '\0';

    strncpy(outExposure->currency, currency, sizeof(outExposure->currency) - 1);
    outExposure->currency[sizeof(outExposure->currency) - 1] = '\0';

    outExposure->amount = amount;
    outExposure->rate_to_usd = rate_to_usd;
    outExposure->usd_value = usd_value;
}

// Untuk backward compatibility - masih menyediakan fungsi CSV
// namun mengkonversi ke binary di belakang layar
DLL_EXPORT bool __stdcall insertTradeBulk(sqlite3* db, const char* csvData) {
    if (!csvData) return false;

    // Hitung jumlah baris
    size_t lineCount = 0;
    const char* p = csvData;
    while (*p) {
        if (*p == '\n') lineCount++;
        p++;
    }
    if (lineCount == 0) return true; // No data

    TradeData* trades = (TradeData*)malloc(lineCount * sizeof(TradeData));
    if (!trades) return false;

    // Duplicate input buffer
    char* dup = _strdup(csvData);
    if (!dup) {
        free(trades);
        return false;
    }

    size_t tradeIndex = 0;
    char* line = strtok(dup, "\n");
    while (line) {
        int    account, ticket;
        char   symbol[64], type[64], open_time[32], close_time[32];
        double lots, open_price, stop_loss, take_profit, profit;

        int n = sscanf(
            line,
            "%d,%d,%63[^,],%63[^,],%lf,%lf,%lf,%lf,%lf,%31[^,],%31[^\n]",
            &account, &ticket, symbol, type,
            &lots, &open_price, &stop_loss, &take_profit, &profit,
            open_time, close_time
        );

        if (n >= 10) {  // close_time may be empty
            TradeData* trade = &trades[tradeIndex++];
            createTradeData(
                account, ticket, symbol, type, lots, open_price, stop_loss, take_profit,
                profit, open_time, (n == 11) ? close_time : "", trade
            );
        }

        line = strtok(NULL, "\n");
    }

    free(dup);

    // Call binary version with converted data
    bool result = upsertTradeBinary(db, trades, tradeIndex * sizeof(TradeData));
    free(trades);
    return result;
}

// Backward compatibility untuk upsertTradeBulk
DLL_EXPORT bool __stdcall upsertTradeBulk(sqlite3* db, const char* csvData) {
    if (!csvData) return false;

    // Hitung jumlah baris
    size_t lineCount = 0;
    const char* p = csvData;
    while (*p) {
        if (*p == '\n') lineCount++;
        p++;
    }
    if (lineCount == 0) return true; // No data

    TradeData* trades = (TradeData*)malloc(lineCount * sizeof(TradeData));
    if (!trades) return false;

    // Duplicate input buffer
    char* dup = _strdup(csvData);
    if (!dup) {
        free(trades);
        return false;
    }

    size_t tradeIndex = 0;
    for (char* line = strtok(dup, "\n"); line; line = strtok(NULL, "\n")) {
        int    account, ticket;
        char   symbol[64], type[64], open_time[32], close_time[32];
        double lots, op, sl, tp, profit;

        int n = sscanf(
            line,
            "%d,%d,%63[^,],%63[^,],%lf,%lf,%lf,%lf,%lf,%31[^,],%31[^\n]",
            &account, &ticket, symbol, type,
            &lots, &op, &sl, &tp, &profit,
            open_time, close_time
        );

        if (n >= 10) {  // close_time may be empty
            TradeData* trade = &trades[tradeIndex++];
            createTradeData(
                account, ticket, symbol, type, lots, op, sl, tp,
                profit, open_time, (n == 11) ? close_time : "", trade
            );
        }
    }

    free(dup);

    // Call binary version with converted data
    bool result = upsertTradeBinary(db, trades, tradeIndex * sizeof(TradeData));
    free(trades);
    return result;
}

// Backward compatibility untuk insertExposureBulk
DLL_EXPORT bool __stdcall insertExposureBulk(sqlite3* db, const char* csvData) {
    if (!csvData) return false;

    // Hitung jumlah baris
    size_t lineCount = 0;
    const char* p = csvData;
    while (*p) {
        if (*p == '\n') lineCount++;
        p++;
    }
    if (lineCount == 0) return true; // No data

    ExposureData* exposures = (ExposureData*)malloc(lineCount * sizeof(ExposureData));
    if (!exposures) return false;

    // Duplicate buffer
    char* dup = _strdup(csvData);
    if (!dup) {
        free(exposures);
        return false;
    }

    size_t exposureIndex = 0;
    char* line = strtok(dup, "\n");
    while (line) {
        char   snapshot_time[32];
        char   currency[16];
        double amount, rate_to_usd, usd_value;

        int n = sscanf(
            line,
            "%31[^,],%15[^,],%lf,%lf,%lf",
            snapshot_time, currency,
            &amount, &rate_to_usd, &usd_value
        );

        if (n == 5) {
            ExposureData* exposure = &exposures[exposureIndex++];
            createExposureData(
                snapshot_time, currency, amount, rate_to_usd, usd_value, exposure
            );
        }

        line = strtok(NULL, "\n");
    }

    free(dup);

    // Call binary version with converted data
    bool result = insertExposureBinary(db, exposures, exposureIndex * sizeof(ExposureData));
    free(exposures);
    return result;
}

#endif  // SQLITE_INTEGRATED_H
