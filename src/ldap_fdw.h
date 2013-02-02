/*-------------------------------------------------------------------------
 *
 *      foreign-data wrapper for LDAP
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

#if PG_VERSION_NUM < 90200
#error wrong Postgresql version, 9.2.x is required
#endif

#include "funcapi.h"
#include "access/reloptions.h"
#include "catalog/pg_foreign_table.h"
#include "catalog/pg_foreign_server.h"
#include "catalog/pg_user_mapping.h"
/*
  #include "catalog/pg_type.h"
*/
#include "commands/defrem.h"
#include "commands/explain.h"
#include "foreign/fdwapi.h"
#include "foreign/foreign.h"
#include "miscadmin.h"
/*
  #include "mb/pg_wchar.h"
  #include "nodes/makefuncs.h"
*/
#include "optimizer/cost.h"
#include "optimizer/pathnode.h"
#include "optimizer/planmain.h"
#include "optimizer/restrictinfo.h"
/*
  #include "storage/fd.h"
  #include "utils/array.h"
*/
#include "utils/builtins.h"
#include "utils/rel.h"

#include <ldap.h>

#define LDAP_FDW_SET_OPTION(op, value) if (strcmp(def->defname, value) == 0) *op = defGetString(def)
#define LDAP_FDW_SET_OPTION_INT(op, value) if (strcmp(def->defname, value) == 0) *op = atoi(defGetString(def))
#define LDAP_FDW_SET_DEFAULT_OPTION(op, value) if (!*op) *op = value


#define PROCID_TEXTEQ 67
#define MAX_ARGS 100

#define LDAP_FDW_OPTION_ADDRESS       "address"
#define LDAP_FDW_OPTION_PORT          "port"
#define LDAP_FDW_OPTION_USER_DN       "user_dn"
#define LDAP_FDW_OPTION_PASSWORD      "password"
#define LDAP_FDW_OPTION_ATTRIBUTES    "attributes"
#define LDAP_FDW_OPTION_BASE_DN       "base_dn"
#define LDAP_FDW_OPTION_QUERY         "query"
#define LDAP_FDW_OPTION_LDAP_VERSION  "ldap_version"

#define LDAP_FDW_OPTION_ADDRESS_DEFAULT     (char *) "127.0.0.1"
#define LDAP_FDW_OPTION_PORT_DEFAULT        389
#define LDAP_FDW_OPTION_USER_DN_DEFAULT     (char *) "cn=admin,dc=example,dc=org"
#define LDAP_FDW_OPTION_PASSWORD_DEFAULT    (char *) "admin"
#define LDAP_FDW_OPTION_BASE_DN_DEFAULT     (char *) "dc=example,dc=org"
#define LDAP_FDW_OPTION_QUERY_DEFAULT       (char *) "(objectClass=*)"
#define LDAP_FDW_OPTION_LDAP_VERSION_DEFAULT  (char *) LDAP_VERSION3

extern LDAP *ldap_init(char *, int);
extern int ldap_simple_bind_s(LDAP *, const char *, const char *);
extern char **ldap_get_values(LDAP *, LDAPMessage *, char *);
extern int ldap_count_values(char **);
extern void ldap_value_free(char **);
extern int ldap_unbind(LDAP *);

/*
 * Valid options that could be used by
 * this wrapper
 */
typedef struct LdapFdwOption
{
  const char *option_name;
  Oid        option_context;
} LdapFdwOption;

static struct LdapFdwOption valid_options[] =
{
  {"address",   ForeignServerRelationId },
  {"port",      ForeignServerRelationId },
  {"user_dn",   UserMappingRelationId },
  {"password",  UserMappingRelationId },
  {"attributes",ForeignTableRelationId},
  {"base_dn",   ForeignTableRelationId},
  {"query",     ForeignTableRelationId},

  {NULL,        InvalidOid}
};

/*
 * Stores the FDW execution state
 */
typedef struct
{
  LDAP          *ldap_connection;
  LDAPMessage   *ldap_answer;
  LDAPMessage   *ldap_entry;
  BerElement    *ldap_ber;

  AttInMetadata *att_in_metadata;

  char          *address;
  int           port;
  char          *ldap_version;
  char          *user_dn;
  char          *password;
  char          *base_dn;
  char          *query;

  int           row;
} LdapFdwExecutionState;

/*
 * FDW functions declarations
 */

static void
ldapGetForeignRelSize(PlannerInfo *root,
                      RelOptInfo *baserel,
                      Oid foreigntableid);

static void
ldapGetForeignPaths(PlannerInfo *root,
                    RelOptInfo *baserel,
                    Oid foreigntableid);

static ForeignScan *
ldapGetForeignPlan(PlannerInfo *root,
                   RelOptInfo *baserel,
                   Oid foreigntableid,
                   ForeignPath *best_path,
                   List *tlist,
                   List *scan_clauses);

static void
ldapExplainForeignScan(ForeignScanState *node,
                       ExplainState *es);

static void
ldapBeginForeignScan(ForeignScanState *node,
                     int eflags);

static TupleTableSlot *
ldapIterateForeignScan(ForeignScanState *node);

static void
ldapReScanForeignScan(ForeignScanState *node);

static void
ldapEndForeignScan(ForeignScanState *node);

/*
static bool
ldapAnalyzeForeignTable(Relation relation,
                    AcquireSampleRowsFunc *func,
                    BlockNumber *totalpages);
*/

/*
 * Helper functions
 */
static void _get_str_attributes(char *attributes[], Relation relation);
static int  _name_str_case_cmp(Name name, const char *str);
static bool _is_valid_option(const char *option, Oid context);
static void _ldap_get_options(Oid foreign_table_id, char **address, int *port, char **ldap_version, char **user_dn, char **password, char **base_dn, char **query, char **attributes);
static void _ldap_check_quals(Node *, TupleDesc, char **, char **, bool *);
static char ** _string_to_array(char *);
