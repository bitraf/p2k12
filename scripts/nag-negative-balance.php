#!/usr/bin/env php
<?
require_once('Mail.php');
setlocale (LC_ALL, 'nb_NO.UTF-8');
date_default_timezone_set ('Europe/Oslo');

if (false === $dbcon = pg_connect("dbname=p2k12 user=p2k12"))
  exit;

$res = pg_query(<<<SQL
SELECT ub.id, ub.balance, full_Name, email, name FROM user_balances ub INNER JOIN active_members m ON m.account = ub.id WHERE balance > 0;
SQL
  );



if (false === $res)
  exit;

while ($row = pg_fetch_assoc($res))
{
  $recipient = $row['email'];
  $subject = "p2k12: Du har negativ pengebeholdning";
  $member_price = pg_query_params($dbcon, "SELECT price FROM active_members WHERE account=$1", array($row['id'])); 
if ($member_price)
  {
    $member_price = pg_fetch_assoc($member_price);
    $member_price = $member_price['price'];
  }
  else
  {
    echo "Failed to get membership price.";
    exit;
  }

  if ( ($row['balance'] < 1000 && $member_price >= 300) || ($row['balance'] < 100 && $member_price < 300) ) {
    $approach = "Du kan for eksempel løse dette ved å handle ting \nog putte dem inn i kjøleskapet, eller ved å sette";
  } 
  else {
    $approach = "Du må betale nå, fordi du har brukt mer\nkreditt enn vi tillater, sett";
  }

  $body = <<<MSG
Hei!

Du skylder p2k12 kr {$row['balance']}.  Dette skyldes at du har handlet for mer 
penger enn du har satt inn.  {$approach} inn penger på kontonummer

  0539 59 45248

som tilhører

  Alexander Alemayhu
  Vetlandsveien 41
  0671 OSLO

med betalingsinformasjon

  {$row['name']}

Selv om du betaler nå risikerer du å få denne meldingen fler ganger fordi 
mottakerkontoen ikke blir sjekket.  Ikke klag før tredje gang.

Beste hilsen,
/usr/local/share/p2k12/nag-negative-balance.php
MSG
  ;

  $smtp = Mail::factory('smtp', array ('host' => "smtp.webfaction.com", 'auth' => true, 'username' => "", 'password' => ""));

  $headers = array ('From' => "Bitraf <post@bitraf.no>", 'To' => $recipient, 'Subject' => $subject, 'Content-Transfer-Encoding' => '8bit', 'Content-Type' => 'text/plain; charset="UTF-8"');

  $mail_result = $smtp->send($recipient, $headers, $body);

  if ($mail_result !== true)
  {
    echo "Mislyktes i å sende e-post til $recipient\n";

    break;
  }
}
