/*-------------------------------------------------------------------------
 *
 *		  foreign-data wrapper for LDAP
 *
 * Copyright (c) 2011, PostgreSQL Global Development Group
 *
 * This software is released under the PostgreSQL Licence
 *
 * Author: Dickson S. Guedes <guedes@guedesoft.net>
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "access/reloptions.h"
#include "catalog/pg_foreign_table.h"
#include "catalog/pg_foreign_server.h"
#include "catalog/pg_user_mapping.h"
#include "commands/defrem.h"
#include "commands/explain.h"
#include "foreign/fdwapi.h"
#include "foreign/foreign.h"
#include "funcapi.h"
#include "miscadmin.h"
#include "nodes/makefuncs.h"
#include "optimizer/cost.h"
#include "utils/rel.h"
#include "utils/builtins.h"

#include <ldap.h>

PG_MODULE_MAGIC;

/*
 * Valid options that could be used by 
 * this wrapper
 */
typedef struct LdapFdwOption
{
	const char *option_name;
	Oid        option_context;
};

static struct LdapFdwOption valid_options[] = 
{
	{"address",			ForeignServerRelationId },
	{"port",			ForeignServerRelationId },
	/* not implemented yet
	   {"auth_method",		ForeignServerRelationId },
	   {"ldap_version",	ForeignServerRelationId },
	   */
	{"user_dn",			UserMappingRelationId },
	{"password",		UserMappingRelationId },

	{"base_dn",			ForeignTableRelationId},

	{NULL,			InvalidOid}
};

/*
 * Stores the FDW execution state
 */
typedef struct LdapFdwExecutionState
{
	LDAP			*ldap_connection;
	LDAPMessage		*ldap_answer;
	LDAPMessage		*ldap_entry;
	BerElement		*ldap_ber;

	AttInMetadata	*att_in_metadata;

	char			*address;
	int				port;
	char			*auth_method;
	char			*ldap_version;
	char			*user_dn;
	char			*password;
	char			*base_dn;
	char			*query;

	int				row;
} LdapFdwExecutionState;

/*
 * Handler and Validator functions
 */
extern Datum ldap_fdw_handler(PG_FUNCTION_ARGS);
extern Datum ldap_fdw_validator(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(ldap_fdw_handler);
PG_FUNCTION_INFO_V1(ldap_fdw_validator);

/*
 * FDW functions declarations
 */
static FdwPlan *ldap_plan(Oid foreign_table_id, PlannerInfo *root, RelOptInfo *base_relation);
static void ldap_explain(ForeignScanState *node, ExplainState *es);
static void ldap_begin(ForeignScanState *node, int eflags);
static TupleTableSlot *ldap_iterate(ForeignScanState *node);
static void ldap_rescan(ForeignScanState *node);
static void ldap_end(ForeignScanState *node);

/*
 * Helper functions
 */
static void get_str_attributes(char *attributes[], Relation relation);
static int  name_str_case_cmp(Name name, const char *str);
static bool is_valid_option(const char *option, Oid context);
static void ldap_get_options(Oid foreign_table_id, char **address, int *port, char **auth_method, char **ldap_version, char **user_dn, char **password, char **base_dn);

/*
 * FDW functions implementation
 */

Datum
ldap_fdw_handler(PG_FUNCTION_ARGS)
{
	FdwRoutine *fdw_routine = makeNode(FdwRoutine);

	fdw_routine->PlanForeignScan = ldap_plan;
	fdw_routine->ExplainForeignScan = ldap_explain;
	fdw_routine->BeginForeignScan = ldap_begin;
	fdw_routine->IterateForeignScan = ldap_iterate;
	fdw_routine->ReScanForeignScan = ldap_rescan;
	fdw_routine->EndForeignScan = ldap_end;

	PG_RETURN_POINTER(fdw_routine);
}

Datum
ldap_fdw_validator(PG_FUNCTION_ARGS)
{
	List	   *options_list = untransformRelOptions(PG_GETARG_DATUM(0));
	Oid			catalog = PG_GETARG_OID(1);
	ListCell   *cell;

	foreach(cell, options_list)
	{
		DefElem    *def = (DefElem *) lfirst(cell);

		if (!is_valid_option(def->defname, catalog))
		{
			struct LdapFdwOption *opt;
			StringInfoData buf;

			/*
			 * Unknown option specified, complain about it. Provide a hint
			 * with list of valid options for the object.
			 */
			initStringInfo(&buf);
			for (opt = valid_options; opt->option_name; opt++)
			{
				if (catalog == opt->option_context)
					appendStringInfo(&buf, "%s%s", (buf.len > 0) ? ", " : "",
									 opt->option_name);
			}

			ereport(ERROR,
					(errcode(ERRCODE_FDW_INVALID_OPTION_NAME),
					 errmsg("invalid option \"%s\"", def->defname),
					 errhint("Valid options in this context are: %s",
							 buf.data)));
		}
	}

	PG_RETURN_BOOL(true);
}

static FdwPlan * 
ldap_plan( Oid foreign_table_id,
		   PlannerInfo *root,
		   RelOptInfo  *base_relation)
{
	FdwPlan *fdw_plan;

	fdw_plan = makeNode(FdwPlan);

	fdw_plan->startup_cost = 10;
	base_relation->rows = 1;
	fdw_plan->total_cost = 15;

	return fdw_plan;
}

static void
ldap_explain(ForeignScanState *node, ExplainState *es)
{
	/* TODO: calculate real values */
	ExplainPropertyText("Foreign Ldap", "ldap", es);

	if (es->costs)
	{
		ExplainPropertyLong("Foreign Ldap cost", 10.5, es);
	}
}

static void
ldap_begin(ForeignScanState *node, int eflags)
{
	char			*srv_address = NULL;
	int				srv_port = 0;
	char			*srv_auth_method = NULL;
	char			*srv_ldap_version = NULL;
	char			*srv_user_dn = NULL;
	char			*srv_password = NULL;
	char			*srv_base_dn = NULL;

	LDAP			*ldap_connection;
	LDAPMessage		*ldap_answer;
	int				ldap_result;

	LdapFdwExecutionState  *festate;
	char			*query;
	char			*attributes[256]; 

	/*
	 * nao faz nada caso esteja em explain
	 */
	if (eflags & EXEC_FLAG_EXPLAIN_ONLY)
		return;

	ldap_get_options(RelationGetRelid(node->ss.ss_currentRelation),
					 &srv_address, &srv_port, &srv_auth_method,
					 &srv_ldap_version, &srv_user_dn, &srv_password,
					 &srv_base_dn);

	ldap_connection = ldap_init(srv_address, srv_port);

	if (ldap_connection == NULL)
		ereport(ERROR,
				(errcode(ERRCODE_FDW_UNABLE_TO_ESTABLISH_CONNECTION),
				 errmsg("failed to create LDAP handler for address '%s' on port '%d'", srv_address, srv_port)
				));

	ldap_result = ldap_set_option(ldap_connection, LDAP_OPT_PROTOCOL_VERSION, &srv_ldap_version);

	if (ldap_result != LDAP_SUCCESS)
		ereport(ERROR,
				(errcode(ERRCODE_FDW_UNABLE_TO_ESTABLISH_CONNECTION),
				 errmsg("failed to set version 3 for LDAP server on address '%s' port '%d'. LDAP ERROR: %s", srv_address, srv_port, ldap_err2string(ldap_result))
				));

	ldap_result = ldap_simple_bind_s(ldap_connection, srv_user_dn, srv_password );

	if (ldap_result != LDAP_SUCCESS)
		ereport(ERROR,
				(errcode(ERRCODE_FDW_UNABLE_TO_ESTABLISH_CONNECTION),
				 errmsg("failed to authenticate to LDAP server using user_dn: %s. LDAP ERROR: %s", srv_user_dn, ldap_err2string(ldap_result))
				));

	query = (char *) palloc(1024);
	snprintf(query, 1024, "(&(objectClass=*))");
	get_str_attributes(&attributes, node->ss.ss_currentRelation);

	ldap_result = ldap_search_s(ldap_connection, srv_base_dn, LDAP_SCOPE_ONELEVEL, query, attributes, 0, &ldap_answer);

	if (ldap_result != LDAP_SUCCESS)
		ereport(ERROR,
				(errcode(ERRCODE_FDW_UNABLE_TO_CREATE_EXECUTION),
				 errmsg("failed to execute the LDAP search '%s' on base_dn '%s'. LDAP ERROR: %s", query, srv_base_dn, ldap_err2string(ldap_result))
				));

	festate = (LdapFdwExecutionState *) palloc(sizeof(LdapFdwExecutionState));
	node->fdw_state = (void *) festate;

	festate->ldap_connection = ldap_connection;
	festate->base_dn = srv_base_dn;
	festate->query = query;
	festate->row = 0;
	festate->ldap_entry = ldap_first_entry(ldap_connection, ldap_answer);
	festate->att_in_metadata = TupleDescGetAttInMetadata(node->ss.ss_currentRelation->rd_att);
}

static TupleTableSlot *
ldap_iterate(ForeignScanState *node)
{
	LdapFdwExecutionState	*festate = (LdapFdwExecutionState *) node->fdw_state;
	TupleTableSlot			*slot = node->ss.ss_ScanTupleSlot;
	Relation				relation = node->ss.ss_currentRelation;

	BerElement		*ber;
	HeapTuple		tuple;

	int				total_attributes, i;
	char			**att_values, **tup_values;
	char			*attribute = "";
	char			*dn = "";
	MemoryContext		oldcontext;

	ExecClearTuple(slot);

	if (festate->ldap_entry != NULL)
	{
		total_attributes = relation->rd_att->natts;
		att_values = (char **) palloc(sizeof(char *) * 1000);
		tup_values = (char **) palloc(sizeof(char *) * total_attributes);

		dn = ldap_get_dn(festate->ldap_connection, festate->ldap_entry);

		/* 
		 * FIXME
		 *
		 * actually i'm using a query that fetches all object class, but there
		 * some objects that don't have attibute, this must be handled.
		 *
		 * TODO
		 * the attribute fecthing could be improve to not loops every ldap_iterate call.
		 */
		for (i=0; i < total_attributes; i++)
		{
			Name attr_name = &relation->rd_att->attrs[i]->attname;
			tup_values[i] = NULL;

			for ( attribute = ldap_first_attribute(festate->ldap_connection, festate->ldap_entry, &ber);
				  attribute != NULL;
				  attribute = ldap_next_attribute(festate->ldap_connection, festate->ldap_entry, ber)) {

				if ((name_str_case_cmp(attr_name, attribute) == 0) &&
					((att_values = ldap_get_values(festate->ldap_connection, festate->ldap_entry, attribute)) != NULL) )
				{
					if (att_values[0] != NULL) 
						tup_values[i] = strdup(att_values[0]);
					break;
				}
			}
		}

		/* TODO: needs a switch context here? */
		oldcontext = MemoryContextSwitchTo(node->ss.ps.ps_ExprContext->ecxt_per_query_memory);
		tuple = BuildTupleFromCStrings(festate->att_in_metadata, tup_values);
		MemoryContextSwitchTo(oldcontext);

		ExecStoreTuple(tuple, slot, InvalidBuffer, false);

		festate->ldap_entry = ldap_next_entry(festate->ldap_connection, festate->ldap_entry);
	}

	return slot;
}

static void
ldap_rescan(ForeignScanState *node)
{
	LdapFdwExecutionState *festate = (LdapFdwExecutionState *) node->fdw_state;
}

static void
ldap_end(ForeignScanState *node)
{
	/*
	 * empty for now
	 */
}

/*
 * Helper functions
 */

/*
 * Extract the name of attributes from a Relation 
 * and return a array of strings where each element
 * is the attribute name. The array is null-terminated
 */
static void
get_str_attributes(char *attributes[], Relation relation)
{
	int total_attributes = relation->rd_att->natts;
	int i;

	for (i=0; i < total_attributes; i++)
	{
		Name attr_name = &relation->rd_att->attrs[i]->attname;
		attributes[i] = NameStr(*attr_name);
	}

	attributes[i++] = NULL;
}

/*
 * Case insensitive compare a 'name' with 'string'.
 * The string could or not be null-terminated.
 */
static int
name_str_case_cmp(Name name, const char *str)
{
	if (!name && !str)
		return 0;
	if (!name)
		return -1;				/* NULL < anything */
	if (!str)
		return 1;				/* NULL < anything */
	return pg_strncasecmp(NameStr(*name), str, sizeof(str));
}

/*
 * Check if the provided option is one of the valid options.
 * context is the Oid of the catalog holding the object the option is for.
 */
static bool
is_valid_option(const char *option, Oid context)
{
	struct LdapFdwOption *opt;

	for (opt = valid_options; opt->option_name; opt++)
	{
		if (context == opt->option_context && strcmp(opt->option_name, option) == 0)
			return true;
	}
	return false;
}

/*
 * Fetch the options for a ldap_fdw foreign table.
 */
static void
ldap_get_options(Oid foreign_table_id, char **address, int *port, char **auth_method, char **ldap_version, char **user_dn, char **password, char **base_dn)
{
	ForeignTable	*f_table;
	ForeignServer	*f_server;
	UserMapping		*f_mapping;
	List			*options;
	ListCell		*lc;

	/*
	 * Extract options from FDW objects.
	 */
	f_table = GetForeignTable(foreign_table_id);
	f_server = GetForeignServer(f_table->serverid);
	f_mapping = GetUserMapping(GetUserId(), f_table->serverid);

	options = NIL;
	options = list_concat(options, f_table->options);
	options = list_concat(options, f_server->options);
	options = list_concat(options, f_mapping->options);

	foreach(lc, options)
	{
		DefElem *def = (DefElem *) lfirst(lc);

		if (strcmp(def->defname, "address") == 0)
			*address = defGetString(def);

		if (strcmp(def->defname, "port") == 0)
			*port = atoi(defGetString(def));

		if (strcmp(def->defname, "auth_method") == 0)
			*auth_method = defGetString(def);

		if (strcmp(def->defname, "ldap_version") == 0)
			*ldap_version = defGetString(def);

		if (strcmp(def->defname, "user_dn") == 0)
			*user_dn = defGetString(def);

		if (strcmp(def->defname, "password") == 0)
			*password = defGetString(def);

		if (strcmp(def->defname, "base_dn") == 0)
			*base_dn = defGetString(def);
	}

	/* Default values, if required */
	if (!*address)
		*address = "127.0.0.1";

	if (!*port)
		*port = 389;

	*auth_method  = LDAP_AUTH_SIMPLE;
	*ldap_version = LDAP_VERSION3; 
}
