/*****************************************************************************\
 *  slurm_topology.c - Topology plugin function setup.
 *****************************************************************************
 *  Copyright (C) 2009-2010 Lawrence Livermore National Security.
 *  Copyright (C) 2014 Silicon Graphics International Corp. All rights reserved.
 *  Produced at Lawrence Livermore National Laboratory (cf, DISCLAIMER).
 *  Written by Morris Jette <jette1@llnl.gov>
 *  CODE-OCEC-09-009. All rights reserved.
 *
 *  This file is part of Slurm, a resource management program.
 *  For details, see <https://slurm.schedmd.com/>.
 *  Please also read the included file: DISCLAIMER.
 *
 *  Slurm is free software; you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 *  In addition, as a special exception, the copyright holders give permission
 *  to link the code of portions of this program with the OpenSSL library under
 *  certain conditions as described in each individual source file, and
 *  distribute linked combinations including the two. You must obey the GNU
 *  General Public License in all respects for all of the code used other than
 *  OpenSSL. If you modify file(s) with this exception, you may extend this
 *  exception to your version of the file(s), but you are not obligated to do
 *  so. If you do not wish to do so, delete this exception statement from your
 *  version.  If you delete this exception statement from all source files in
 *  the program, then also delete it here.
 *
 *  Slurm is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with Slurm; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA.
\*****************************************************************************/

#include <pthread.h>

#include "src/common/log.h"
#include "src/common/plugrack.h"
#include "src/common/slurm_protocol_api.h"
#include "src/interfaces/topology.h"
#include "src/common/timers.h"
#include "src/common/xmalloc.h"
#include "src/common/xstring.h"

/* defined here but is really tree plugin related */
switch_record_t *switch_record_table = NULL;
int switch_record_cnt = 0;
int switch_levels = 0;               /* number of switch levels     */

typedef struct slurm_topo_ops {
	int		(*build_config)		( void );
	bool		(*node_ranking)		( void );
	int		(*get_node_addr)	( char* node_name,
						  char** addr,
						  char** pattern );
} slurm_topo_ops_t;

/*
 * Must be synchronized with slurm_topo_ops_t above.
 */
static const char *syms[] = {
	"topology_p_build_config",
	"topology_p_generate_node_ranking",
	"topology_p_get_node_addr",
};

static slurm_topo_ops_t ops;
static plugin_context_t	*g_context = NULL;
static pthread_mutex_t g_context_lock = PTHREAD_MUTEX_INITIALIZER;
static plugin_init_t plugin_inited = PLUGIN_NOT_INITED;

/*
 * The topology plugin can not be changed via reconfiguration
 * due to background threads, job priorities, etc. slurmctld must
 * be restarted and job priority changes may be required to change
 * the topology type.
 */
extern int topology_g_init(void)
{
	int retval = SLURM_SUCCESS;
	char *plugin_type = "topo";

	slurm_mutex_lock(&g_context_lock);

	if (plugin_inited)
		goto done;

	if (!slurm_conf.topology_plugin) {
		plugin_inited = PLUGIN_NOOP;
		goto done;
	}

	g_context = plugin_context_create(plugin_type,
					  slurm_conf.topology_plugin,
					  (void **) &ops, syms, sizeof(syms));

	if (!g_context) {
		error("cannot create %s context for %s",
		      plugin_type, slurm_conf.topology_plugin);
		retval = SLURM_ERROR;
		plugin_inited = PLUGIN_NOT_INITED;
		goto done;
	}

	plugin_inited = PLUGIN_INITED;
done:
	slurm_mutex_unlock(&g_context_lock);
	return retval;
}

extern int topology_g_fini(void)
{
	int rc = SLURM_SUCCESS;

	if (g_context) {
		rc = plugin_context_destroy(g_context);
		g_context = NULL;
	}

	plugin_inited = PLUGIN_NOT_INITED;

	return rc;
}

extern int topology_g_build_config(void)
{
	int rc;
	DEF_TIMERS;

	xassert(plugin_inited);

	if (plugin_inited == PLUGIN_NOOP)
		return SLURM_SUCCESS;

	START_TIMER;
	rc = (*(ops.build_config))();
	END_TIMER3(__func__, 20000);

	return rc;
}

/*
 * This operation is only supported by those topology plugins for
 * which the node ordering between slurmd and slurmctld is invariant.
 */
extern bool topology_g_generate_node_ranking(void)
{
	xassert(plugin_inited);

	if (plugin_inited == PLUGIN_NOOP)
		return false;

	return (*(ops.node_ranking))();
}

extern int topology_g_get_node_addr(char *node_name, char **addr,
				    char **pattern)
{
	xassert(plugin_inited);

	if (plugin_inited == PLUGIN_NOOP) {
#ifndef HAVE_FRONT_END
		if (find_node_record(node_name) == NULL)
			return SLURM_ERROR;
#endif

		*addr = xstrdup(node_name);
		*pattern = xstrdup("node");
		return SLURM_SUCCESS;
	}

	return (*(ops.get_node_addr))(node_name,addr,pattern);
}
