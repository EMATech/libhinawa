// SPDX-License-Identifier: LGPL-2.1-or-later
#include "internal.h"

#include <string.h>
#include <errno.h>

/**
 * HinawaSndTscm:
 * A state reader for Tascam FireWire models
 *
 * A [class@SndTscm] is an application of protocol defined by TASCAM.
 *
 * Deprecated: 2.5. libhitaki library provides [class@Hitaki.SndTascam] as the alternative.
 */

typedef struct {
	struct snd_firewire_tascam_state image;
	guint32 state[SNDRV_FIREWIRE_TASCAM_STATE_COUNT];
} HinawaSndTscmPrivate;
G_DEFINE_TYPE_WITH_PRIVATE(HinawaSndTscm, hinawa_snd_tscm, HINAWA_TYPE_SND_UNIT)

/* This object has one signal. */
enum tscm_sig_type {
        TSCM_SIG_TYPE_CTL,
        TSCM_SIG_TYPE_COUNT,
};
static guint tscm_sigs[TSCM_SIG_TYPE_COUNT] = { 0 };

static void hinawa_snd_tscm_class_init(HinawaSndTscmClass *klass)
{
	/**
	 * HinawaSndTscm::control:
	 * @self: A [class@SndTscm]
	 * @index: the numeric index on image of status and control info.
	 * @before: the value of info before changed.
	 * @after: the value of info after changed.
	 *
	 * Emitted when TASCAM FireWire unit transfer control message.
	 * is emitted.
	 *
	 * Since: 1.1
	 * Deprecated: 2.5. Use implementation of [signal@Hitaki.TascamProtocol::changed] in
	 *	       [class@Hitaki.SndTascam] instead.
	 */
	tscm_sigs[TSCM_SIG_TYPE_CTL] =
		g_signal_new("control",
			     G_OBJECT_CLASS_TYPE(klass),
			     G_SIGNAL_RUN_LAST | G_SIGNAL_DEPRECATED,
			     G_STRUCT_OFFSET(HinawaSndTscmClass, control),
			     NULL, NULL,
			     hinawa_sigs_marshal_VOID__UINT_UINT_UINT,
			     G_TYPE_NONE,
			     3, G_TYPE_UINT, G_TYPE_UINT, G_TYPE_UINT);
}

static void hinawa_snd_tscm_init(HinawaSndTscm *self)
{
	return;
}

/**
 * hinawa_snd_tscm_new:
 *
 * Instantiate [class@SndTscm] object and return the instance.
 *
 * Returns: an instance of [class@SndTscm].
 *
 * Since: 1.3.
 * Deprecated: 2.5. Use [method@Hitaki.SndTascam.new] instead.
 */
HinawaSndTscm *hinawa_snd_tscm_new(void)
{
	return g_object_new(HINAWA_TYPE_SND_TSCM, NULL);
}

/**
 * hinawa_snd_tscm_open:
 * @self: A [class@SndTscm]
 * @path: A full path of a special file for ALSA hwdep character device
 * @error: A [struct@GLib.Error]. Error can be generated with three domains; GLib.FileError,
 *	   Hinawa.FwNodeError, and Hinawa.SndUnitError.
 *
 * Open ALSA hwdep character device and check it for Tascam devices.
 *
 * Since: 1.1
 * Deprecated: 2.5. Use implementation of [method@Hitaki.AlsaFirewire.open] in
 *	       [class@Hitaki.SndTascam] instead.
 */
void hinawa_snd_tscm_open(HinawaSndTscm *self, gchar *path, GError **error)
{
	g_return_if_fail(HINAWA_IS_SND_TSCM(self));
	g_return_if_fail(path != NULL && strlen(path) > 0);
	g_return_if_fail(error == NULL || *error == NULL);

	hinawa_snd_unit_open(&self->parent_instance, path, error);
}

/**
 * hinawa_snd_tscm_get_state:
 * @self: A [class@SndTscm]
 * @error: A [struct@GLib.Error]. Error can be generated with domain of Hinawa.SndUnitError.
 *
 * Get the latest states of target device.
 *
 * Returns: (element-type guint32) (array fixed-size=64) (transfer none): state image.
 *
 * Since: 1.1
 * Deprecated: 2.5. Use implementation of [method@Hitaki.TascamProtocol.read_state] in
 *	       [class@Hitaki.SndTascam] instead.
 */
const guint32 *hinawa_snd_tscm_get_state(HinawaSndTscm *self, GError **error)
{
	HinawaSndTscmPrivate *priv;
	int i;

	g_return_val_if_fail(HINAWA_IS_SND_TSCM(self), NULL);
	g_return_val_if_fail(error == NULL || *error == NULL, NULL);

	priv = hinawa_snd_tscm_get_instance_private(self);

	hinawa_snd_unit_ioctl(&self->parent_instance,
			      SNDRV_FIREWIRE_IOCTL_TASCAM_STATE, &priv->image,
			      error);
	if (*error != NULL)
		return NULL;

	for (i = 0; i < SNDRV_FIREWIRE_TASCAM_STATE_COUNT; ++i)
		priv->state[i] = GUINT32_FROM_BE(priv->image.data[i]);
	return priv->state;
}

void hinawa_snd_tscm_handle_control(HinawaSndTscm *self, const void *buf,
				    ssize_t len)
{
	const struct snd_firewire_event_tascam_control *event =
			(struct snd_firewire_event_tascam_control *)buf;
	const struct snd_firewire_tascam_change *change;

	g_return_if_fail(HINAWA_IS_SND_TSCM(self));

	if (len < sizeof(event->type))
		return;
	len -= sizeof(event->type);

	change = event->changes;
	while (len >= sizeof(*change)) {
		g_signal_emit(self, tscm_sigs[TSCM_SIG_TYPE_CTL], 0,
			      change->index,
			      GUINT32_FROM_BE(change->before),
			      GUINT32_FROM_BE(change->after));
		++change;
		len -= sizeof(*change);
	}
}
