// SPDX-License-Identifier: LGPL-2.1-or-later
#ifndef __ALSA_HINAWA_FW_FCP_H__
#define __ALSA_HINAWA_FW_FCP_H__

#include <hinawa.h>

G_BEGIN_DECLS

#define HINAWA_TYPE_FW_FCP	(hinawa_fw_fcp_get_type())

G_DECLARE_DERIVABLE_TYPE(HinawaFwFcp, hinawa_fw_fcp, HINAWA, FW_FCP, HinawaFwResp);

#define HINAWA_FW_FCP_ERROR	hinawa_fw_fcp_error_quark()

GQuark hinawa_fw_fcp_error_quark();

struct _HinawaFwFcpClass {
	HinawaFwRespClass parent_class;

        /**
         * HinawaFwFcpClass::responded:
         * @self: A [class@FwFcp].
         * @frame: (array length=frame_size)(element-type guint8): The array with elements for byte
         *         data of response for Function Control Protocol.
         * @frame_size: The number of elements of the array.
         *
	 * Class closure for the [signal@FwFcp::responded] signal.
	 *
	 * Since: 2.1
         */
	void (*responded)(HinawaFwFcp *self, const guint8 *frame, guint frame_size);
};

HinawaFwFcp *hinawa_fw_fcp_new(void);

void hinawa_fw_fcp_transaction(HinawaFwFcp *self,
			       const guint8 *req_frame, gsize req_frame_size,
			       guint8 *const *resp_frame, gsize *resp_frame_size,
			       GError **error);

void hinawa_fw_fcp_bind(HinawaFwFcp *self, HinawaFwNode *node, GError **error);
void hinawa_fw_fcp_unbind(HinawaFwFcp *self);

void hinawa_fw_fcp_command(HinawaFwFcp *self, const guint8 *cmd, gsize cmd_size,
			   guint timeout_ms, GError **error);

void hinawa_fw_fcp_avc_transaction(HinawaFwFcp *self, const guint8 *cmd, gsize cmd_size,
				   guint8 *const *resp, gsize *resp_size, guint timeout_ms,
				   GError **error);

G_END_DECLS

#endif
