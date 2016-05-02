#ifndef POSTGRESQL_H_
#define POSTGRESQL_H_ 1

#ifdef __cplusplus
extern "C" {
#endif

void SQL_Init(const char *connect_string);

void SQL_SetP2k12Account(const char *account);

int SQL_Query(const char *query, ...)
    __attribute__ ((format (printf, 1, 2)));

int SQL_RowCount();

const char *SQL_Value(unsigned int row, unsigned int column);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* !POSTGRESQL_H_ */
