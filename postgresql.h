#ifndef POSTGRESQL_H_
#define POSTGRESQL_H_ 1

#ifdef __cplusplus
extern "C" {
#endif

void SQL_Init(const char *connect_string);

int SQL_Query(const char *query, ...);

unsigned int SQL_RowCount();

const char *SQL_Value(unsigned int row, unsigned int column);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* !POSTGRESQL_H_ */
