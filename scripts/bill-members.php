#!/usr/bin/env php
<?
require_once('Mail.php');
require_once('Mail/mime.php');
require_once('smtp-config.php');
setlocale (LC_ALL, 'nb_NO.UTF-8');
date_default_timezone_set ('Europe/Oslo');

$today = ltrim(strftime('%e. %B %Y'));

if (false === pg_connect("dbname=p2k12 user=p2k12"))
  exit;

$member_res = pg_query("SELECT account, type, full_name, email FROM active_members WHERE type IN ('aktiv', 'støtte', 'filantrop') ORDER BY full_name");

$now = time();

$smtp = Mail::factory('smtp', array ('host' => $smtp_host, 'auth' => true, 'username' => $smtp_user, 'password' => $smtp_password));

while ($member = pg_fetch_assoc($member_res))
{
  $join_date_res = pg_query_params('SELECT EXTRACT(EPOCH FROM MIN(date)) FROM members WHERE account = $1', array($member['account']));
  $join_date = pg_fetch_result($join_date_res, 0, 0);
  $join_date_pretty = ltrim(strftime('%e. %B %Y', $join_date));

  $invoice_count_res = pg_query_params('SELECT COUNT(*) FROM member_invoices WHERE account = $1', array($member['account']));
  $invoice_count = pg_fetch_result($invoice_count_res, 0, 0);

  $pay_count_res = pg_query_params('SELECT COUNT(*) FROM payments WHERE account = $1 AND paid_date IS NOT NULL', array($member['account']));
  $pay_count = pg_fetch_result($pay_count_res, 0, 0);

  $period_res = pg_query_params("SELECT EXTRACT(EPOCH FROM 'epoch'::TIMESTAMP + $1 * '1 second'::INTERVAL + $2 * '1 month'::INTERVAL),
                                        EXTRACT(EPOCH FROM 'epoch'::TIMESTAMP + $1 * '1 second'::INTERVAL + ($2 + 1) * '1 month'::INTERVAL - '1 day'::INTERVAL)", array($join_date, $pay_count));
  $period_from = pg_fetch_result($period_res, 0, 0);
  $period_to = pg_fetch_result($period_res, 0, 1);

  if ($period_from > $now)
  {
    echo "++ {$member['full_name']} already paid\n";
    continue;
  }

  if ($period_to < $now && $invoice_count > 0)
  {
    echo "** {$member['full_name']} did not pay last period\n";

    /*continue;*/
  }

  $previous_invoice_res = pg_query_params("SELECT EXTRACT(EPOCH FROM pay_by) FROM member_invoices WHERE account = $1 AND date >= 'epoch'::TIMESTAMP + $2 * '1 second'::INTERVAL ORDER BY date DESC", array($member['account'], $period_from));
  if (pg_num_rows($previous_invoice_res))
    $previous_invoice = pg_fetch_result($previous_invoice_res, 0, 0);
  else
    $previous_invoice = false;

  if ($previous_invoice !== false)
  {
    if (pg_num_rows($previous_invoice_res) > 1)
    {
      echo "-- {$member['full_name']} already billed twice\n";

      continue;
    }
    else if ($previous_invoice < $now - 3 * 86400)
    {
      echo "-- {$member['full_name']} already billed, did not pay\n";

      continue;
    }
    else
    {
      echo "   {$member['full_name']} already billed\n";

      continue;
    }
  }
  else
    echo "%% {$member['full_name']} billing now\n";

  $pay_by = $now + 7 * 86400;

  if ($pay_by > $period_to)
    $pay_by = $period_to;

  $pay_by_pretty = strftime('%d.%m.%Y', $pay_by);

  if ($member['type'] == 'støtte')
    $amount = '300';
  elseif ($member['type'] == 'aktiv')
    $amount = '500';
  elseif ($member['type'] == 'filantrop')
    $amount = '1000';
  else
    continue;

  $period_from_pretty = ltrim(strftime('%e. %B', $period_from));
  $period_to_pretty = ltrim(strftime('%e. %B', $period_to));

  if ($previous_invoice === false)
  {
    $body = <<<MSG
Dato: $today
Medlemskap fra $period_from_pretty til $period_to_pretty

Betalingsfrist: $pay_by_pretty
Betalingsinformasjon: {$member['full_name']}
Mottaker: Bitraf, Darres gate 24, 0175 OSLO
Beløp: $amount kr
Kontonummer: 1503 273 5581

Takk for at du betaler medlemsavgift!  Bitraf er avhengig av medlemsavgift for 
å betale husleie og å kjøpe nødvendig utstyr.

Du ble registrert som medlem i Bitraf $join_date_pretty.
MSG
    ;

    $headers = array ('Subject' => "[Bitraf] Medlemskap fra $period_from_pretty til $period_to_pretty");
  }
  else
  {
    $pay_by = $now;
    $previous_invoice_pretty = ltrim(strftime('%e. %B', $previous_invoice));

    $headers = array ('Subject' => "[Bitraf] Påminnelse: Medlemskap fra $period_from_pretty til $period_to_pretty");

    $body = <<<MSG
Dette er en påminnelse for en tidligere faktura med
betalingsfrist $previous_invoice_pretty som vi ikke har registrert
noen betaling for.

Dato: $today
Medlemskap fra $period_from_pretty til $period_to_pretty

Betalingsfrist: snarest
Betalingsinformasjon: {$member['full_name']}
Mottaker: Bitraf, Darres gate 24, 0175 OSLO
Beløp: $amount kr
Kontonummer: 1503 273 5581

Takk for at du betaler medlemsavgift!  Bitraf er avhengig av medlemsavgift for 
å betale husleie og å kjøpe nødvendig utstyr.

Du ble registrert som medlem i Bitraf $join_date_pretty.  Hvis du ønsker å si 
opp medlemskapet ditt, kan du sende en e-post til post@bitraf.no.
MSG
    ;
  }

  $headers['From'] = "Bitraf <post@bitraf.no>";
  $headers['To'] = "{$member['full_name']} <{$member['email']}>";
  $headers['Content-Transfer-Encoding'] = '8bit';
  $headers['Content-Type'] = 'text/plain; charset="UTF-8"';

  $message = new Mail_mime("\n");
  $message->setTXTBody($body);
  $body = $message->get(array('text_charset' => 'utf-8', 'head_charset' => 'utf-8'));

  $headers = $message->headers($headers);

  /*
  $mail_result = $smtp->send($member['email'], $headers, $body);
  pg_query_params('INSERT INTO member_invoices (account, pay_by, amount) VALUES ($1, $2, $3)', array($member['account'], strftime('%Y-%m-%d', $pay_by), $amount));
   */
}
