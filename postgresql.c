#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <err.h>
#include <syslog.h>

#include <postgresql/libpq-fe.h>

#include "postgresql.h"

static PGconn *pg; /* Database connection handle */
static PGresult *pgresult;
static int tuple_count;

void SQL_Init(const char *connect_string)
{
	pg = PQconnectdb(connect_string);

	if (PQstatus(pg) != CONNECTION_OK)
		errx(EXIT_FAILURE, "PostgreSQL connection failed: %s", PQerrorMessage(pg));
}

int SQL_Query(const char *fmt, ...)
{
	char query[4096];
	static char numbufs[10][128];
	const char *args[10];
	int lengths[10];
	int formats[10];
	const char *c;
	int argcount = 0;
	va_list ap;
	int rowsaffected, is_size_t = 0;

	char *o, *end;

	o = query;
	end = o + sizeof(query);

	if (pgresult)
	{
		PQclear(pgresult);
		pgresult = 0;
	}

	va_start(ap, fmt);

	for (c = fmt; *c; )
	{
		switch (*c)
		{
		case '%':

			is_size_t = 0;

			++c;

			if (*c == 'z')
			{
				is_size_t = 1;
				++c;
			}

			switch (*c)
			{
			case 's':

				args[argcount] = va_arg(ap, const char*);
				lengths[argcount] = strlen(args[argcount]);
				formats[argcount] = 0;

				break;

			case 'd':

				snprintf(numbufs[argcount], 127, "%d", va_arg(ap, int));
				args[argcount] = numbufs[argcount];
				lengths[argcount] = strlen(args[argcount]);
				formats[argcount] = 0;

				break;

			case 'u':

				if (is_size_t)
					snprintf(numbufs[argcount], 127, "%llu", (unsigned long long) va_arg(ap, size_t));
				else
					snprintf(numbufs[argcount], 127, "%u", va_arg(ap, unsigned int));
				args[argcount] = numbufs[argcount];
				lengths[argcount] = strlen(args[argcount]);
				formats[argcount] = 0;

				break;

			case 'l':

				snprintf(numbufs[argcount], 127, "%lld", va_arg(ap, long long));
				args[argcount] = numbufs[argcount];
				lengths[argcount] = strlen(args[argcount]);
				formats[argcount] = 0;

				break;

			case 'f':

				snprintf(numbufs[argcount], 127, "%f", va_arg(ap, double));
				args[argcount] = numbufs[argcount];
				lengths[argcount] = strlen(args[argcount]);
				formats[argcount] = 0;

				break;

			case 'B':

				args[argcount] = va_arg(ap, const char *);
				lengths[argcount] = va_arg(ap, size_t);
				formats[argcount] = 1;

				break;

			default:

				assert(!"unknown format character");

				return -1;
			}

			++c;
			++argcount;

			assert(o + 3 < end);

			*o++ = '$';
			if (argcount >= 10)
				*o++ = '0' + (argcount / 10);
			*o++ = '0' + (argcount % 10);

			break;

		default:

			assert(o + 1 < end);

			*o++ = *c++;
		}
	}

	va_end(ap);

	*o = 0;

	for (;;)
	{
		pgresult = PQexecParams(pg, query, argcount, 0, args, lengths, formats, 0);

		if (PQresultStatus(pgresult) != PGRES_FATAL_ERROR)
			break;

		PQclear(pgresult);
		pgresult = 0;

		printf ("PostgreSQL query failed: %s", PQerrorMessage(pg));

		if (PQstatus(pg) != CONNECTION_OK)
		{
			syslog(LOG_INFO, "Resetting database connection");

			for (;;)
			{
				PQreset(pg);

				if (PQstatus(pg) == CONNECTION_OK)
					break;

				usleep (1000000);
			}

			syslog(LOG_INFO, "Database connection OK");

			continue;
		}

		return -1;
	}

	tuple_count = PQntuples(pgresult);
	rowsaffected = strtol(PQcmdTuples(pgresult), 0, 0);

	return rowsaffected;
}

unsigned int SQL_RowCount()
{
	return tuple_count;
}

const char *SQL_Value(unsigned int row, unsigned int column)
{
	return PQgetvalue(pgresult, row, column);
}
