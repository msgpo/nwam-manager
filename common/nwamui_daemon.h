/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set expandtab ts=4 shiftwidth=4: */
/* 
 * Copyright 2007-2009 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 *
 * CDDL HEADER START
 * 
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 * 
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 * 
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 * 
 * CDDL HEADER END
 * 
 * File:   nwamui_daemon.h
 *
 */

#ifndef _NWAMUI_DAEMON_H
#define	_NWAMUI_DAEMON_H

#ifndef _libnwamui_H
#error "Please include libnwamui.h header instead."
#endif

G_BEGIN_DECLS

#define NWAMUI_TYPE_DAEMON               (nwamui_daemon_get_type ())
#define NWAMUI_DAEMON(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), NWAMUI_TYPE_DAEMON, NwamuiDaemon))
#define NWAMUI_DAEMON_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), NWAMUI_TYPE_DAEMON, NwamuiDaemonClass))
#define NWAMUI_IS_DAEMON(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NWAMUI_TYPE_DAEMON))
#define NWAMUI_IS_DAEMON_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), NWAMUI_TYPE_DAEMON))
#define NWAMUI_DAEMON_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), NWAMUI_TYPE_DAEMON, NwamuiDaemonClass))


typedef struct _NwamuiDaemon		      NwamuiDaemon;
typedef struct _NwamuiDaemonClass        NwamuiDaemonClass;
typedef struct _NwamuiDaemonPrivate	  NwamuiDaemonPrivate;

typedef enum {
    NWAMUI_DAEMON_STATUS_UNINITIALIZED = 0,
    NWAMUI_DAEMON_STATUS_ALL_OK,
    NWAMUI_DAEMON_STATUS_NEEDS_ATTENTION,
    NWAMUI_DAEMON_STATUS_ERROR,
    NWAMUI_DAEMON_STATUS_LAST
} nwamui_daemon_status_t;

typedef enum {
    NWAMUI_DAEMON_INFO_UNKNOWN,
    NWAMUI_DAEMON_INFO_ERROR,
    NWAMUI_DAEMON_INFO_INACTIVE,
    NWAMUI_DAEMON_INFO_ACTIVE,
    NWAMUI_DAEMON_INFO_RAW,
    NWAMUI_DAEMON_INFO_WLANS_CHANGED,
    NWAMUI_DAEMON_INFO_WLAN_CONNECTED,
    NWAMUI_DAEMON_INFO_WLAN_DISCONNECTED, /* unused */
    NWAMUI_DAEMON_INFO_WLAN_CONNECT_FAILED,
    NWAMUI_DAEMON_INFO_WIFI_SELECTION_NEEDED,
    NWAMUI_DAEMON_INFO_WIFI_KEY_NEEDED,
    NWAMUI_DAEMON_INFO_GENERIC,
} nwamui_daemon_info_t;


typedef enum {
    NWAMUI_DAEMON_EVENT_CAUSE_NONE,
    NWAMUI_DAEMON_EVENT_CAUSE_DHCP_DOWN,
    NWAMUI_DAEMON_EVENT_CAUSE_DHCP_TIMER,
    NWAMUI_DAEMON_EVENT_CAUSE_UNPLUGGED,
    NWAMUI_DAEMON_EVENT_CAUSE_USER_PRIO,
    NWAMUI_DAEMON_EVENT_CAUSE_HIGHER_PRIO_AVAIL,
    NWAMUI_DAEMON_EVENT_CAUSE_NEW_AP,
    NWAMUI_DAEMON_EVENT_CAUSE_AP_GONE,
    NWAMUI_DAEMON_EVENT_CAUSE_AP_FADED,
    NWAMUI_DAEMON_EVENT_CAUSE_ALL_BUT_ONE_DOWN,
    NWAMUI_DAEMON_EVENT_CAUSE_NOT_WANTED,
    NWAMUI_DAEMON_EVENT_CAUSE_DAEMON_SHUTTING_DOWN,
    NWAMUI_DAEMON_EVENT_CAUSE_DIFFERENT_AP_SELECTED,
    NWAMUI_DAEMON_EVENT_CAUSE_HW_REMOVED,
    NWAMUI_DAEMON_EVENT_CAUSE_UNKNOWN,
    NWAMUI_DAEMON_EVENT_CAUSE_LAST /* Not to be used directly */
} nwamui_daemon_event_cause_t;


struct _NwamuiDaemon
{
	NwamuiObject                      object;

	/*< private >*/
	NwamuiDaemonPrivate    *prv;
};

struct _NwamuiDaemonClass
{
	NwamuiObjectClass                parent_class;
        
	void (*daemon_info)             (NwamuiDaemon *self, gint info, GObject *obj, gpointer data, gpointer user_data);
	void (*wifi_selection_needed)   (NwamuiDaemon *self, NwamuiNcu* ncu, gpointer user_data);
    void (*add_wifi_fav)            (NwamuiDaemon *self, NwamuiWifiNet* new_wifi, gpointer user_data);
    void (*remove_wifi_fav)         (NwamuiDaemon *self, NwamuiWifiNet* new_wifi, gpointer user_data);
    void (*wifi_key_needed)         (NwamuiDaemon *self, NwamuiWifiNet* wifi_net, gpointer user_data);
    void (*wifi_scan_started)       (NwamuiDaemon *self, gpointer user_data);
    void (*wifi_scan_result)        (NwamuiDaemon *self, NwamuiWifiNet* wifi_net, gpointer user_data);
	
};

extern  GType                       nwamui_daemon_get_type (void) G_GNUC_CONST;

extern NwamuiDaemon*                nwamui_daemon_get_instance (void);

extern nwamui_daemon_status_t       nwamui_daemon_get_status( NwamuiDaemon* self );

extern NwamuiObject*                nwamui_daemon_get_ncp_by_name( NwamuiDaemon *self, const gchar* name );

extern NwamuiObject*                nwamui_daemon_get_env_by_name( NwamuiDaemon *self, const gchar* name );

extern NwamuiObject*                nwamui_daemon_get_enm_by_name( NwamuiDaemon *self, const gchar* name );

extern NwamuiObject*                nwamui_daemon_get_active_ncp(NwamuiDaemon *self);

extern gchar*                       nwamui_daemon_get_active_ncp_name(NwamuiDaemon *self);

extern gboolean                     nwamui_daemon_is_active_ncp(NwamuiDaemon *self, NwamuiObject* ncp ) ;

extern NwamuiNcu*                   nwamui_ncp_get_first_wireless_ncu_from_active_ncp(NwamuiDaemon *self);

extern gboolean                     nwamui_daemon_env_selection_is_manual(NwamuiDaemon *self);

extern gboolean                     nwamui_daemon_is_active_env(NwamuiDaemon *self, NwamuiObject* env ) ;

extern NwamuiEnv*                   nwamui_daemon_get_active_env(NwamuiDaemon *self);

extern gchar*                       nwamui_daemon_get_active_env_name(NwamuiDaemon *self);

extern GtkTreeModel *               nwamui_get_default_svcs (NwamuiDaemon *self);

extern gint                         nwamui_daemon_get_online_enm_num(NwamuiDaemon *self);

extern void                         nwamui_daemon_wifi_start_scan(NwamuiDaemon *self);

extern void                         nwamui_daemon_dispatch_wifi_scan_events_from_cache(NwamuiDaemon* daemon);

extern gint                         nwamui_daemon_get_num_scanned_wifi(NwamuiDaemon* self );

extern NwamuiObject*                nwamui_daemon_find_fav_wifi_net_by_name(NwamuiDaemon *self, const gchar* name );

extern nwamui_daemon_status_t       nwamui_daemon_get_status_icon_type( NwamuiDaemon *daemon );

extern const gchar*                 nwamui_deamon_status_to_string( nwamui_daemon_status_t status );

extern void                         nwamui_daemon_foreach_ncp(NwamuiDaemon *self, GFunc func, gpointer user_data);
extern void                         nwamui_daemon_foreach_loc(NwamuiDaemon *self, GFunc func, gpointer user_data);
extern void                         nwamui_daemon_foreach_enm(NwamuiDaemon *self, GFunc func, gpointer user_data);
extern void                         nwamui_daemon_foreach_fav_wifi(NwamuiDaemon *self, GFunc func, gpointer user_data);
extern GList*                       nwamui_daemon_find_ncp(NwamuiDaemon *self, GCompareFunc func, gconstpointer user_data);
extern GList*                       nwamui_daemon_find_loc(NwamuiDaemon *self, GCompareFunc func, gconstpointer user_data);
extern GList*                       nwamui_daemon_find_enm(NwamuiDaemon *self, GCompareFunc func, gconstpointer user_data);
extern GList*                       nwamui_daemon_find_fav_wifi(NwamuiDaemon *self, GCompareFunc func, gconstpointer user_data);

G_END_DECLS

#endif	/* _NWAMUI_DAEMON_H */

