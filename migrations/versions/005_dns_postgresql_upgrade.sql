CREATE TABLE dns_entries(
    id      SERIAL        PRIMARY KEY,
    account INT           NOT NULL REFERENCES accounts,
    fqdn    VARCHAR(100)  NOT NULL UNIQUE,
    ip4     CIDR
    check (ip4 IS NULL OR masklen(ip4) = 32),
    ip6     CIDR
    check (ip6 IS NULL OR masklen(ip6) = 128),
    cname   VARCHAR(100)
    check ((cname IS NOT NULL AND ip4 IS NULL AND ip6 IS NULL) OR cname IS NULL)
);

select audit.audit_table('dns_entries', true, false, ARRAY[]::text[], ARRAY['p2k12.account']::text[]);

GRANT ALL PRIVILEGES ON dns_entries to p2k12_pos;
GRANT ALL PRIVILEGES ON dns_entries_id_seq to p2k12_pos;

CREATE OR REPLACE VIEW pretty_dns_entries as
SELECT
    dn.id,
    account,
    a.name as account_name,
    fqdn,
    substr(fqdn, 0, strpos(fqdn, '.')) as host,
    substr(fqdn, strpos(fqdn, '.') + 1) as zone,
    host(ip4) as ip4,
    host(ip6) as ip6,
    cname
from
    dns_entries dn
inner join accounts a on dn.account=a.id;
GRANT SELECT ON pretty_dns_entries to p2k12_pos;
