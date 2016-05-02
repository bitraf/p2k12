CREATE OR REPLACE FUNCTION p2k12_create_member(
    account_name TEXT,
    full_name TEXT,
    email TEXT
    ) RETURNS INT AS $$
DECLARE
  account_id INTEGER = (SELECT id FROM accounts WHERE name = account_name);
BEGIN
  IF account_id IS NOT NULL
  THEN
    RAISE 'p2k12_username_exists'
      USING HINT = 'The username is already is use';
  END IF;

  INSERT INTO accounts(name, type)
  VALUES(account_name, 'user')
  RETURNING id INTO account_id;

  INSERT INTO MEMBERS(full_name, email, account, price)
  VALUES(full_name, email, account_id, 0);

  INSERT INTO checkins(account) VALUES(account_id);

  RETURN account_id;
END;
$$
LANGUAGE 'plpgsql'
SECURITY DEFINER;

CREATE OR REPLACE FUNCTION p2k12_become_member(
    account_id INT,
    price INT
    ) RETURNS VOID AS $$
BEGIN
  INSERT INTO members (full_name, email, price, account)
  SELECT full_name, email, price, account
  FROM members
  WHERE account = account_id
  ORDER BY date DESC
  LIMIT 1;
END;
$$
LANGUAGE 'plpgsql'
SECURITY DEFINER;

REVOKE
  INSERT, UPDATE, DELETE, TRUNCATE, REFERENCES, TRIGGER
ON
  members,
  accounts
FROM p2k12_pos;
