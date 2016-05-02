select audit.audit_table('accounts', true, false, ARRAY[]::text[], ARRAY['p2k12.account']::text[]);
select audit.audit_table('members', true, false, ARRAY[]::text[], ARRAY['p2k12.account']::text[]);
select audit.audit_table('checkins', true, false, ARRAY[]::text[], ARRAY['p2k12.account']::text[]);
