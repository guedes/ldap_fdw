\set ECHO 0
BEGIN;
\i sql/ldap_fdw.sql
\set ECHO all

-- Tests goes here.

ROLLBACK;
