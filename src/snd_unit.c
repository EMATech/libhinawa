// SPDX-License-Identifier: LGPL-2.1-or-later
#include "internal.h"

#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

/**
 * HinawaSndUnit:
 * An event listener for ALSA FireWire sound devices.
 *
 * This class is an application of ALSA FireWire stack. Any functionality which ALSA drivers in the
 * stack can be available.
 *
 * Deprecated: 2.5. libhitaki library provides [class@Hitaki.SndUnit] as the alternative.
 */

/**
 * hinawa_snd_unit_error_quark:
 *
 * Return the [alias@GLib.Quark] for [struct@GLib.Error] which has code in Hinawa.SndUnitError.
 *
 * Since: 2.1
 * Deprecated: 2.5. Use Hitaki.AlsaFirewireError instead.
 *
 * Returns: A [alias@GLib.Quark].
 */
G_DEFINE_QUARK(hinawa-snd-unit-error-quark, hinawa_snd_unit_error)

static const char *const err_msgs[] = {
	[HINAWA_SND_UNIT_ERROR_DISCONNECTED] = "The associated hwdep device is not available",
	[HINAWA_SND_UNIT_ERROR_USED] = "The hedep device is already in use",
	[HINAWA_SND_UNIT_ERROR_OPENED] = "The instance is already associated to unit",
	[HINAWA_SND_UNIT_ERROR_NOT_OPENED] = "The instance is not associated to unit yet",
	[HINAWA_SND_UNIT_ERROR_LOCKED] = "The associated hwdep device is already locked or kernel packet streaming runs",
	[HINAWA_SND_UNIT_ERROR_UNLOCKED] = "The associated hwdep device is not locked against kernel packet streaming",
	[HINAWA_SND_UNIT_ERROR_WRONG_CLASS] = "The hwdep device is not for the unit expected by the class",
};

#define generate_local_error(error, code) \
	g_set_error_literal(error, HINAWA_FW_NODE_ERROR, code, err_msgs[code])

#define generate_file_error(error, code, format, arg) \
	g_set_error(error, G_FILE_ERROR, code, format, arg)

#define generate_syscall_error(error, errno, format, arg)			\
	g_set_error(error, HINAWA_SND_UNIT_ERROR, HINAWA_SND_UNIT_ERROR_FAILED,	\
		    format " %d(%s)", arg, errno, strerror(errno))

typedef struct {
	int fd;
	struct snd_firewire_get_info info;
	HinawaFwNode *node;

	gboolean streaming;
} HinawaSndUnitPrivate;
G_DEFINE_TYPE_WITH_PRIVATE(HinawaSndUnit, hinawa_snd_unit, G_TYPE_OBJECT)

typedef struct {
	GSource src;
	HinawaSndUnit *unit;
	gpointer tag;
	void *buf;
	size_t len;
} SndUnitSource;

enum snd_unit_prop_type {
	SND_UNIT_PROP_TYPE_FW_TYPE = 1,
	SND_UNIT_PROP_TYPE_CARD_ID,
	SND_UNIT_PROP_TYPE_DEVICE,
	SND_UNIT_PROP_TYPE_GUID,
	SND_UNIT_PROP_TYPE_STREAMING,
	SND_UNIT_PROP_TYPE_COUNT,
};
static GParamSpec *snd_unit_props[SND_UNIT_PROP_TYPE_COUNT] = { NULL, };

// This object has two signals.
enum snd_unit_sig_type {
	SND_UNIT_SIG_TYPE_LOCK_STATUS = 0,
	SND_UNIT_SIG_TYPE_DISCONNECTED,
	SND_UNIT_SIG_TYPE_COUNT,
};
static guint snd_unit_sigs[SND_UNIT_SIG_TYPE_COUNT] = { 0 };

static void snd_unit_get_property(GObject *obj, guint id,
				  GValue *val, GParamSpec *spec)
{
	HinawaSndUnit *self = HINAWA_SND_UNIT(obj);
	HinawaSndUnitPrivate *priv = hinawa_snd_unit_get_instance_private(self);

	switch (id) {
	case SND_UNIT_PROP_TYPE_FW_TYPE:
		g_value_set_enum(val, (HinawaSndUnitType)priv->info.type);
		break;
	case SND_UNIT_PROP_TYPE_CARD_ID:
		g_value_set_uint(val, priv->info.card);
		break;
	case SND_UNIT_PROP_TYPE_DEVICE:
		g_value_set_static_string(val,
					(const gchar *)priv->info.device_name);
		break;
	case SND_UNIT_PROP_TYPE_GUID:
		g_value_set_uint64(val,
				GUINT64_FROM_BE(*((guint64 *)priv->info.guid)));
		break;
	case SND_UNIT_PROP_TYPE_STREAMING:
		g_value_set_boolean(val, priv->streaming);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, id, spec);
		break;
	}
}

static void snd_unit_finalize(GObject *obj)
{
	HinawaSndUnit *self = HINAWA_SND_UNIT(obj);
	HinawaSndUnitPrivate *priv = hinawa_snd_unit_get_instance_private(self);

	close(priv->fd);
	priv->fd = -1;
	g_object_unref(priv->node);

	G_OBJECT_CLASS(hinawa_snd_unit_parent_class)->finalize(obj);
}

static void hinawa_snd_unit_class_init(HinawaSndUnitClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	gobject_class->get_property = snd_unit_get_property;
	gobject_class->finalize = snd_unit_finalize;

	/**
	 * HinawaSndUnit:type:
	 *
	 * Since: 1.0
	 * Deprecated: 2.5. Use [property@Hitaki.AlsaFirewire:unit-type] instead.
	 */
	snd_unit_props[SND_UNIT_PROP_TYPE_FW_TYPE] =
		g_param_spec_enum("type", "type",
				  "The value of HinawaSndUnitType enumerators",
				  HINAWA_TYPE_SND_UNIT_TYPE,
				  HINAWA_SND_UNIT_TYPE_DICE,
				  G_PARAM_READABLE | G_PARAM_DEPRECATED);

	/**
	 * HinawaSndUnit:card:
	 *
	 * Since: 2.0
	 * Deprecated: 2.5. Use [property@Hitaki.AlsaFirewire:card_id] instead.
	 */
	snd_unit_props[SND_UNIT_PROP_TYPE_CARD_ID] =
		g_param_spec_uint("card", "card",
				  "The numeric ID for ALSA sound card",
				  0, G_MAXUINT,
				  0,
				  G_PARAM_READABLE | G_PARAM_DEPRECATED);

	/**
	 * HinawaSndUnit:device:
	 *
	 *
	 * Since: 0.3
	 * Deprecated: 2.5. Use [property@Hitaki.AlsaFirewire:node-device] instead.
	 */
	snd_unit_props[SND_UNIT_PROP_TYPE_DEVICE] =
		g_param_spec_string("device", "device",
				    "A name of special file as FireWire unit.",
				    NULL,
				    G_PARAM_READABLE | G_PARAM_DEPRECATED);

	/**
	 * HinawaSndUnit:streaming:
	 *
	 * Since: 0.4
	 * Deprecated: 2.5. Use [property@Hitaki.AlsaFirewire:is-locked] instead.
	 */
	snd_unit_props[SND_UNIT_PROP_TYPE_STREAMING] =
		g_param_spec_boolean("streaming", "streaming",
				     "Whether this device is streaming or not",
				     FALSE,
				     G_PARAM_READABLE | G_PARAM_DEPRECATED);

	/**
	 * HinawaSndUnit:guid:
	 *
	 * Since: 0.4
	 * Deprecated: 2.5. Use [property@Hitaki.AlsaFirewire:guid] instead.
	 */
	snd_unit_props[SND_UNIT_PROP_TYPE_GUID] =
		g_param_spec_uint64("guid", "guid",
				    "Global unique ID for this firewire unit.",
				    0, G_MAXUINT64, 0,
				    G_PARAM_READABLE | G_PARAM_DEPRECATED);

	g_object_class_install_properties(gobject_class,
					  SND_UNIT_PROP_TYPE_COUNT,
					  snd_unit_props);

	/**
	 * HinawaSndUnit::lock-status:
	 * @self: A [class@SndUnit]
	 * @state: %TRUE when locked, %FALSE when unlocked.
	 *
	 * Emitted when corresponding ALSA driver changes status of lock by starting/stopping
	 * packet streaming requested by ALSA PCM/Rawmidi applications.
	 *
	 * Since: 0.3
	 * Deprecated: 2.5. Use change notify signal of [property@Hitaki.AlsaFirewire:is-locked]
	 *	       instead.
	 */
	snd_unit_sigs[SND_UNIT_SIG_TYPE_LOCK_STATUS] =
		g_signal_new("lock-status",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST | G_SIGNAL_DEPRECATED,
			     G_STRUCT_OFFSET(HinawaSndUnitClass, lock_status),
			     NULL, NULL,
			     g_cclosure_marshal_VOID__BOOLEAN,
			     G_TYPE_NONE, 1, G_TYPE_BOOLEAN);

	/**
	 * HinawaSndUnit::disconnected:
	 * @self: A [class@SndUnit]
	 *
	 * Emitted when the sound card is not available anymore due to unbinding driver or hot
	 * unplugging. The owner of this object should call [method@GObject.Object.unref] as
	 * quickly as possible to release ALSA hwdep character device.
	 *
	 * Since: 2.0
	 * Deprecated: 2.5. Use change notify signal of
	 *	       [property@Hitaki.AlsaFirewire:is-disconnected] instead.
	 */
	snd_unit_sigs[SND_UNIT_SIG_TYPE_DISCONNECTED] =
		g_signal_new("disconnected",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST | G_SIGNAL_DEPRECATED,
			     G_STRUCT_OFFSET(HinawaSndUnitClass, disconnected),
			     NULL, NULL,
			     g_cclosure_marshal_VOID__VOID,
			     G_TYPE_NONE, 0, G_TYPE_NONE, 0);
}

static void hinawa_snd_unit_init(HinawaSndUnit *self)
{
	HinawaSndUnitPrivate *priv = hinawa_snd_unit_get_instance_private(self);

	priv->fd = -1;
	priv->node = g_object_new(HINAWA_TYPE_FW_NODE, NULL);
}

/**
 * hinawa_snd_unit_new:
 *
 * Instantiate [class@SndUnit] object and return the instance.
 *
 * Returns: an instance of [class@SndUnit].
 *
 * Since: 1.3.
 * Deprecated: 2.5. Use [method@Hitaki.SndUnit.new] instead.
 */
HinawaSndUnit *hinawa_snd_unit_new(void)
{
	return g_object_new(HINAWA_TYPE_SND_UNIT, NULL);
}

/**
 * hinawa_snd_unit_open:
 * @self: A [class@SndUnit]
 * @path: A full path of a special file for ALSA hwdep character device
 * @error: A [struct@GLib.Error]. Error can be generated with three domains; GLib.FileError,
 *	   Hinawa.FwNodeError, and Hinawa.SndUnitError.
 *
 * Open ALSA hwdep character device and check it for FireWire sound devices.
 *
 * Since: 0.4
 * Deprecated: 2.5. Use implementation of [method@Hitaki.AlsaFirewire.open] in
 *	       [class@Hitaki.SndUnit] instead.
 */
void hinawa_snd_unit_open(HinawaSndUnit *self, gchar *path, GError **error)
{
	HinawaSndUnitPrivate *priv;
	char fw_cdev[32];

	g_return_if_fail(HINAWA_IS_SND_UNIT(self));
	g_return_if_fail(path != NULL && strlen(path) > 0);
	g_return_if_fail(error == NULL || *error == NULL);

	priv = hinawa_snd_unit_get_instance_private(self);
	if (priv->fd >= 0) {
		generate_local_error(error, HINAWA_SND_UNIT_ERROR_OPENED);
		return;
	}

	priv->fd = open(path, O_RDWR);
	if (priv->fd < 0) {
		if (errno == ENODEV) {
			generate_local_error(error, HINAWA_SND_UNIT_ERROR_DISCONNECTED);
		} else if (errno == EBUSY) {
			generate_local_error(error, HINAWA_SND_UNIT_ERROR_USED);
		} else {
			GFileError code = g_file_error_from_errno(errno);

			if (code != G_FILE_ERROR_FAILED)
				generate_file_error(error, code, "open(%s)", path);
			else
				generate_syscall_error(error, errno, "open(%s)", path);
		}
		return;
	}

	/* Get FireWire sound device information. */
	if (ioctl(priv->fd, SNDRV_FIREWIRE_IOCTL_GET_INFO, &priv->info) < 0) {
		if (errno == ENODEV)
			generate_local_error(error, HINAWA_SND_UNIT_ERROR_DISCONNECTED);
		else
			generate_syscall_error(error, errno, "ioctl(%s)", "SNDRV_FIREWIRE_IOCTL_GET_INFO");
		goto end;
	}

	if (HINAWA_IS_SND_DICE(self) && priv->info.type != SNDRV_FIREWIRE_TYPE_DICE) {
		generate_local_error(error, HINAWA_SND_UNIT_ERROR_WRONG_CLASS);
		goto end;
	}
	if (HINAWA_IS_SND_EFW(self) && priv->info.type != SNDRV_FIREWIRE_TYPE_FIREWORKS) {
		generate_local_error(error, HINAWA_SND_UNIT_ERROR_WRONG_CLASS);
		goto end;
	}
	if (HINAWA_IS_SND_DG00X(self) && priv->info.type != SNDRV_FIREWIRE_TYPE_DIGI00X) {
		generate_local_error(error, HINAWA_SND_UNIT_ERROR_WRONG_CLASS);
		goto end;
	}
	if (HINAWA_IS_SND_MOTU(self) && priv->info.type != SNDRV_FIREWIRE_TYPE_MOTU) {
		generate_local_error(error, HINAWA_SND_UNIT_ERROR_WRONG_CLASS);
		goto end;
	}
	if (HINAWA_IS_SND_TSCM(self) && priv->info.type != SNDRV_FIREWIRE_TYPE_TASCAM) {
		generate_local_error(error, HINAWA_SND_UNIT_ERROR_WRONG_CLASS);
		goto end;
	}
	snprintf(fw_cdev, sizeof(fw_cdev), "/dev/%s", priv->info.device_name);
	hinawa_fw_node_open(priv->node, fw_cdev, error);
end:
	if (*error != NULL) {
		close(priv->fd);
		priv->fd = -1;
	}
}

/**
 * hinawa_snd_unit_get_node:
 * @self: A [class@SndUnit].
 * @node: (out)(transfer none): A [class@FwNode].
 *
 * Retrieve an instance of [class@FwNode] associated to the given unit.
 *
 * Since: 2.0.
 * Deprecated: 2.5. Instantiate [class@FwNode] according to
 *	       [property@Hitaki.AlsaFirewire:node-device] instead.
 */
void hinawa_snd_unit_get_node(HinawaSndUnit *self, HinawaFwNode **node)
{
	HinawaSndUnitPrivate *priv;

	g_return_if_fail(HINAWA_IS_SND_UNIT(self));
	g_return_if_fail(node != NULL);

	priv = hinawa_snd_unit_get_instance_private(self);
	g_return_if_fail(priv->fd >= 0);

	*node = priv->node;
}
/**
 * hinawa_snd_unit_lock:
 * @self: A [class@SndUnit]
 * @error: A [struct@GLib.Error]. Error can be generated with domain of Hinawa.SndUnitError.
 *
 * Disallow corresponding ALSA driver to start packet streaming.
 *
 * Since: 0.3
 * Deprecated: 2.5. Use implementation of [method@Hitaki.AlsaFirewire.lock] in
 *	       [class@Hitaki.SndUnit] instead.
 */
void hinawa_snd_unit_lock(HinawaSndUnit *self, GError **error)
{
	HinawaSndUnitPrivate *priv;

	g_return_if_fail(HINAWA_IS_SND_UNIT(self));
	g_return_if_fail(error == NULL || *error == NULL);

	priv = hinawa_snd_unit_get_instance_private(self);
	if (priv->fd < 0) {
		generate_local_error(error, HINAWA_SND_UNIT_ERROR_NOT_OPENED);
		return;
	}

	if (ioctl(priv->fd, SNDRV_FIREWIRE_IOCTL_LOCK, NULL) < 0) {
		if (errno == ENODEV)
			generate_local_error(error, HINAWA_SND_UNIT_ERROR_DISCONNECTED);
		else if (errno == EBUSY)
			generate_local_error(error, HINAWA_SND_UNIT_ERROR_LOCKED);
		else
			generate_syscall_error(error, errno, "ioctl(%s)", "SNDRV_FIREWIRE_IOCTL_LOCK");
	}
}

/**
 * hinawa_snd_unit_unlock:
 * @self: A [class@SndUnit]
 * @error: A [struct@GLib.Error]. Error can be generated with domain of Hinawa.SndUnitError.
 *
 * Allow corresponding ALSA driver to start packet streaming.
 *
 * Since: 0.3
 * Deprecated: 2.5. Use implementation of [method@Hitaki.AlsaFirewire.unlock] in
 *	       [class@Hitaki.SndUnit] instead.
 */
void hinawa_snd_unit_unlock(HinawaSndUnit *self, GError **error)
{
	HinawaSndUnitPrivate *priv;

	g_return_if_fail(HINAWA_IS_SND_UNIT(self));
	g_return_if_fail(error == NULL || *error == NULL);

	priv = hinawa_snd_unit_get_instance_private(self);
	if (priv->fd < 0) {
		generate_local_error(error, HINAWA_SND_UNIT_ERROR_NOT_OPENED);
		return;
	}

	if (ioctl(priv->fd, SNDRV_FIREWIRE_IOCTL_UNLOCK, NULL) < 0) {
		if (errno == ENODEV)
			generate_local_error(error, HINAWA_SND_UNIT_ERROR_DISCONNECTED);
		else if (errno == EBADFD)
			generate_local_error(error, HINAWA_SND_UNIT_ERROR_UNLOCKED);
		else
			generate_syscall_error(error, errno, "ioctl(%s)", "SNDRV_FIREWIRE_IOCTL_UNLOCK");
	}
}

/* For internal use. */
void hinawa_snd_unit_write(HinawaSndUnit *self, const void *buf, size_t length,
			   GError **error)
{
	HinawaSndUnitPrivate *priv;
	ssize_t len;

	g_return_if_fail(HINAWA_IS_SND_UNIT(self));
	g_return_if_fail(error == NULL || *error == NULL);

	priv = hinawa_snd_unit_get_instance_private(self);
	if (priv->fd < 0) {
		generate_local_error(error, HINAWA_SND_UNIT_ERROR_NOT_OPENED);
		return;
	}

	len = write(priv->fd, buf, length);
	if (len < 0) {
		if (errno == ENODEV)
			generate_local_error(error, HINAWA_SND_UNIT_ERROR_DISCONNECTED);
		else
			generate_syscall_error(error, errno, "write: %zu", length);
	} else if (len != length){
		generate_syscall_error(error, EIO, "write: %zu", length);
	}
}

void hinawa_snd_unit_ioctl(HinawaSndUnit *self, unsigned long request,
			   void *arg, GError **error)
{
	HinawaSndUnitPrivate *priv;

	g_return_if_fail(HINAWA_IS_SND_UNIT(self));
	g_return_if_fail(error == NULL || *error == NULL);

	priv = hinawa_snd_unit_get_instance_private(self);
	if (priv->fd < 0) {
		generate_local_error(error, HINAWA_SND_UNIT_ERROR_NOT_OPENED);
		return;
	}

	if (ioctl(priv->fd, request, arg) < 0) {
		if (errno == ENODEV) {
			generate_local_error(error, HINAWA_SND_UNIT_ERROR_DISCONNECTED);
		} else {
			const char *arg;

			switch (request) {
			case SNDRV_FIREWIRE_IOCTL_TASCAM_STATE:
				arg = "SNDRV_FIREWIRE_IOCTL_TASCAM_STATE";
				break;
			case SNDRV_FIREWIRE_IOCTL_MOTU_REGISTER_DSP_METER:
				arg = "SNDRV_FIREWIRE_IOCTL_MOTU_REGISTER_DSP_METER";
				break;
			case SNDRV_FIREWIRE_IOCTL_MOTU_COMMAND_DSP_METER:
				arg = "SNDRV_FIREWIRE_IOCTL_MOTU_COMMAND_DSP_METER";
				break;
			case SNDRV_FIREWIRE_IOCTL_MOTU_REGISTER_DSP_PARAMETER:
				arg = "SNDRV_FIREWIRE_IOCTL_MOTU_REGISTER_DSP_PARAMETER";
				break;
			default:
				arg = "Unknown";
				break;
			}

			generate_syscall_error(error, errno, "ioctl(%s)", arg);
		}
	}
}

static void handle_lock_event(HinawaSndUnit *self,
			      const void *buf, ssize_t length)
{
	HinawaSndUnitPrivate *priv = hinawa_snd_unit_get_instance_private(self);
	const struct snd_firewire_event_lock_status *event = buf;

	priv->streaming = event->status;

	g_signal_emit(self, snd_unit_sigs[SND_UNIT_SIG_TYPE_LOCK_STATUS], 0,
		      event->status);
}

static gboolean check_src(GSource *gsrc)
{
	SndUnitSource *src = (SndUnitSource *)gsrc;
	GIOCondition condition;

	// Don't go to dispatch if nothing available. As an error, return
	// TRUE for POLLERR to call .dispatch for internal destruction.
	condition = g_source_query_unix_fd(gsrc, src->tag);
	return !!(condition & (G_IO_IN | G_IO_ERR));
}

static gboolean dispatch_src(GSource *gsrc, GSourceFunc cb, gpointer user_data)
{
	SndUnitSource *src = (SndUnitSource *)gsrc;
	HinawaSndUnit *unit = src->unit;
	HinawaSndUnitPrivate *priv;
	GIOCondition condition;
	struct snd_firewire_event_common *common;
	ssize_t len;

	priv = hinawa_snd_unit_get_instance_private(unit);
	if (priv->fd < 0)
		return G_SOURCE_REMOVE;

	condition = g_source_query_unix_fd(gsrc, src->tag);
	if (condition & G_IO_ERR) {
		g_signal_emit(unit,
			      snd_unit_sigs[SND_UNIT_SIG_TYPE_DISCONNECTED], 0,
			      NULL);

		return G_SOURCE_REMOVE;
	}

	len = read(priv->fd, src->buf, src->len);
	if (len <= 0)
		goto end;

	common = (struct snd_firewire_event_common *)src->buf;

	if (common->type == SNDRV_FIREWIRE_EVENT_LOCK_STATUS)
		handle_lock_event(unit, src->buf, len);
	else if (HINAWA_IS_SND_DICE(unit) &&
		 common->type == SNDRV_FIREWIRE_EVENT_DICE_NOTIFICATION)
		hinawa_snd_dice_handle_notification(HINAWA_SND_DICE(unit),
						    src->buf, len);
	else if (HINAWA_IS_SND_EFW(unit) &&
		 common->type == SNDRV_FIREWIRE_EVENT_EFW_RESPONSE)
		hinawa_snd_efw_handle_response(HINAWA_SND_EFW(unit),
					       src->buf, len);
	else if (HINAWA_IS_SND_DG00X(unit) &&
		 common->type == SNDRV_FIREWIRE_EVENT_DIGI00X_MESSAGE)
		hinawa_snd_dg00x_handle_msg(HINAWA_SND_DG00X(unit),
					    src->buf, len);
	else if (HINAWA_IS_SND_MOTU(unit)) {
		HinawaSndMotu *motu = HINAWA_SND_MOTU(unit);

		if (common->type == SNDRV_FIREWIRE_EVENT_MOTU_NOTIFICATION) {
			hinawa_snd_motu_handle_notification(motu, src->buf, len);
		} else if (common->type == SNDRV_FIREWIRE_EVENT_MOTU_REGISTER_DSP_CHANGE) {
			hinawa_snd_motu_handle_register_dsp_change(motu, src->buf, len);
		}
	}
	else if (HINAWA_IS_SND_TSCM(unit) &&
		 common->type == SNDRV_FIREWIRE_EVENT_TASCAM_CONTROL)
		hinawa_snd_tscm_handle_control(HINAWA_SND_TSCM(unit),
						    src->buf, len);
end:
	/* Just be sure to continue to process this source. */
	return G_SOURCE_CONTINUE;
}

static void finalize_src(GSource *gsrc)
{
	SndUnitSource *src = (SndUnitSource *)gsrc;
	HinawaSndUnitPrivate *priv =
				hinawa_snd_unit_get_instance_private(src->unit);

	if (priv->streaming)
		ioctl(priv->fd, SNDRV_FIREWIRE_IOCTL_UNLOCK, NULL);

	g_free(src->buf);
}

/**
 * hinawa_snd_unit_create_source:
 * @self: A [class@SndUnit].
 * @gsrc: (out): A [struct@GLib.Source].
 * @error: A [struct@GLib.Error]. Error can be generated with domain with Hinawa.SndUnitError.
 *
 * Create [struct@GLib.Source] for [struct@GLib.MainContext] to dispatch events for the sound
 * device.
 *
 * Since: 1.4.
 * Deprecated: 2.5. Use implementation of [method@Hitaki.AlsaFirewire.create_source] in
 *	       [class@Hitaki.SndUnit] instead.
 */
void hinawa_snd_unit_create_source(HinawaSndUnit *self, GSource **gsrc,
				   GError **error)
{
	static GSourceFuncs funcs = {
		.check		= check_src,
		.dispatch	= dispatch_src,
		.finalize	= finalize_src,
	};
	HinawaSndUnitPrivate *priv;
	SndUnitSource *src;

	g_return_if_fail(HINAWA_IS_SND_UNIT(self));
	g_return_if_fail(gsrc != NULL);
	g_return_if_fail(error == NULL || *error == NULL);

	priv = hinawa_snd_unit_get_instance_private(self);
	if (priv->fd < 0) {
		generate_local_error(error, HINAWA_SND_UNIT_ERROR_NOT_OPENED);
		return;
	}

	*gsrc = g_source_new(&funcs, sizeof(SndUnitSource));

	g_source_set_name(*gsrc, "HinawaSndUnit");
	g_source_set_priority(*gsrc, G_PRIORITY_HIGH_IDLE);
	g_source_set_can_recurse(*gsrc, TRUE);

	// MEMO: allocate one page because we cannot assume the size of
	// transaction frame.
	src = (SndUnitSource *)(*gsrc);
	src->len = sysconf(_SC_PAGESIZE);
	src->buf = g_malloc(src->len);

	src->unit = self;
	src->tag = g_source_add_unix_fd(*gsrc, priv->fd, G_IO_IN);

	// Check locked or not.
	if (ioctl(priv->fd, SNDRV_FIREWIRE_IOCTL_LOCK, NULL) < 0) {
		if (errno == EBUSY)
			priv->streaming = true;
		return;
	}
	ioctl(priv->fd, SNDRV_FIREWIRE_IOCTL_UNLOCK, NULL);
}
