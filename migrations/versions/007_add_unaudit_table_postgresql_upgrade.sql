CREATE OR REPLACE FUNCTION audit.unaudit_table(target_table regclass) RETURNS void AS $body$
BEGIN
  EXECUTE 'DROP TRIGGER IF EXISTS audit_trigger_row ON ' || quote_ident(target_table::TEXT);
  EXECUTE 'DROP TRIGGER IF EXISTS audit_trigger_stm ON ' || quote_ident(target_table::TEXT);
END;
$body$
LANGUAGE 'plpgsql';
