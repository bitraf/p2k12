#!/usr/bin/env php
<?
setlocale (LC_ALL, 'nb_NO.UTF-8');
date_default_timezone_set ('Europe/Oslo');

if (false === pg_connect("dbname=p2k12 user=p2k12"))
  exit;

echo "SET DATESTYLE TO German;\n";
echo "SET TIMEZONE TO CET;\n";

$input = file_get_contents('Innbetalinger.txt');

$lines = array_filter(explode("\n", $input));
$items = array();
foreach ($lines as $line)
  $items[] = explode(";", trim($line));

echo "BEGIN;\n";

foreach ($items as $item)
{
  if ($item[8] != 'Ikke behandlet')
    continue;

  $account_res = pg_query_params("SELECT account FROM active_members WHERE LOWER(full_name) = LOWER($1) UNION SELECT account FROM account_aliases WHERE LOWER(alias) = LOWER($1)", array($item[0]));

  if (pg_num_rows($account_res) == 0)
  {
    echo "No match for {$item[0]}\n";

    continue;
  }
  else if (pg_num_rows($account_res) > 1)
  {
    echo "Several matches for {$item[0]}\n";

    continue;
  }
  else
  {
    $account = pg_fetch_result($account_res, 0, 0);

    $amount = str_replace('.', '', $item[5]);
    $amount = str_replace(',', '.', $amount);

    echo "INSERT INTO payments (paid_date, account, amount) VALUES ('{$item[4]}', {$account}, {$amount}); -- {$item[0]}\n";
  }
}

echo "COMMIT;\n";
