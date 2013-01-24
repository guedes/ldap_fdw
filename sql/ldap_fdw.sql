/*
 * Author: Dickson S. Guedes
 * Created at: 2011-07-30 13:35:30 -0300
 *
 */ 

-- create wrapper with validator and handler
CREATE OR REPLACE FUNCTION ldap_fdw_validator (text[], oid)
RETURNS bool
AS 'ldap_fdw'
LANGUAGE C STRICT;

CREATE OR REPLACE FUNCTION ldap_fdw_handler ()
RETURNS fdw_handler
AS 'ldap_fdw'
LANGUAGE C STRICT;

CREATE FOREIGN DATA WRAPPER ldap_fdw
VALIDATOR ldap_fdw_validator HANDLER ldap_fdw_handler;

