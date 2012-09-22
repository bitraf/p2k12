#!/usr/bin/env php
<?
require_once('Mail.php');
setlocale (LC_ALL, 'nb_NO.UTF-8');
date_default_timezone_set ('Europe/Oslo');

if (false === pg_connect("dbname=p2k12 user=p2k12 password=bghj37cfko467fj89dlbhz7thj"))
  exit;

$res = pg_query(<<<SQL
SELECT
    m.id,
    full_name,
    a.name AS user_name,
    email,
    m.type,
    pay_by,
    amount
  FROM member_invoices mi
  INNER JOIN active_members m ON m.id = mi.member
  INNER JOIN accounts a ON a.id = m.account
  WHERE m.type IN ('aktiv', 'filantrop', 'støtte') AND paid_date IS NULL AND pay_by < NOW();
SQL
  );

if (false === $res)
  exit;

while ($row = pg_fetch_assoc($res))
{
  $type = $row['type'];
  $amount = $row['amount'];

  $today = strftime('%Y-%m-%d');
  $full_name = $row['full_name'];
  $pay_by = $row['pay_by'];
  $info = $row['user_name'];

  $recipient = $row['email'];
  $subject = "[Bitraf] =?UTF-8?Q?P=C3=A5minnelse?=, medlemsavgift";
  $body = <<<MSG
Dato: $today
Medlemsavgift for $full_name

Dette er en påminnelse for en tidligere faktura med
betalingsfrist $pay_by som vi ikke har registrert
noen betaling for.

Betalingsinformasjon: $info
Betalingsfrist: snarest
Mottaker: Bitraf, Darres gate 24, 0175 Oslo
Beløp: $amount kr
Kontonummer: 15032735581

Hvis du ønsker å si opp medlemskapet i Bitraf, svar på denne e-posten.
MSG
    ;

  $smtp = Mail::factory('smtp', array ('host' => "smtp.webfaction.com", 'auth' => true, 'username' => "mortehu_social", 'password' => "c9b604e9"));

  $headers = array ('From' => "Bitraf <post@bitraf.no>", 'To' => $recipient, 'Subject' => $subject, 'Content-Transfer-Encoding' => '8bit', 'Content-Type' => 'text/plain; charset="UTF-8"');

  $mail_result = $smtp->send($recipient, $headers, $body);

  if ($mail_result !== true)
  {
    echo "Mislyktes i å sende e-post til $recipient\n";

    break;
  }
}
