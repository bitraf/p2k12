--
-- PostgreSQL database dump
--

SET statement_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = off;
SET check_function_bodies = false;
SET client_min_messages = warning;
SET escape_string_warning = off;

SET search_path = public, pg_catalog;

SET default_tablespace = '';

SET default_with_oids = false;

--
-- Name: accounts; Type: TABLE; Schema: public; Owner: postgres; Tablespace: 
--

CREATE TABLE accounts (
    id integer NOT NULL,
    name text NOT NULL,
    type text NOT NULL
);


ALTER TABLE public.accounts OWNER TO postgres;

--
-- Name: accounts_id_seq; Type: SEQUENCE; Schema: public; Owner: postgres
--

CREATE SEQUENCE accounts_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MAXVALUE
    NO MINVALUE
    CACHE 1;


ALTER TABLE public.accounts_id_seq OWNER TO postgres;

--
-- Name: accounts_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: postgres
--

ALTER SEQUENCE accounts_id_seq OWNED BY accounts.id;


--
-- Name: transaction_lines; Type: TABLE; Schema: public; Owner: mortehu; Tablespace: 
--

CREATE TABLE transaction_lines (
    transaction integer NOT NULL,
    debit_account integer NOT NULL,
    credit_account integer NOT NULL,
    amount numeric(10,2) NOT NULL,
    currency text NOT NULL,
    stock integer DEFAULT 0 NOT NULL
);


ALTER TABLE public.transaction_lines OWNER TO mortehu;

--
-- Name: all_balances; Type: VIEW; Schema: public; Owner: mortehu
--

CREATE VIEW all_balances AS
    SELECT a.id, a.type, a.name, (COALESCE(d.amount, (0)::numeric) - COALESCE(c.amount, (0)::numeric)) AS balance, (COALESCE(d.stock, (0)::bigint) - COALESCE(c.stock)) AS stock FROM ((accounts a LEFT JOIN (SELECT transaction_lines.debit_account AS account, sum(transaction_lines.amount) AS amount, transaction_lines.currency, sum(transaction_lines.stock) AS stock FROM transaction_lines GROUP BY transaction_lines.debit_account, transaction_lines.currency) d ON ((d.account = a.id))) LEFT JOIN (SELECT transaction_lines.credit_account AS account, sum(transaction_lines.amount) AS amount, transaction_lines.currency, sum(transaction_lines.stock) AS stock FROM transaction_lines GROUP BY transaction_lines.credit_account, transaction_lines.currency) c ON ((c.account = a.id))) ORDER BY a.type, a.name;


ALTER TABLE public.all_balances OWNER TO mortehu;

--
-- Name: member_invoices; Type: TABLE; Schema: public; Owner: mortehu; Tablespace: 
--

CREATE TABLE member_invoices (
    member integer NOT NULL,
    date timestamp with time zone DEFAULT now() NOT NULL,
    pay_by date NOT NULL,
    amount numeric(5,2) NOT NULL,
    paid_date date
);


ALTER TABLE public.member_invoices OWNER TO mortehu;

--
-- Name: members; Type: TABLE; Schema: public; Owner: mortehu; Tablespace: 
--

CREATE TABLE members (
    id integer NOT NULL,
    date timestamp with time zone DEFAULT now() NOT NULL,
    full_name text NOT NULL,
    email text NOT NULL,
    type text NOT NULL,
    account integer NOT NULL
);


ALTER TABLE public.members OWNER TO mortehu;

--
-- Name: member_registrations_id_seq; Type: SEQUENCE; Schema: public; Owner: mortehu
--

CREATE SEQUENCE member_registrations_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MAXVALUE
    NO MINVALUE
    CACHE 1;


ALTER TABLE public.member_registrations_id_seq OWNER TO mortehu;

--
-- Name: member_registrations_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: mortehu
--

ALTER SEQUENCE member_registrations_id_seq OWNED BY members.id;


--
-- Name: transactions; Type: TABLE; Schema: public; Owner: mortehu; Tablespace: 
--

CREATE TABLE transactions (
    id integer NOT NULL,
    date timestamp with time zone DEFAULT now() NOT NULL,
    reason text
);


ALTER TABLE public.transactions OWNER TO mortehu;

--
-- Name: pretty_transaction_lines; Type: VIEW; Schema: public; Owner: mortehu
--

CREATE VIEW pretty_transaction_lines AS
    SELECT tl.transaction, tl.debit_account, tl.credit_account, tl.amount, tl.currency, tl.stock, da.name AS debit_account_name, ca.name AS credit_account_name FROM (((transaction_lines tl JOIN accounts da ON ((da.id = tl.debit_account))) JOIN accounts ca ON ((ca.id = tl.credit_account))) JOIN transactions t ON ((t.id = tl.transaction))) ORDER BY t.id;


ALTER TABLE public.pretty_transaction_lines OWNER TO mortehu;

--
-- Name: product_stock; Type: VIEW; Schema: public; Owner: mortehu
--

CREATE VIEW product_stock AS
    SELECT a.id, a.name, (COALESCE((d.stock)::numeric, (0)::numeric) - COALESCE((c.stock)::numeric, (0)::numeric)) AS stock, (COALESCE(d.amount, (0)::numeric) - COALESCE(c.amount, (0)::numeric)) AS amount FROM ((accounts a LEFT JOIN (SELECT transaction_lines.debit_account AS account, sum(transaction_lines.stock) AS stock, sum(transaction_lines.amount) AS amount, transaction_lines.currency FROM transaction_lines GROUP BY transaction_lines.debit_account, transaction_lines.currency) d ON ((d.account = a.id))) LEFT JOIN (SELECT transaction_lines.credit_account AS account, sum(transaction_lines.stock) AS stock, sum(transaction_lines.amount) AS amount FROM transaction_lines GROUP BY transaction_lines.credit_account, transaction_lines.currency) c ON ((c.account = a.id))) WHERE (a.type = 'product'::text);


ALTER TABLE public.product_stock OWNER TO mortehu;

--
-- Name: transactions_id_seq; Type: SEQUENCE; Schema: public; Owner: mortehu
--

CREATE SEQUENCE transactions_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MAXVALUE
    NO MINVALUE
    CACHE 1;


ALTER TABLE public.transactions_id_seq OWNER TO mortehu;

--
-- Name: transactions_id_seq; Type: SEQUENCE OWNED BY; Schema: public; Owner: mortehu
--

ALTER SEQUENCE transactions_id_seq OWNED BY transactions.id;


--
-- Name: user_balances; Type: VIEW; Schema: public; Owner: mortehu
--

CREATE VIEW user_balances AS
    SELECT a.id, a.name, (COALESCE(d.amount, (0)::numeric) - COALESCE(c.amount, (0)::numeric)) AS balance FROM ((accounts a LEFT JOIN (SELECT transaction_lines.debit_account AS account, sum(transaction_lines.amount) AS amount, transaction_lines.currency FROM transaction_lines GROUP BY transaction_lines.debit_account, transaction_lines.currency) d ON ((d.account = a.id))) LEFT JOIN (SELECT transaction_lines.credit_account AS account, sum(transaction_lines.amount) AS amount, transaction_lines.currency FROM transaction_lines GROUP BY transaction_lines.credit_account, transaction_lines.currency) c ON ((c.account = a.id))) WHERE (a.type = 'user'::text);


ALTER TABLE public.user_balances OWNER TO mortehu;

--
-- Name: id; Type: DEFAULT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY accounts ALTER COLUMN id SET DEFAULT nextval('accounts_id_seq'::regclass);


--
-- Name: id; Type: DEFAULT; Schema: public; Owner: mortehu
--

ALTER TABLE ONLY members ALTER COLUMN id SET DEFAULT nextval('member_registrations_id_seq'::regclass);


--
-- Name: id; Type: DEFAULT; Schema: public; Owner: mortehu
--

ALTER TABLE ONLY transactions ALTER COLUMN id SET DEFAULT nextval('transactions_id_seq'::regclass);


--
-- Name: accounts_pkey; Type: CONSTRAINT; Schema: public; Owner: postgres; Tablespace: 
--

ALTER TABLE ONLY accounts
    ADD CONSTRAINT accounts_pkey PRIMARY KEY (id);


--
-- Name: member_registrations_pkey; Type: CONSTRAINT; Schema: public; Owner: mortehu; Tablespace: 
--

ALTER TABLE ONLY members
    ADD CONSTRAINT member_registrations_pkey PRIMARY KEY (id);


--
-- Name: transactions_pkey; Type: CONSTRAINT; Schema: public; Owner: mortehu; Tablespace: 
--

ALTER TABLE ONLY transactions
    ADD CONSTRAINT transactions_pkey PRIMARY KEY (id);


--
-- Name: accounts_name; Type: INDEX; Schema: public; Owner: postgres; Tablespace: 
--

CREATE UNIQUE INDEX accounts_name ON accounts USING btree (name);


--
-- Name: member_invoices_member_fkey; Type: FK CONSTRAINT; Schema: public; Owner: mortehu
--

ALTER TABLE ONLY member_invoices
    ADD CONSTRAINT member_invoices_member_fkey FOREIGN KEY (member) REFERENCES members(id);


--
-- Name: members_account_fkey; Type: FK CONSTRAINT; Schema: public; Owner: mortehu
--

ALTER TABLE ONLY members
    ADD CONSTRAINT members_account_fkey FOREIGN KEY (account) REFERENCES accounts(id);


--
-- Name: transaction_line_credit_account_fkey; Type: FK CONSTRAINT; Schema: public; Owner: mortehu
--

ALTER TABLE ONLY transaction_lines
    ADD CONSTRAINT transaction_line_credit_account_fkey FOREIGN KEY (credit_account) REFERENCES accounts(id);


--
-- Name: transaction_line_debit_account_fkey; Type: FK CONSTRAINT; Schema: public; Owner: mortehu
--

ALTER TABLE ONLY transaction_lines
    ADD CONSTRAINT transaction_line_debit_account_fkey FOREIGN KEY (debit_account) REFERENCES accounts(id);


--
-- Name: transaction_line_transaction_fkey; Type: FK CONSTRAINT; Schema: public; Owner: mortehu
--

ALTER TABLE ONLY transaction_lines
    ADD CONSTRAINT transaction_line_transaction_fkey FOREIGN KEY (transaction) REFERENCES transactions(id);


--
-- Name: public; Type: ACL; Schema: -; Owner: postgres
--

REVOKE ALL ON SCHEMA public FROM PUBLIC;
REVOKE ALL ON SCHEMA public FROM postgres;
GRANT ALL ON SCHEMA public TO postgres;
GRANT ALL ON SCHEMA public TO PUBLIC;


--
-- Name: accounts; Type: ACL; Schema: public; Owner: postgres
--

REVOKE ALL ON TABLE accounts FROM PUBLIC;
REVOKE ALL ON TABLE accounts FROM postgres;
GRANT ALL ON TABLE accounts TO postgres;
GRANT SELECT,INSERT ON TABLE accounts TO p2k12;


--
-- Name: accounts_id_seq; Type: ACL; Schema: public; Owner: postgres
--

REVOKE ALL ON SEQUENCE accounts_id_seq FROM PUBLIC;
REVOKE ALL ON SEQUENCE accounts_id_seq FROM postgres;
GRANT ALL ON SEQUENCE accounts_id_seq TO postgres;
GRANT SELECT,UPDATE ON SEQUENCE accounts_id_seq TO p2k12;


--
-- Name: transaction_lines; Type: ACL; Schema: public; Owner: mortehu
--

REVOKE ALL ON TABLE transaction_lines FROM PUBLIC;
REVOKE ALL ON TABLE transaction_lines FROM mortehu;
GRANT ALL ON TABLE transaction_lines TO mortehu;
GRANT SELECT,INSERT ON TABLE transaction_lines TO p2k12;


--
-- Name: all_balances; Type: ACL; Schema: public; Owner: mortehu
--

REVOKE ALL ON TABLE all_balances FROM PUBLIC;
REVOKE ALL ON TABLE all_balances FROM mortehu;
GRANT ALL ON TABLE all_balances TO mortehu;
GRANT SELECT ON TABLE all_balances TO p2k12;


--
-- Name: member_invoices; Type: ACL; Schema: public; Owner: mortehu
--

REVOKE ALL ON TABLE member_invoices FROM PUBLIC;
REVOKE ALL ON TABLE member_invoices FROM mortehu;
GRANT ALL ON TABLE member_invoices TO mortehu;
GRANT SELECT,INSERT ON TABLE member_invoices TO p2k12;


--
-- Name: members; Type: ACL; Schema: public; Owner: mortehu
--

REVOKE ALL ON TABLE members FROM PUBLIC;
REVOKE ALL ON TABLE members FROM mortehu;
GRANT ALL ON TABLE members TO mortehu;
GRANT SELECT,INSERT ON TABLE members TO p2k12;


--
-- Name: member_registrations_id_seq; Type: ACL; Schema: public; Owner: mortehu
--

REVOKE ALL ON SEQUENCE member_registrations_id_seq FROM PUBLIC;
REVOKE ALL ON SEQUENCE member_registrations_id_seq FROM mortehu;
GRANT ALL ON SEQUENCE member_registrations_id_seq TO mortehu;
GRANT SELECT,UPDATE ON SEQUENCE member_registrations_id_seq TO p2k12;


--
-- Name: transactions; Type: ACL; Schema: public; Owner: mortehu
--

REVOKE ALL ON TABLE transactions FROM PUBLIC;
REVOKE ALL ON TABLE transactions FROM mortehu;
GRANT ALL ON TABLE transactions TO mortehu;
GRANT SELECT,INSERT ON TABLE transactions TO p2k12;


--
-- Name: pretty_transaction_lines; Type: ACL; Schema: public; Owner: mortehu
--

REVOKE ALL ON TABLE pretty_transaction_lines FROM PUBLIC;
REVOKE ALL ON TABLE pretty_transaction_lines FROM mortehu;
GRANT ALL ON TABLE pretty_transaction_lines TO mortehu;
GRANT SELECT ON TABLE pretty_transaction_lines TO p2k12;


--
-- Name: product_stock; Type: ACL; Schema: public; Owner: mortehu
--

REVOKE ALL ON TABLE product_stock FROM PUBLIC;
REVOKE ALL ON TABLE product_stock FROM mortehu;
GRANT ALL ON TABLE product_stock TO mortehu;
GRANT SELECT ON TABLE product_stock TO p2k12;


--
-- Name: transactions_id_seq; Type: ACL; Schema: public; Owner: mortehu
--

REVOKE ALL ON SEQUENCE transactions_id_seq FROM PUBLIC;
REVOKE ALL ON SEQUENCE transactions_id_seq FROM mortehu;
GRANT ALL ON SEQUENCE transactions_id_seq TO mortehu;
GRANT SELECT,UPDATE ON SEQUENCE transactions_id_seq TO p2k12;


--
-- Name: user_balances; Type: ACL; Schema: public; Owner: mortehu
--

REVOKE ALL ON TABLE user_balances FROM PUBLIC;
REVOKE ALL ON TABLE user_balances FROM mortehu;
GRANT ALL ON TABLE user_balances TO mortehu;
GRANT SELECT ON TABLE user_balances TO p2k12;


--
-- PostgreSQL database dump complete
--

