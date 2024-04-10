/** @file  mlanregclass.c
 *
 * @brief This files contains mlanconfig regclass command handling.
 *
 *
 * Copyright 2008-2021 NXP
 *
 * NXP CONFIDENTIAL
 * The source code contained or described herein and all documents related to
 * the source code (Materials) are owned by NXP, its
 * suppliers and/or its licensors. Title to the Materials remains with NXP,
 * its suppliers and/or its licensors. The Materials contain
 * trade secrets and proprietary and confidential information of NXP, its
 * suppliers and/or its licensors. The Materials are protected by worldwide
 * copyright and trade secret laws and treaty provisions. No part of the
 * Materials may be used, copied, reproduced, modified, published, uploaded,
 * posted, transmitted, distributed, or disclosed in any way without NXP's prior
 * express written permission.
 *
 * No license under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or delivery
 * of the Materials, either expressly, by implication, inducement, estoppel or
 * otherwise. Any license under such intellectual property rights must be
 * express and approved by NXP in writing.
 *
 */
/************************************************************************
Change log:
     08/11/2009: initial version
************************************************************************/

#include "mlanconfig.h"
#include "mlanhostcmd.h"
#include "mlanmisc.h"
#include "mlanoffload.h"
#include "mlanregclass.h"

/********************************************************
		Local Variables
********************************************************/

/********************************************************
		Global Variables
********************************************************/

/********************************************************
		Local Functions
********************************************************/

/**
 *  @brief Convert reg domain number to string
 *
 *  @param reg_domain Reg Domain
 *
 *  @return           Reg Domain type
 */
static char *reg_domain_to_str(reg_domain_e reg_domain)
{
	switch (reg_domain) {
	case REGDOMAIN_FCC:
		return "FCC";

	case REGDOMAIN_ETSI:
		return "ETSI";

	case REGDOMAIN_MIC:
		return "MIC";

	case REGDOMAIN_OTHER:
		return "MULTI";

	default:
		break;
	}

	return "UNKN";
}

/**
 *  @brief Convert reg channel table number to string
 *
 *  @param table_select Reg channel table
 *
 *  @return             Reg channel table type
 */
static char *table_num_to_str(reg_chan_table_e table_select)
{
	switch (table_select) {
	case REGTABLE_USER:
		return "User";

	case REGTABLE_MULTIDOMAIN:
		return "MultiDomain";

	case REGTABLE_ESS:
		return "ESS";

	case REGTABLE_DEFAULT:
		return "Default";

	default:
		break;
	}

	return "UNKN";
}

/**
 *  @brief      Regclass dump channel table
 *
 *  @param argc     Number of arguments
 *  @param argv     A pointer to arguments array
 *
 *  @return         MLAN_STATUS_SUCCESS--success, otherwise--fail
 */
static int reg_class_dump_chan_table(int argc, char *argv[])
{
	int ioctl_val, subioctl_val;
	struct iwreq iwr;
	HostCmd_DS_GEN *hostcmd;
	HostCmd_DS_REGCLASS_GET_CHAN_TABLE *get_table;
	t_u8 *buf, *pos;
	int idx, cmd_len;
	t_u16 regLimits;
	boolean invalid_cmd = FALSE;

	buf = 0;

	if (argv[0] == NULL) {
		invalid_cmd = TRUE;
	} else {
		if (get_priv_ioctl("hostcmd", &ioctl_val, &subioctl_val) ==
		    MLAN_STATUS_FAILURE) {
			return -EOPNOTSUPP;
		}

		cmd_len = sizeof(HostCmd_DS_GEN) +
			  sizeof(HostCmd_DS_REGCLASS_GET_CHAN_TABLE);
		buf = (t_u8 *)calloc(sizeof(t_u8), cmd_len);

		hostcmd = (HostCmd_DS_GEN *)buf;
		hostcmd->command = HostCmd_CMD_REGCLASS_CHAN_TABLE;
		hostcmd->size = 0;
		hostcmd->seq_num = 0;
		hostcmd->result = 0;
		pos = buf + sizeof(HostCmd_DS_GEN);

		get_table = (HostCmd_DS_REGCLASS_GET_CHAN_TABLE *)pos;
		get_table->action = cpu_to_le16(HostCmd_ACT_GEN_GET);

		if (reg_class_table_select(
			    argv[0],
			    (reg_chan_table_e *)&get_table->table_select) ==
		    FALSE) {
			invalid_cmd = TRUE;
		}
	}

	if (invalid_cmd) {
		printf("\nValid tables table; valid [user, md, ess, default]\n\n");
		if (buf) {
			free(buf);
		}
		return MLAN_STATUS_FAILURE;
	}

	hostcmd->command = cpu_to_le16(hostcmd->command);
	hostcmd->size = cpu_to_le16(cmd_len);

	get_table->table_select = cpu_to_le16((t_u16)(get_table->table_select));

	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, dev_name, IFNAMSIZ);
	iwr.u.data.pointer = (caddr_t)buf;
	iwr.u.data.length = cmd_len;
	iwr.u.data.flags = 0;

	if (ioctl(sockfd, ioctl_val, &iwr) < 0) {
		perror("mlanconfig: regClassIoctl ioctl");
		free(buf);
		return -EFAULT;
	}

	if (!le16_to_cpu(hostcmd->result)) {
		printf("HOSTCMD_RESP: ReturnCode=%#04x, Result=%#04x\n",
		       le16_to_cpu(hostcmd->command),
		       le16_to_cpu(hostcmd->result));
	} else {
		printf("HOSTCMD failed: ReturnCode=%#04x, Result=%#04x\n",
		       le16_to_cpu(hostcmd->command),
		       le16_to_cpu(hostcmd->result));
	}

	get_table->table_select = le16_to_cpu(get_table->table_select);
	get_table->chan = le32_to_cpu(get_table->chan);

	printf("---------------------------------------");
	printf("---------------------------------------\n");
	printf("%35s: %s [%d]\n", "Channel Table",
	       table_num_to_str(get_table->table_select), (int)get_table->chan);
	printf("---------------------------------------");
	printf("---------------------------------------\n");
	printf(" chn | freq | sfrq | sp |  class   | maxP | behavior limits\n");
	printf("---------------------------------------");
	printf("---------------------------------------\n");

	for (idx = 0; (unsigned int)idx < get_table->chan; idx++) {
		char regDisp[8];

		sprintf(regDisp, "%4s-%02u",
			reg_domain_to_str(
				get_table->chan_entry[idx].reg_domain),
			get_table->chan_entry[idx].regulatory_class);

		printf(" %03u | %04u | %04u | %02u | %-8s |  %02u  |",
		       get_table->chan_entry[idx].chan_num,
		       (get_table->chan_entry[idx].start_freq +
			(get_table->chan_entry[idx].chan_num * 5)),
		       le16_to_cpu(get_table->chan_entry[idx].start_freq),
		       le16_to_cpu(get_table->chan_entry[idx].chan_spacing),
		       regDisp, get_table->chan_entry[idx].max_tx_power);

		regLimits = le16_to_cpu(get_table->chan_entry[idx].reg_limits);

		if (regLimits & BLIMIT_NOMADIC)
			printf(" nomadic");
		if (regLimits & BLIMIT_INDOOR_ONLY)
			printf(" indoor");
		if (regLimits & BLIMIT_TPC)
			printf(" tpc");
		if (regLimits & BLIMIT_DFS)
			printf(" dfs");
		if (regLimits & BLIMIT_IBSS_PROHIBIT)
			printf(" no_ibss");
		if (regLimits & BLIMIT_FOUR_MS_CS)
			printf(" 4ms_cs");
		if (regLimits & BLIMIT_LIC_BASE_STA)
			printf(" base_sta");
		if (regLimits & BLIMIT_MOBILE_STA)
			printf(" mobile");
		if (regLimits & BLIMIT_PUBLIC_SAFETY)
			printf(" safety");
		if (regLimits & BLIMIT_ISM_BANDS)
			printf(" ism");

		printf("\n");
	}
	printf("---------------------------------------");
	printf("---------------------------------------\n");
	printf("\n");

	free(buf);

	return MLAN_STATUS_SUCCESS;
}

/**
 *  @brief      Regclass configure user table
 *
 *  @param argc     Number of arguments
 *  @param argv     A pointer to arguments array
 *
 *  @return         MLAN_STATUS_SUCCESS--success, otherwise--fail
 */
static int reg_class_config_user_table(int argc, char *argv[])
{
	int ioctl_val, subioctl_val, cmd_len;
	struct iwreq iwr;
	t_u8 *buf, *pos;
	HostCmd_DS_GEN *hostcmd;
	HostCmd_DS_REGCLASS_CONFIG_USER_TABLE *cfg_user_table;

	if (argv[0] == NULL) {
		printf("\nCountry string not specified\n");
		return MLAN_STATUS_FAILURE;
	}

	if (get_priv_ioctl("hostcmd", &ioctl_val, &subioctl_val) ==
	    MLAN_STATUS_FAILURE) {
		return -EOPNOTSUPP;
	}

	cmd_len = sizeof(HostCmd_DS_GEN) +
		  sizeof(HostCmd_DS_REGCLASS_CONFIG_USER_TABLE);
	buf = (t_u8 *)calloc(sizeof(t_u8), cmd_len);

	hostcmd = (HostCmd_DS_GEN *)buf;
	hostcmd->command = HostCmd_CMD_REGCLASS_CONFIG_USER_TABLE;
	hostcmd->size = 0;
	hostcmd->seq_num = 0;
	hostcmd->result = 0;

	pos = buf + sizeof(HostCmd_DS_GEN);

	cfg_user_table = (HostCmd_DS_REGCLASS_CONFIG_USER_TABLE *)pos;
	cfg_user_table->action = cpu_to_le16(HostCmd_ACT_GEN_SET);
	memcpy(cfg_user_table->regulatory_str, argv[0],
	       MIN(strlen(argv[0]), sizeof(cfg_user_table->regulatory_str)));

	hostcmd->command = cpu_to_le16(hostcmd->command);
	hostcmd->size = cpu_to_le16(cmd_len);

	strncpy(iwr.ifr_name, dev_name, IFNAMSIZ);
	iwr.u.data.flags = 0;
	iwr.u.data.pointer = (caddr_t)buf;
	iwr.u.data.length = cmd_len;

	if (ioctl(sockfd, ioctl_val, &iwr) < 0) {
		perror("mlanconfig: regClassIoctl ioctl");
		free(buf);
		return -EFAULT;
	}

	if (!le16_to_cpu(hostcmd->result)) {
		printf("HOSTCMD_RESP: ReturnCode=%#04x, Result=%#04x\n",
		       le16_to_cpu(hostcmd->command),
		       le16_to_cpu(hostcmd->result));
	} else {
		printf("HOSTCMD failed: ReturnCode=%#04x, Result=%#04x\n",
		       le16_to_cpu(hostcmd->command),
		       le16_to_cpu(hostcmd->result));
	}

	free(buf);
	return MLAN_STATUS_SUCCESS;
}

/**
 *  @brief      Issue regclass multi-domain command
 *
 *  @param argc     number of arguments
 *  @param argv     A pointer to arguments array
 *
 *  @return         MLAN_STATUS_SUCCESS--success, otherwise--fail
 */
static int reg_class_multidomain(int argc, char *argv[])
{
	int ioctl_val, subioctl_val, cmd_len;
	struct iwreq iwr;
	t_u8 *buf, *pos;
	HostCmd_DS_GEN *hostcmd;
	HostCmd_DS_REGCLASS_MULTIDOMAIN_CONTROL *multidomain_ctrl;
	boolean invalid_cmd = FALSE;

	if (argv[0] == NULL) {
		invalid_cmd = TRUE;
	} else {
		if (get_priv_ioctl("hostcmd", &ioctl_val, &subioctl_val) ==
		    MLAN_STATUS_FAILURE) {
			return -EOPNOTSUPP;
		}

		cmd_len = sizeof(HostCmd_DS_GEN) +
			  sizeof(HostCmd_DS_REGCLASS_MULTIDOMAIN_CONTROL);

		buf = (t_u8 *)calloc(sizeof(t_u8), cmd_len);

		hostcmd = (HostCmd_DS_GEN *)buf;
		hostcmd->command = HostCmd_CMD_REGCLASS_MULTIDOMAIN_CONTROL;
		hostcmd->size = 0;
		hostcmd->seq_num = 0;
		hostcmd->result = 0;

		pos = buf + sizeof(HostCmd_DS_GEN);

		multidomain_ctrl =
			(HostCmd_DS_REGCLASS_MULTIDOMAIN_CONTROL *)pos;
		if (strcmp(argv[0], "on") == 0) {
			multidomain_ctrl->multidomain_enable = 1;
		} else if (strcmp(argv[0], "off") == 0) {
			multidomain_ctrl->multidomain_enable = 0;
		} else {
			invalid_cmd = TRUE;
			free(buf);
		}
	}

	if (invalid_cmd) {
		printf("\nUnknown multiDomain command; valid [on, off]\n\n");
		return MLAN_STATUS_FAILURE;
	}

	multidomain_ctrl->multidomain_enable =
		cpu_to_le32(multidomain_ctrl->multidomain_enable);

	hostcmd->command = cpu_to_le16(hostcmd->command);
	hostcmd->size = cpu_to_le16(cmd_len);

	strncpy(iwr.ifr_name, dev_name, IFNAMSIZ);
	iwr.u.data.flags = 0;
	iwr.u.data.pointer = (caddr_t)buf;
	iwr.u.data.length = cmd_len;

	if (ioctl(sockfd, ioctl_val, &iwr) < 0) {
		perror("mlanconfig: regClass ioctl");
		free(buf);
		return -EFAULT;
	} else {
		printf("\nMultiDomain: %s\n",
		       le32_to_cpu(multidomain_ctrl->multidomain_enable) ?
			       "Enabled" :
			       "Disabled");
	}

	if (!le16_to_cpu(hostcmd->result)) {
		printf("HOSTCMD_RESP: ReturnCode=%#04x, Result=%#04x\n",
		       le16_to_cpu(hostcmd->command),
		       le16_to_cpu(hostcmd->result));
	} else {
		printf("HOSTCMD failed: ReturnCode=%#04x, Result=%#04x\n",
		       le16_to_cpu(hostcmd->command),
		       le16_to_cpu(hostcmd->result));
	}

	free(buf);
	return MLAN_STATUS_SUCCESS;
}

/**
 *  @brief Issue a regclass command
 *
 *  @param argc     number of arguments
 *  @param argv     A pointer to arguments array
 *
 *  @return         MLAN_STATUS_SUCCESS--success, otherwise--fail
 */
int process_regclass(int argc, char *argv[])
{
	sub_cmd_exec_t sub_cmd[] = {
		{"table", 1, 1, reg_class_dump_chan_table},
		{"multidomain", 1, 1, reg_class_multidomain},
		{"country", 1, 1, reg_class_config_user_table}};

	return (process_sub_cmd(sub_cmd, NELEMENTS(sub_cmd), argc, argv));
}
