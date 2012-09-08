#!/usr/bin/env php
<?
require_once('Mail.php');
require_once('Mail/mime.php');
require_once('smtp-config.php');
setlocale (LC_ALL, 'nb_NO.UTF-8');
date_default_timezone_set ('Europe/Oslo');

$today = ltrim(strftime('%e. %B %Y'));

$secret = file_get_contents('/var/lib/p2k12/secret');

if (false === pg_connect("dbname=p2k12 user=p2k12"))
{
  echo "Drats!\n";

  exit;
}

if (false === ($member_res = pg_query("SELECT am.account, mt.price AS monthly_dues, am.full_name, am.email FROM active_members am INNER JOIN membership_infos mt ON mt.name = am.type WHERE am.account IN (SELECT account FROM members WHERE date > (SELECT MAX(time) FROM billing_runs) AND account NOT IN (SELECT account FROM members WHERE date <= (SELECT MAX(time) FROM billing_runs))) AND mt.recurrence = '1 month'::INTERVAL ORDER BY am.full_name")))
{
  echo "Bollocks!\n";

  exit;
}

$now = time();

$smtp = Mail::factory('smtp', array ('host' => $smtp_host, 'auth' => true, 'username' => $smtp_user, 'password' => $smtp_password));

$period = strftime('%B');

while ($member = pg_fetch_assoc($member_res))
{
  $signature = substr(hash_hmac('sha256', $member['account'], $secret), 0, 8);

  $body = <<<MSG
Dato: $today
Medlemskap, $period

Betalingsinformasjon:

  {$member['full_name']}

Mottaker:
 
  Bitraf
  Darres gate 24
  0175 OSLO

Beløp:

  {$member['monthly_dues']} kr

Kontonummer:

  1503 273 5581

Hvis dette scriptet har rett, er dette din første medlemsfaktura, så velkommen 
til Bitraf!  Takk for at du betaler medlemsavgift!  Bitraf er avhengig av 
medlemsavgift for å betale husleie og å kjøpe nødvendig utstyr.

For å se detaljer om ditt medlemskap, gå til

  http://p2k12.bitraf.no/my/{$member['account']}/{$signature}/
MSG
  ;

  $headers = array ('Subject' => "[Bitraf] Medlemskap, $period");

  $headers['From'] = "Bitraf <post@bitraf.no>";
  $headers['To'] = "{$member['full_name']} <{$member['email']}>";
  $headers['Content-Transfer-Encoding'] = '8bit';
  $headers['Content-Type'] = 'text/plain; charset="UTF-8"';

  $message = new Mail_mime("\n");
  $message->setTXTBody($body);
  $body = $message->get(array('text_charset' => 'utf-8', 'head_charset' => 'utf-8'));

  $headers = $message->headers($headers);

  echo "** {$member['full_name']}\n";
  /*
  $mail_result = $smtp->send($member['email'], $headers, $body);
   */
}

pg_query('INSERT INTO billing_runs DEFAULT VALUES;');
