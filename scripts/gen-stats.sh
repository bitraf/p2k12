#!/bin/bash

set -e
set -o pipefail

psql -X -q 'dbname=p2k12' -c "COPY (WITH input AS  (SELECT date, amount FROM transaction_lines JOIN transactions ON (transaction = transactions.id) WHERE debit_account IN (SELECT id FROM accounts WHERE type = 'user') AND credit_account IN (SELECT id FROM accounts WHERE type = 'product') ORDER BY date ASC) SELECT EXTRACT(epoch FROM date), SUM(amount) OVER (ORDER BY date) FROM input ORDER BY date) TO STDOUT;" > /home/webapps/live/charts/data/purchases.txt

psql -X -q 'dbname=p2k12' -c "COPY (WITH input AS  (SELECT date, amount FROM transaction_lines JOIN transactions ON (transaction = transactions.id) WHERE debit_account IN (SELECT id FROM accounts WHERE name = 'deficit') ORDER BY date ASC) SELECT EXTRACT(epoch FROM date), SUM(amount) OVER (ORDER BY date) FROM input ORDER BY date) TO STDOUT;" > /home/webapps/live/charts/data/deficit.txt

./gen-stats.gnuplot
