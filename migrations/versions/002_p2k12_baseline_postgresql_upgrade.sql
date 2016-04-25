SET search_path = public, pg_catalog;
--
-- PostgreSQL database dump
--

SET statement_timeout = 0;
SET lock_timeout = 0;
SET client_encoding = 'SQL_ASCII';
SET standard_conforming_strings = on;
SET check_function_bodies = false;
SET client_min_messages = warning;

--
-- Name: memberships; Type: SCHEMA; Schema: -; Owner: -
--

CREATE SCHEMA memberships;

--
-- Name: fuzzystrmatch; Type: EXTENSION; Schema: -; Owner: -
--

CREATE EXTENSION IF NOT EXISTS fuzzystrmatch WITH SCHEMA public;


--
-- Name: EXTENSION fuzzystrmatch; Type: COMMENT; Schema: -; Owner: -
--

COMMENT ON EXTENSION fuzzystrmatch IS 'determine similarities and distance between strings';


SET search_path = memberships, pg_catalog;

SET default_tablespace = '';

SET default_with_oids = false;

--
-- Name: documents; Type: TABLE; Schema: memberships; Owner: -; Tablespace:
--

CREATE TABLE documents (
    document_id integer NOT NULL,
    text text,
    ctime timestamp with time zone DEFAULT now() NOT NULL
);


--
-- Name: documents_document_id_seq; Type: SEQUENCE; Schema: memberships; Owner: -
--

CREATE SEQUENCE documents_document_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


--
-- Name: documents_document_id_seq; Type: SEQUENCE OWNED BY; Schema: memberships; Owner: -
--

ALTER SEQUENCE documents_document_id_seq OWNED BY documents.document_id;


--
-- Name: donations; Type: TABLE; Schema: memberships; Owner: -; Tablespace:
--

CREATE TABLE donations (
    payment_id integer,
    account_id integer,
    paid_date date,
    amount numeric(9,2)
);


--
-- Name: invoice_documents; Type: TABLE; Schema: memberships; Owner: -; Tablespace:
--

CREATE TABLE invoice_documents (
    invoice_id integer NOT NULL,
    document_id integer NOT NULL
);


--
-- Name: invoices; Type: TABLE; Schema: memberships; Owner: -; Tablespace:
--

CREATE TABLE invoices (
    invoice_id integer NOT NULL,
    text text NOT NULL
);


--
-- Name: membership_period_invoices; Type: TABLE; Schema: memberships; Owner: -; Tablespace:
--

CREATE TABLE membership_period_invoices (
    membership_period_id integer NOT NULL,
    invoice_id integer NOT NULL
);


--
-- Name: membership_periods; Type: TABLE; Schema: memberships; Owner: -; Tablespace:
--

CREATE TABLE membership_periods (
    membership_period_id integer NOT NULL,
    account_id integer NOT NULL,
    start_date date NOT NULL,
    end_date date NOT NULL,
    price numeric(8,2) NOT NULL
);


--
-- Name: membership_periods_id_seq; Type: SEQUENCE; Schema: memberships; Owner: -
--

CREATE SEQUENCE membership_periods_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


--
-- Name: membership_periods_id_seq; Type: SEQUENCE OWNED BY; Schema: memberships; Owner: -
--

ALTER SEQUENCE membership_periods_id_seq OWNED BY membership_periods.membership_period_id;


--
-- Name: payments; Type: TABLE; Schema: memberships; Owner: -; Tablespace:
--

CREATE TABLE payments (
    payment_id integer NOT NULL,
    account_id integer NOT NULL,
    paid_date date NOT NULL,
    amount numeric(9,2) NOT NULL,
    comment text
);


--
-- Name: payments_payment_id_seq; Type: SEQUENCE; Schema: memberships; Owner: -
--

CREATE SEQUENCE payments_payment_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


--
-- Name: payments_payment_id_seq; Type: SEQUENCE OWNED BY; Schema: memberships; Owner: -
--

ALTER SEQUENCE payments_payment_id_seq OWNED BY payments.payment_id;


SET search_path = public, pg_catalog;

--
-- Name: account_aliases; Type: TABLE; Schema: public; Owner: -; Tablespace:
--

CREATE TABLE account_aliases (
    account integer NOT NULL,
    alias text NOT NULL
);


--
-- Name: accounts; Type: TABLE; Schema: public; Owner: -; Tablespace:
--

CREATE TABLE accounts (
    id integer NOT NULL,
    name text NOT NULL,
    type text NOT NULL,
    last_billed timestamp with time zone
);


--
-- Name: accounts_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

CREATE SEQUENCE accounts_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


--
-- Name: accounts_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: -
--

ALTER SEQUENCE accounts_id_seq OWNED BY accounts.id;


--
-- Name: members; Type: TABLE; Schema: public; Owner: -; Tablespace:
--


CREATE TABLE members (
    id integer NOT NULL,
    date timestamp with time zone DEFAULT now() NOT NULL,
    full_name text NOT NULL,
    email text NOT NULL,
    account integer NOT NULL,
    organization text,
    price numeric(8,0) NOT NULL,
    recurrence interval DEFAULT '1 mon'::interval NOT NULL,
    flag character varying(10),
    CONSTRAINT members_email_check CHECK ((email ~~ '%%%%@%%%%.%%%%'::text)),
    CONSTRAINT members_full_name_check CHECK ((length(full_name) > 0)),
    CONSTRAINT nonnegative_price CHECK ((price >= (0)::numeric))
);


--
-- Name: active_members; Type: VIEW; Schema: public; Owner: -
--

CREATE VIEW active_members AS
 SELECT DISTINCT ON (m.account) m.id,
    m.date,
    m.full_name,
    m.email,
    m.price,
    m.recurrence,
    m.account,
    m.organization,
    m.flag
   FROM members m
  ORDER BY m.account, m.id DESC;


--
-- Name: transaction_lines; Type: TABLE; Schema: public; Owner: -; Tablespace:
--

CREATE TABLE transaction_lines (
    transaction integer NOT NULL,
    debit_account integer NOT NULL,
    credit_account integer NOT NULL,
    amount numeric(10,2) NOT NULL,
    currency text NOT NULL,
    stock integer DEFAULT 0 NOT NULL,
    CONSTRAINT transaction_lines_amount_check CHECK ((amount >= (0)::numeric))
);


--
-- Name: all_balances; Type: VIEW; Schema: public; Owner: -
--

CREATE VIEW all_balances AS
 SELECT a.id,
    a.type,
    a.name,
    (COALESCE(d.amount, (0)::numeric) - COALESCE(c.amount, (0)::numeric)) AS balance,
    (COALESCE(d.stock, (0)::bigint) - COALESCE(c.stock, (0)::bigint)) AS stock
   FROM ((accounts a
     LEFT JOIN ( SELECT transaction_lines.debit_account AS account,
            sum(transaction_lines.amount) AS amount,
            transaction_lines.currency,
            sum(transaction_lines.stock) AS stock
           FROM transaction_lines
          GROUP BY transaction_lines.debit_account, transaction_lines.currency) d ON ((d.account = a.id)))
     LEFT JOIN ( SELECT transaction_lines.credit_account AS account,
            sum(transaction_lines.amount) AS amount,
            transaction_lines.currency,
            sum(transaction_lines.stock) AS stock
           FROM transaction_lines
          GROUP BY transaction_lines.credit_account, transaction_lines.currency) c ON ((c.account = a.id)))
  ORDER BY a.type, lower(a.name);


--
-- Name: auth; Type: TABLE; Schema: public; Owner: -; Tablespace:
--

CREATE TABLE auth (
    account integer NOT NULL,
    realm text NOT NULL,
    data text NOT NULL
);


--
-- Name: auth_log; Type: TABLE; Schema: public; Owner: -; Tablespace:
--

CREATE TABLE auth_log (
    host inet NOT NULL,
    account integer,
    date timestamp with time zone DEFAULT now() NOT NULL,
    realm text NOT NULL
);


--
-- Name: billing_runs; Type: TABLE; Schema: public; Owner: -; Tablespace:
--

CREATE TABLE billing_runs (
    date timestamp with time zone DEFAULT now(),
    account integer NOT NULL,
    amount numeric(6,2) NOT NULL
);


--
-- Name: checkins; Type: TABLE; Schema: public; Owner: -; Tablespace:
--

CREATE TABLE checkins (
    id integer NOT NULL,
    account integer NOT NULL,
    date timestamp with time zone DEFAULT now() NOT NULL,
    type character varying(50) DEFAULT 'checkin'::character varying NOT NULL
);


--
-- Name: checkins_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

CREATE SEQUENCE checkins_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


--
-- Name: checkins_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: -
--

ALTER SEQUENCE checkins_id_seq OWNED BY checkins.id;


--
-- Name: transactions; Type: TABLE; Schema: public; Owner: -; Tablespace:
--

CREATE TABLE transactions (
    id integer NOT NULL,
    date timestamp with time zone DEFAULT now() NOT NULL,
    reason text
);


--
-- Name: undone_transactions; Type: VIEW; Schema: public; Owner: -
--

CREATE VIEW undone_transactions AS
 SELECT (substr(transactions.reason, 6))::integer AS transaction_id
   FROM transactions
  WHERE (transactions.reason ~~ 'undo %%%%'::text);


--
-- Name: debt_ceilings; Type: VIEW; Schema: public; Owner: -
--

CREATE VIEW debt_ceilings AS
 SELECT transaction_lines.credit_account AS account_id,
    sum(transaction_lines.amount) AS debt_ceiling
   FROM (transaction_lines
     JOIN accounts ON ((accounts.id = transaction_lines.debit_account)))
  WHERE ((accounts.type = 'product'::text) AND (NOT (transaction_lines.transaction IN ( SELECT undone_transactions.transaction_id
           FROM undone_transactions))))
  GROUP BY transaction_lines.credit_account;


--
-- Name: events; Type: TABLE; Schema: public; Owner: -; Tablespace:
--

CREATE TABLE events (
    id integer NOT NULL,
    date timestamp with time zone DEFAULT now() NOT NULL,
    type text NOT NULL,
    account integer,
    amount numeric(9,2)
);


--
-- Name: events_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

CREATE SEQUENCE events_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


--
-- Name: events_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: -
--

ALTER SEQUENCE events_id_seq OWNED BY events.id;


--
-- Name: fiken_faktura; Type: TABLE; Schema: public; Owner: -; Tablespace:
--

CREATE TABLE fiken_faktura (
    id integer NOT NULL,
    last_date_billed timestamp with time zone DEFAULT (now() - '1 mon'::interval) NOT NULL,
    fiken_kundenummer integer NOT NULL,
    account integer NOT NULL
);


--
-- Name: fiken_faktura_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

CREATE SEQUENCE fiken_faktura_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


--
-- Name: fiken_faktura_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: -
--

ALTER SEQUENCE fiken_faktura_id_seq OWNED BY fiken_faktura.id;


--
-- Name: invoices; Type: TABLE; Schema: public; Owner: -; Tablespace:
--

CREATE TABLE invoices (
    id integer NOT NULL,
    text text NOT NULL
);


--
-- Name: join_dates; Type: VIEW; Schema: public; Owner: -
--

CREATE VIEW join_dates AS
 SELECT members.account,
    min(members.date) AS date
   FROM members
  GROUP BY members.account;


--
-- Name: mac; Type: TABLE; Schema: public; Owner: -; Tablespace:
--

CREATE TABLE mac (
    account integer NOT NULL,
    macaddr macaddr NOT NULL,
    device character varying(30) DEFAULT 'PC'::character varying
);


--
-- Name: mac_old; Type: TABLE; Schema: public; Owner: -; Tablespace:
--

CREATE TABLE mac_old (
    account integer NOT NULL,
    macaddr macaddr NOT NULL,
    device character varying(30) DEFAULT 'PC'::character varying
);


--
-- Name: member_invoices; Type: TABLE; Schema: public; Owner: -; Tablespace:
--

CREATE TABLE member_invoices (
    date timestamp with time zone DEFAULT now() NOT NULL,
    pay_by date NOT NULL,
    amount numeric(5,2) NOT NULL,
    id integer NOT NULL,
    account integer,
    bilag integer
);


--
-- Name: member_invoices_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

CREATE SEQUENCE member_invoices_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


--
-- Name: member_invoices_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: -
--

ALTER SEQUENCE member_invoices_id_seq OWNED BY member_invoices.id;


--
-- Name: member_registrations_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

CREATE SEQUENCE member_registrations_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


--
-- Name: member_registrations_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: -
--

ALTER SEQUENCE member_registrations_id_seq OWNED BY members.id;


--
-- Name: membership_infos; Type: TABLE; Schema: public; Owner: -; Tablespace:
--

CREATE TABLE membership_infos (
    name text NOT NULL,
    price numeric(8,0) NOT NULL,
    recurrence interval NOT NULL
);


--
-- Name: membership_time_ranges; Type: VIEW; Schema: public; Owner: -
--

CREATE VIEW membership_time_ranges AS
 SELECT DISTINCT ON (m.id) m.account,
    m.id AS member_id,
    m.date AS date_begin,
    COALESCE(n.date, ('infinity'::timestamp without time zone)::timestamp with time zone) AS date_end,
    m.price
   FROM (members m
     LEFT JOIN members n ON (((m.account = n.account) AND (n.id > m.id))))
  ORDER BY m.id, n.id;


--
-- Name: pretty_transaction_lines; Type: VIEW; Schema: public; Owner: -
--

CREATE VIEW pretty_transaction_lines AS
 SELECT tl.transaction,
    tl.debit_account,
    tl.credit_account,
    tl.amount,
    tl.currency,
    tl.stock,
    da.name AS debit_account_name,
    ca.name AS credit_account_name,
    t.date
   FROM (((transaction_lines tl
     JOIN accounts da ON ((da.id = tl.debit_account)))
     JOIN accounts ca ON ((ca.id = tl.credit_account)))
     JOIN transactions t ON ((t.id = tl.transaction)))
  ORDER BY t.id;


--
-- Name: product_stock; Type: VIEW; Schema: public; Owner: -
--

CREATE VIEW product_stock AS
 SELECT a.id,
    a.name,
    (COALESCE((d.stock)::numeric, (0)::numeric) - COALESCE((c.stock)::numeric, (0)::numeric)) AS stock,
    (COALESCE(d.amount, (0)::numeric) - COALESCE(c.amount, (0)::numeric)) AS amount
   FROM ((accounts a
     LEFT JOIN ( SELECT transaction_lines.debit_account AS account,
            sum(transaction_lines.stock) AS stock,
            sum(transaction_lines.amount) AS amount,
            transaction_lines.currency
           FROM transaction_lines
          GROUP BY transaction_lines.debit_account, transaction_lines.currency) d ON ((d.account = a.id)))
     LEFT JOIN ( SELECT transaction_lines.credit_account AS account,
            sum(transaction_lines.stock) AS stock,
            sum(transaction_lines.amount) AS amount
           FROM transaction_lines
          GROUP BY transaction_lines.credit_account, transaction_lines.currency) c ON ((c.account = a.id)))
  WHERE (a.type = 'product'::text);


--
-- Name: stripe_customer; Type: TABLE; Schema: public; Owner: -; Tablespace:
--

CREATE TABLE stripe_customer (
    account integer NOT NULL,
    id text
);


--
-- Name: stripe_payment; Type: TABLE; Schema: public; Owner: -; Tablespace:
--

CREATE TABLE stripe_payment (
    invoice_id text NOT NULL,
    account integer NOT NULL,
    start_date date NOT NULL,
    end_date date NOT NULL,
    price numeric(8,2) NOT NULL,
    paid_date date
);


--
-- Name: transactions_id_seq; Type: SEQUENCE; Schema: public; Owner: -
--

CREATE SEQUENCE transactions_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


--
-- Name: transactions_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: -
--

ALTER SEQUENCE transactions_id_seq OWNED BY transactions.id;


--
-- Name: user_balances; Type: VIEW; Schema: public; Owner: -
--

CREATE VIEW user_balances AS
 SELECT a.id,
    a.name,
    (COALESCE(d.amount, (0)::numeric) - COALESCE(c.amount, (0)::numeric)) AS balance
   FROM ((accounts a
     LEFT JOIN ( SELECT transaction_lines.debit_account AS account,
            sum(transaction_lines.amount) AS amount,
            transaction_lines.currency
           FROM transaction_lines
          GROUP BY transaction_lines.debit_account, transaction_lines.currency) d ON ((d.account = a.id)))
     LEFT JOIN ( SELECT transaction_lines.credit_account AS account,
            sum(transaction_lines.amount) AS amount,
            transaction_lines.currency
           FROM transaction_lines
          GROUP BY transaction_lines.credit_account, transaction_lines.currency) c ON ((c.account = a.id)));


SET search_path = memberships, pg_catalog;

--
-- Name: document_id; Type: DEFAULT; Schema: memberships; Owner: -
--

ALTER TABLE ONLY documents ALTER COLUMN document_id SET DEFAULT nextval('documents_document_id_seq'::regclass);


--
-- Name: membership_period_id; Type: DEFAULT; Schema: memberships; Owner: -
--

ALTER TABLE ONLY membership_periods ALTER COLUMN membership_period_id SET DEFAULT nextval('membership_periods_id_seq'::regclass);


--
-- Name: payment_id; Type: DEFAULT; Schema: memberships; Owner: -
--

ALTER TABLE ONLY payments ALTER COLUMN payment_id SET DEFAULT nextval('payments_payment_id_seq'::regclass);


SET search_path = public, pg_catalog;

--
-- Name: id; Type: DEFAULT; Schema: public; Owner: -
--

ALTER TABLE ONLY accounts ALTER COLUMN id SET DEFAULT nextval('accounts_id_seq'::regclass);


--
-- Name: id; Type: DEFAULT; Schema: public; Owner: -
--

ALTER TABLE ONLY checkins ALTER COLUMN id SET DEFAULT nextval('checkins_id_seq'::regclass);


--
-- Name: id; Type: DEFAULT; Schema: public; Owner: -
--

ALTER TABLE ONLY events ALTER COLUMN id SET DEFAULT nextval('events_id_seq'::regclass);


--
-- Name: id; Type: DEFAULT; Schema: public; Owner: -
--

ALTER TABLE ONLY fiken_faktura ALTER COLUMN id SET DEFAULT nextval('fiken_faktura_id_seq'::regclass);


--
-- Name: id; Type: DEFAULT; Schema: public; Owner: -
--

ALTER TABLE ONLY member_invoices ALTER COLUMN id SET DEFAULT nextval('member_invoices_id_seq'::regclass);


--
-- Name: id; Type: DEFAULT; Schema: public; Owner: -
--

ALTER TABLE ONLY members ALTER COLUMN id SET DEFAULT nextval('member_registrations_id_seq'::regclass);


--
-- Name: id; Type: DEFAULT; Schema: public; Owner: -
--

ALTER TABLE ONLY transactions ALTER COLUMN id SET DEFAULT nextval('transactions_id_seq'::regclass);


SET search_path = memberships, pg_catalog;

--
-- Name: documents_pkey; Type: CONSTRAINT; Schema: memberships; Owner: -; Tablespace:
--

ALTER TABLE ONLY documents
    ADD CONSTRAINT documents_pkey PRIMARY KEY (document_id);


--
-- Name: invoice_documents_pkey; Type: CONSTRAINT; Schema: memberships; Owner: -; Tablespace:
--

ALTER TABLE ONLY invoice_documents
    ADD CONSTRAINT invoice_documents_pkey PRIMARY KEY (invoice_id, document_id);


--
-- Name: invoices_pkey; Type: CONSTRAINT; Schema: memberships; Owner: -; Tablespace:
--

ALTER TABLE ONLY invoices
    ADD CONSTRAINT invoices_pkey PRIMARY KEY (invoice_id);


--
-- Name: membership_period_invoices_pkey; Type: CONSTRAINT; Schema: memberships; Owner: -; Tablespace:
--

ALTER TABLE ONLY membership_period_invoices
    ADD CONSTRAINT membership_period_invoices_pkey PRIMARY KEY (membership_period_id, invoice_id);


--
-- Name: membership_periods_pkey; Type: CONSTRAINT; Schema: memberships; Owner: -; Tablespace:
--

ALTER TABLE ONLY membership_periods
    ADD CONSTRAINT membership_periods_pkey PRIMARY KEY (membership_period_id);


--
-- Name: payments_pkey; Type: CONSTRAINT; Schema: memberships; Owner: -; Tablespace:
--

ALTER TABLE ONLY payments
    ADD CONSTRAINT payments_pkey PRIMARY KEY (payment_id);


SET search_path = public, pg_catalog;

--
-- Name: account_aliases_alias_key; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace:
--

ALTER TABLE ONLY account_aliases
    ADD CONSTRAINT account_aliases_alias_key UNIQUE (alias);


--
-- Name: accounts_pkey; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace:
--

ALTER TABLE ONLY accounts
    ADD CONSTRAINT accounts_pkey PRIMARY KEY (id);


--
-- Name: checkins_pkey; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace:
--

ALTER TABLE ONLY checkins
    ADD CONSTRAINT checkins_pkey PRIMARY KEY (id);


--
-- Name: events_pkey; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace:
--

ALTER TABLE ONLY events
    ADD CONSTRAINT events_pkey PRIMARY KEY (id);


--
-- Name: fiken_faktura_pkey; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace:
--

ALTER TABLE ONLY fiken_faktura
    ADD CONSTRAINT fiken_faktura_pkey PRIMARY KEY (id);


--
-- Name: invoices_pkey; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace:
--

ALTER TABLE ONLY invoices
    ADD CONSTRAINT invoices_pkey PRIMARY KEY (id);


--
-- Name: mac2_macaddr_key; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace:
--

ALTER TABLE ONLY mac
    ADD CONSTRAINT mac2_macaddr_key UNIQUE (macaddr);


--
-- Name: member_invoices_pkey; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace:
--

ALTER TABLE ONLY member_invoices
    ADD CONSTRAINT member_invoices_pkey PRIMARY KEY (id);


--
-- Name: member_registrations_pkey; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace:
--

ALTER TABLE ONLY members
    ADD CONSTRAINT member_registrations_pkey PRIMARY KEY (id);


--
-- Name: membership_infos_pkey; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace:
--

ALTER TABLE ONLY membership_infos
    ADD CONSTRAINT membership_infos_pkey PRIMARY KEY (name);


--
-- Name: stripe_customer_pkey; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace:
--

ALTER TABLE ONLY stripe_customer
    ADD CONSTRAINT stripe_customer_pkey PRIMARY KEY (account);


--
-- Name: transactions_pkey; Type: CONSTRAINT; Schema: public; Owner: -; Tablespace:
--

ALTER TABLE ONLY transactions
    ADD CONSTRAINT transactions_pkey PRIMARY KEY (id);


SET search_path = memberships, pg_catalog;

--
-- Name: membership_payments_unique_account_date; Type: INDEX; Schema: memberships; Owner: -; Tablespace:
--

CREATE UNIQUE INDEX membership_payments_unique_account_date ON payments USING btree (account_id, paid_date);


SET search_path = public, pg_catalog;

--
-- Name: accounts_lower_name; Type: INDEX; Schema: public; Owner: -; Tablespace:
--

CREATE UNIQUE INDEX accounts_lower_name ON accounts USING btree (lower(name));


--
-- Name: accounts_name; Type: INDEX; Schema: public; Owner: -; Tablespace:
--

CREATE UNIQUE INDEX accounts_name ON accounts USING btree (name);


--
-- Name: auth_log_date_idx; Type: INDEX; Schema: public; Owner: -; Tablespace:
--

CREATE INDEX auth_log_date_idx ON auth_log USING btree (date);


--
-- Name: checkins_date; Type: INDEX; Schema: public; Owner: -; Tablespace:
--

CREATE INDEX checkins_date ON checkins USING btree (date);

ALTER TABLE checkins CLUSTER ON checkins_date;


SET search_path = memberships, pg_catalog;

--
-- Name: invoice_documents_document_id_fkey; Type: FK CONSTRAINT; Schema: memberships; Owner: -
--

ALTER TABLE ONLY invoice_documents
    ADD CONSTRAINT invoice_documents_document_id_fkey FOREIGN KEY (document_id) REFERENCES documents(document_id);


--
-- Name: invoice_documents_invoice_id_fkey; Type: FK CONSTRAINT; Schema: memberships; Owner: -
--

ALTER TABLE ONLY invoice_documents
    ADD CONSTRAINT invoice_documents_invoice_id_fkey FOREIGN KEY (invoice_id) REFERENCES invoices(invoice_id);


--
-- Name: membership_period_invoices_invoice_id_fkey; Type: FK CONSTRAINT; Schema: memberships; Owner: -
--

ALTER TABLE ONLY membership_period_invoices
    ADD CONSTRAINT membership_period_invoices_invoice_id_fkey FOREIGN KEY (invoice_id) REFERENCES invoices(invoice_id) ON UPDATE CASCADE;


--
-- Name: membership_period_invoices_membership_period_id_fkey; Type: FK CONSTRAINT; Schema: memberships; Owner: -
--

ALTER TABLE ONLY membership_period_invoices
    ADD CONSTRAINT membership_period_invoices_membership_period_id_fkey FOREIGN KEY (membership_period_id) REFERENCES membership_periods(membership_period_id) ON UPDATE CASCADE;


--
-- Name: membership_periods_account_fkey; Type: FK CONSTRAINT; Schema: memberships; Owner: -
--

ALTER TABLE ONLY membership_periods
    ADD CONSTRAINT membership_periods_account_fkey FOREIGN KEY (account_id) REFERENCES public.accounts(id);


--
-- Name: payments_account_fkey; Type: FK CONSTRAINT; Schema: memberships; Owner: -
--

ALTER TABLE ONLY payments
    ADD CONSTRAINT payments_account_fkey FOREIGN KEY (account_id) REFERENCES public.accounts(id);


SET search_path = public, pg_catalog;

--
-- Name: account_aliases_account_fkey; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY account_aliases
    ADD CONSTRAINT account_aliases_account_fkey FOREIGN KEY (account) REFERENCES accounts(id);


--
-- Name: auth_account_fkey; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY auth
    ADD CONSTRAINT auth_account_fkey FOREIGN KEY (account) REFERENCES accounts(id);


--
-- Name: auth_log_account_fkey; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY auth_log
    ADD CONSTRAINT auth_log_account_fkey FOREIGN KEY (account) REFERENCES accounts(id);


--
-- Name: billing_runs_account_fkey; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY billing_runs
    ADD CONSTRAINT billing_runs_account_fkey FOREIGN KEY (account) REFERENCES accounts(id);


--
-- Name: checkins_account_fkey; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY checkins
    ADD CONSTRAINT checkins_account_fkey FOREIGN KEY (account) REFERENCES accounts(id);


--
-- Name: events_account_fkey; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY events
    ADD CONSTRAINT events_account_fkey FOREIGN KEY (account) REFERENCES accounts(id);


--
-- Name: member_invoices_account_fkey; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY member_invoices
    ADD CONSTRAINT member_invoices_account_fkey FOREIGN KEY (account) REFERENCES accounts(id);


--
-- Name: members_account_fkey; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY members
    ADD CONSTRAINT members_account_fkey FOREIGN KEY (account) REFERENCES accounts(id);


--
-- Name: stripe_customer_account_fkey; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY stripe_customer
    ADD CONSTRAINT stripe_customer_account_fkey FOREIGN KEY (account) REFERENCES accounts(id);


--
-- Name: transaction_line_credit_account_fkey; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY transaction_lines
    ADD CONSTRAINT transaction_line_credit_account_fkey FOREIGN KEY (credit_account) REFERENCES accounts(id);


--
-- Name: transaction_line_debit_account_fkey; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY transaction_lines
    ADD CONSTRAINT transaction_line_debit_account_fkey FOREIGN KEY (debit_account) REFERENCES accounts(id);


--
-- Name: transaction_line_transaction_fkey; Type: FK CONSTRAINT; Schema: public; Owner: -
--

ALTER TABLE ONLY transaction_lines
    ADD CONSTRAINT transaction_line_transaction_fkey FOREIGN KEY (transaction) REFERENCES transactions(id);


--
-- Name: memberships; Type: ACL; Schema: -; Owner: -
--

REVOKE ALL ON SCHEMA memberships FROM PUBLIC;


--
-- Name: public; Type: ACL; Schema: -; Owner: -
--

REVOKE ALL ON SCHEMA public FROM PUBLIC;
REVOKE ALL ON SCHEMA public FROM postgres;
GRANT ALL ON SCHEMA public TO postgres;
GRANT ALL ON SCHEMA public TO PUBLIC;


SET search_path = memberships, pg_catalog;

--
-- Name: membership_periods; Type: ACL; Schema: memberships; Owner: -
--

REVOKE ALL ON TABLE membership_periods FROM PUBLIC;


--
-- Name: payments; Type: ACL; Schema: memberships; Owner: -
--

REVOKE ALL ON TABLE payments FROM PUBLIC;


SET search_path = public, pg_catalog;

--
-- Name: account_aliases; Type: ACL; Schema: public; Owner: -
--

REVOKE ALL ON TABLE account_aliases FROM PUBLIC;
GRANT SELECT ON TABLE account_aliases TO p2k12_pos;


--
-- Name: accounts; Type: ACL; Schema: public; Owner: -
--

REVOKE ALL ON TABLE accounts FROM PUBLIC;
REVOKE ALL ON TABLE accounts FROM postgres;
GRANT ALL ON TABLE accounts TO postgres;
GRANT SELECT,INSERT ON TABLE accounts TO p2k12_pos;


--
-- Name: accounts_id_seq; Type: ACL; Schema: public; Owner: -
--

REVOKE ALL ON SEQUENCE accounts_id_seq FROM PUBLIC;
REVOKE ALL ON SEQUENCE accounts_id_seq FROM postgres;
GRANT ALL ON SEQUENCE accounts_id_seq TO postgres;
GRANT USAGE ON SEQUENCE accounts_id_seq TO p2k12_pos;


--
-- Name: members; Type: ACL; Schema: public; Owner: -
--

REVOKE ALL ON TABLE members FROM PUBLIC;
REVOKE ALL ON TABLE members FROM postgres;
GRANT ALL ON TABLE members TO postgres;
GRANT SELECT,INSERT ON TABLE members TO p2k12_pos;


--
-- Name: active_members; Type: ACL; Schema: public; Owner: -
--

REVOKE ALL ON TABLE active_members FROM PUBLIC;
GRANT SELECT ON TABLE active_members TO p2k12_pos;


--
-- Name: transaction_lines; Type: ACL; Schema: public; Owner: -
--

REVOKE ALL ON TABLE transaction_lines FROM PUBLIC;
REVOKE ALL ON TABLE transaction_lines FROM postgres;
GRANT ALL ON TABLE transaction_lines TO postgres;
GRANT SELECT,INSERT ON TABLE transaction_lines TO p2k12_pos;


--
-- Name: all_balances; Type: ACL; Schema: public; Owner: -
--

REVOKE ALL ON TABLE all_balances FROM PUBLIC;
REVOKE ALL ON TABLE all_balances FROM postgres;
GRANT ALL ON TABLE all_balances TO postgres;
GRANT SELECT ON TABLE all_balances TO p2k12_pos;


--
-- Name: auth; Type: ACL; Schema: public; Owner: -
--

REVOKE ALL ON TABLE auth FROM PUBLIC;
REVOKE ALL ON TABLE auth FROM postgres;
GRANT ALL ON TABLE auth TO postgres;
GRANT SELECT,INSERT,UPDATE ON TABLE auth TO p2k12_pos;


--
-- Name: auth_log; Type: ACL; Schema: public; Owner: -
--

REVOKE ALL ON TABLE auth_log FROM PUBLIC;
REVOKE ALL ON TABLE auth_log FROM postgres;
GRANT ALL ON TABLE auth_log TO postgres;
GRANT SELECT,INSERT ON TABLE auth_log TO p2k12_pos;


--
-- Name: billing_runs; Type: ACL; Schema: public; Owner: -
--

REVOKE ALL ON TABLE billing_runs FROM PUBLIC;
GRANT SELECT ON TABLE billing_runs TO p2k12_pos;


--
-- Name: checkins; Type: ACL; Schema: public; Owner: -
--

REVOKE ALL ON TABLE checkins FROM PUBLIC;
REVOKE ALL ON TABLE checkins FROM postgres;
GRANT ALL ON TABLE checkins TO postgres;
GRANT SELECT,INSERT ON TABLE checkins TO p2k12_pos;


--
-- Name: checkins_id_seq; Type: ACL; Schema: public; Owner: -
--

REVOKE ALL ON SEQUENCE checkins_id_seq FROM PUBLIC;
REVOKE ALL ON SEQUENCE checkins_id_seq FROM postgres;
GRANT ALL ON SEQUENCE checkins_id_seq TO postgres;
GRANT USAGE ON SEQUENCE checkins_id_seq TO p2k12_pos;


--
-- Name: transactions; Type: ACL; Schema: public; Owner: -
--

REVOKE ALL ON TABLE transactions FROM PUBLIC;
REVOKE ALL ON TABLE transactions FROM postgres;
GRANT ALL ON TABLE transactions TO postgres;
GRANT SELECT,INSERT ON TABLE transactions TO p2k12_pos;


--
-- Name: undone_transactions; Type: ACL; Schema: public; Owner: -
--

REVOKE ALL ON TABLE undone_transactions FROM PUBLIC;
GRANT SELECT ON TABLE undone_transactions TO p2k12_pos;


--
-- Name: debt_ceilings; Type: ACL; Schema: public; Owner: -
--

REVOKE ALL ON TABLE debt_ceilings FROM PUBLIC;
GRANT SELECT ON TABLE debt_ceilings TO p2k12_pos;


--
-- Name: events; Type: ACL; Schema: public; Owner: -
--

REVOKE ALL ON TABLE events FROM PUBLIC;
GRANT SELECT ON TABLE events TO p2k12_pos;


--
-- Name: events_id_seq; Type: ACL; Schema: public; Owner: -
--

REVOKE ALL ON SEQUENCE events_id_seq FROM PUBLIC;


--
-- Name: fiken_faktura; Type: ACL; Schema: public; Owner: -
--

REVOKE ALL ON TABLE fiken_faktura FROM PUBLIC;
REVOKE ALL ON TABLE fiken_faktura FROM postgres;
GRANT ALL ON TABLE fiken_faktura TO postgres;
GRANT SELECT,INSERT,UPDATE ON TABLE fiken_faktura TO p2k12_pos;


--
-- Name: fiken_faktura_id_seq; Type: ACL; Schema: public; Owner: -
--

REVOKE ALL ON SEQUENCE fiken_faktura_id_seq FROM PUBLIC;
REVOKE ALL ON SEQUENCE fiken_faktura_id_seq FROM postgres;
GRANT ALL ON SEQUENCE fiken_faktura_id_seq TO postgres;
GRANT USAGE ON SEQUENCE fiken_faktura_id_seq TO p2k12_pos;


--
-- Name: invoices; Type: ACL; Schema: public; Owner: -
--

REVOKE ALL ON TABLE invoices FROM PUBLIC;
GRANT SELECT ON TABLE invoices TO p2k12_pos;


--
-- Name: join_dates; Type: ACL; Schema: public; Owner: -
--

REVOKE ALL ON TABLE join_dates FROM PUBLIC;
GRANT SELECT ON TABLE join_dates TO p2k12_pos;


--
-- Name: mac; Type: ACL; Schema: public; Owner: -
--

REVOKE ALL ON TABLE mac FROM PUBLIC;
GRANT SELECT ON TABLE mac TO p2k12_pos;


--
-- Name: mac_old; Type: ACL; Schema: public; Owner: -
--

REVOKE ALL ON TABLE mac_old FROM PUBLIC;
GRANT SELECT ON TABLE mac_old TO p2k12_pos;


--
-- Name: member_invoices; Type: ACL; Schema: public; Owner: -
--

REVOKE ALL ON TABLE member_invoices FROM PUBLIC;
REVOKE ALL ON TABLE member_invoices FROM postgres;
GRANT ALL ON TABLE member_invoices TO postgres;
GRANT SELECT ON TABLE member_invoices TO p2k12_pos;


--
-- Name: member_invoices_id_seq; Type: ACL; Schema: public; Owner: -
--

REVOKE ALL ON SEQUENCE member_invoices_id_seq FROM PUBLIC;
REVOKE ALL ON SEQUENCE member_invoices_id_seq FROM postgres;
GRANT ALL ON SEQUENCE member_invoices_id_seq TO postgres;


--
-- Name: member_registrations_id_seq; Type: ACL; Schema: public; Owner: -
--

REVOKE ALL ON SEQUENCE member_registrations_id_seq FROM PUBLIC;
REVOKE ALL ON SEQUENCE member_registrations_id_seq FROM postgres;
GRANT ALL ON SEQUENCE member_registrations_id_seq TO postgres;
GRANT USAGE ON SEQUENCE member_registrations_id_seq TO p2k12_pos;


--
-- Name: membership_infos; Type: ACL; Schema: public; Owner: -
--

REVOKE ALL ON TABLE membership_infos FROM PUBLIC;
GRANT SELECT ON TABLE membership_infos TO p2k12_pos;


--
-- Name: pretty_transaction_lines; Type: ACL; Schema: public; Owner: -
--

REVOKE ALL ON TABLE pretty_transaction_lines FROM PUBLIC;
REVOKE ALL ON TABLE pretty_transaction_lines FROM postgres;
GRANT ALL ON TABLE pretty_transaction_lines TO postgres;
GRANT SELECT ON TABLE pretty_transaction_lines TO p2k12_pos;


--
-- Name: product_stock; Type: ACL; Schema: public; Owner: -
--

REVOKE ALL ON TABLE product_stock FROM PUBLIC;
REVOKE ALL ON TABLE product_stock FROM postgres;
GRANT ALL ON TABLE product_stock TO postgres;
GRANT SELECT ON TABLE product_stock TO p2k12_pos;


--
-- Name: stripe_customer; Type: ACL; Schema: public; Owner: -
--

REVOKE ALL ON TABLE stripe_customer FROM PUBLIC;
REVOKE ALL ON TABLE stripe_customer FROM postgres;
GRANT ALL ON TABLE stripe_customer TO postgres;
GRANT ALL ON TABLE stripe_customer TO p2k12_pos;


--
-- Name: stripe_payment; Type: ACL; Schema: public; Owner: -
--

REVOKE ALL ON TABLE stripe_payment FROM PUBLIC;
REVOKE ALL ON TABLE stripe_payment FROM postgres;
GRANT ALL ON TABLE stripe_payment TO postgres;
GRANT ALL ON TABLE stripe_payment TO p2k12_pos;


--
-- Name: transactions_id_seq; Type: ACL; Schema: public; Owner: -
--

REVOKE ALL ON SEQUENCE transactions_id_seq FROM PUBLIC;
REVOKE ALL ON SEQUENCE transactions_id_seq FROM postgres;
GRANT ALL ON SEQUENCE transactions_id_seq TO postgres;
GRANT USAGE ON SEQUENCE transactions_id_seq TO p2k12_pos;


--
-- Name: user_balances; Type: ACL; Schema: public; Owner: -
--

REVOKE ALL ON TABLE user_balances FROM PUBLIC;
REVOKE ALL ON TABLE user_balances FROM postgres;
GRANT ALL ON TABLE user_balances TO postgres;
GRANT SELECT ON TABLE user_balances TO p2k12_pos;


--
-- PostgreSQL database dump complete
--
