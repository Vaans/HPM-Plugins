//===== Hercules Plugin ======================================
//= Charms
//===== By: ==================================================
//= Dastgir
//= AnnieRuru
//= original by digitalhamster
//===== Current Version: =====================================
//= 1.1
//===== Compatible With: ===================================== 
//= Hercules 2019-06-02
//===== Description: =========================================
//= Give item bonus like the game DiabloII
//===== Topic ================================================
//= http://herc.ws/board/topic/11575-charms/
//===== Additional Comments: =================================  
//= if you set Charm_Stack: true, it still run the status_calc_pc even if
//= - you already having the charm, but just add it up
//= - you have 10 charms in stack (give only 1 time bonus), but drop 5 charms
//= it still run status_calc_pc everytime, and I don't know a better way to do this ...
//============================================================

#include "common/hercules.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "common/conf.h"
#include "common/memmgr.h"
#include "common/nullpo.h"

#include "map/pc.h"
#include "map/script.h"
#include "map/status.h"
#include "map/itemdb.h"

#include "plugins/HPMHooking.h"
#include "common/HPMDataCheck.h"

HPExport struct hplugin_info pinfo = {
	"charms",
	SERVER_TYPE_MAP,
	"1.1",
	HPM_VERSION,
};

struct charm_item_data {
	bool charm;
	bool charm_stack;
};

struct player_data { // To Prevent Item Dup
	bool recalculate;
};

void itemdb_readdb_additional_fields_pre(int *itemid, struct config_setting_t **it_, int *n_, const char **source)
{
	struct item_data *idata = itemdb->load(*itemid);
	struct charm_item_data *cidata = getFromITEMDATA(idata, 0);
	struct config_setting_t *tt, *it = *it_;
	if (idata->type != IT_ETC)
		return;
	if (!cidata) {
		CREATE(cidata, struct charm_item_data, 1);
		addToITEMDATA(idata, cidata, 0, true);
	}
	if ((tt = libconfig->setting_get_member(it, "Charm")))
		if (libconfig->setting_get_bool(tt))
			cidata->charm = true;
	if ((tt = libconfig->setting_get_member(it, "Charm_Stack")))
		if (libconfig->setting_get_bool(tt))
			cidata->charm_stack = true;
	return;
}

int itemdb_isstackable_pre(int *nameid)
{
	struct item_data *idata = itemdb->search(*nameid);
	struct charm_item_data *cidata;
	nullpo_ret(idata);
	if (idata->type != IT_ETC)
		return 1;
	cidata = getFromITEMDATA(idata, 0);

	if (cidata == NULL)
		return 1;

	if (cidata->charm_stack == true) {
		hookStop();
		return 1;
	}
	if (cidata->charm == true) {
		hookStop();
		return 0;
	}
	return 1;
}

int itemdb_isstackable2_pre(struct item_data **data)
{
	struct charm_item_data *cidata = NULL;
	nullpo_ret(*data);

	if ((*data)->type != IT_ETC)
		return 1;

	cidata = getFromITEMDATA(*data, 0);
	if (cidata == NULL)
		return 1;
	if (cidata->charm_stack == true) {
		hookStop();
		return 1;
	}
	if (cidata->charm == true) {
		hookStop();
		return 0;
	}
	return 1;
}

// TODO: Maybe should add those job/level/upper flag restriction like I did on eathena ? hmm ... but hercules omit those fields ... using default
void status_calc_pc_additional_pre(struct map_session_data **sd_, enum e_status_calc_opt *opt)
{
	int i = 0;
	struct charm_item_data *cidata = NULL;
	struct map_session_data *sd = *sd_;
	for (i = 0; i < MAX_INVENTORY; ++i) {
		if (sd->inventory_data[i] == NULL)
			continue;
		if (sd->inventory_data[i]->type != IT_ETC)
			continue;

		cidata = getFromITEMDATA(sd->inventory_data[i], 0);

		if (cidata == NULL)
			continue;
		if (cidata->charm == false)
			continue;
		if (sd->inventory_data[i]->script) {
			script->run(sd->inventory_data[i]->script, 0, sd->bl.id, 0);
		}
	}
	return;
}

int pc_additem_post(int retVal, struct map_session_data *sd, const struct item *item_data ,int amount, e_log_pick_type log_type)
{
	struct item_data *idata = itemdb->search(item_data->nameid);
	struct charm_item_data *cidata = NULL;

	if (retVal != 0)
		return retVal;

	if (idata->type != IT_ETC)
		return retVal;

	cidata = getFromITEMDATA(idata, 0);

	if (cidata == NULL)
		return retVal;
	if (cidata->charm == true) {
		status_calc_pc(sd, SCO_NONE);
	}
	return retVal;
}

int pc_delitem_pre(struct map_session_data **sd_, int *n, int *amount, int *type, enum delitem_reason *reason, enum e_log_pick_type *log_type)
{
	struct charm_item_data *cidata = NULL;
	struct player_data *ssd = NULL;
	struct map_session_data *sd = *sd_;
	nullpo_retr(1, sd);
	if (sd->status.inventory[*n].nameid == 0 || *amount <= 0 || sd->status.inventory[*n].amount < *amount || sd->inventory_data[*n] == NULL) {
		hookStop();
		return 1;
	}
	if (sd->inventory_data[*n]->type != IT_ETC)
		return 0;
	cidata = getFromITEMDATA(sd->inventory_data[*n], 0);
	if (cidata == NULL)
		return 0;
	if (cidata->charm == true) {
		ssd = getFromMSD(sd, 0);
		if (ssd == NULL) {
			CREATE(ssd, struct player_data, 1);
			ssd->recalculate = 1;
			addToMSD(sd, ssd, 0, true);
		}
		else
			ssd->recalculate = 1;
	}
	return 0;
}

// maybe I should've just overload this function ...
int pc_delitem_post(int retVal, struct map_session_data *sd, int n, int amount, int type, enum delitem_reason reason, e_log_pick_type log_type)
{
	struct player_data *ssd = getFromMSD(sd, 0);
	if (ssd && ssd->recalculate == 1) {
		status_calc_pc(sd, SCO_NONE);
		ssd->recalculate = 0;
	}
	return retVal;
}

HPExport void plugin_init(void)
{
	addHookPre(itemdb, readdb_additional_fields, itemdb_readdb_additional_fields_pre);
	addHookPre(itemdb, isstackable, itemdb_isstackable_pre);
	addHookPre(itemdb, isstackable2, itemdb_isstackable2_pre);
	addHookPre(status, calc_pc_additional, status_calc_pc_additional_pre);
	addHookPost(pc, additem, pc_additem_post);
	addHookPre(pc, delitem, pc_delitem_pre);
	addHookPost(pc, delitem, pc_delitem_post);
}

HPExport void server_online(void)
{
	ShowInfo("'%s' Plugin by Dastgir/Hercules. Version '%s'\n", pinfo.name, pinfo.version);
}
