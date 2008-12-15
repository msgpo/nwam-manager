/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set expandtab ts=4 shiftwidth=4: */
/* 
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
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
 * File:   nwamui_ncu.c
 *
 */

#include <glib-object.h>
#include <glib/gi18n.h>
#include <gtk/gtkliststore.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>

#include "libnwamui.h"
#include "nwamui_ncu.h"

static GObjectClass *parent_class = NULL;

struct _NwamuiNcuPrivate {
        nwam_ncu_handle_t               nwam_ncu;
        NwamuiNcp*                      ncp;  /* Parent NCP */

        /* General Properties */
        gchar*                          vanity_name;
        gchar*                          device_name;

        /* Link Properties */
        gchar*                          phy_address;
        gchar**                         autopush;
        guint64                         mtu;


        /* IPTun Properties */
        nwam_iptun_type_t               tun_type; 
        gchar*                          tun_tsrc;
        gchar*                          tun_tdst;
        gchar*                          tun_encr;
        gchar*                          tun_encr_auth;
        gchar*                          tun_auth;

        nwamui_ncu_type_t               ncu_type;
        gboolean                        active;
        guint                           speed;
        NwamuiIp*                       ipv4_primary_ip;
        gchar*                          ipv4_gateway;
        gboolean                        ipv6_active;
        NwamuiIp*                       ipv6_primary_ip;
        GtkListStore*                   v4addresses;
        GtkListStore*                   v6addresses;
        NwamuiWifiNet*                  wifi_info;
        nwamui_cond_activation_mode_t   activiation_mode;
        gboolean                        enabled;
        gint                            priority_group;
        nwamui_cond_priority_group_mode_t    
                                        priority_group_mode;
        gpointer                        conditions;
};

enum {
        PROP_NWAM_NCU = 1,
        PROP_NCP,
        PROP_VANITY_NAME,
        PROP_DEVICE_NAME,
        PROP_PHY_ADDRESS,
        PROP_NCU_TYPE,
        PROP_ACTIVE,
        PROP_SPEED,
        PROP_IPV4_AUTO_CONF,
        PROP_IPV4_ADDRESS,
        PROP_IPV4_SUBNET,
        PROP_IPV4_GATEWAY,
        PROP_IPV6_ACTIVE,
        PROP_IPV6_AUTO_CONF,
        PROP_IPV6_ADDRESS,
        PROP_IPV6_PREFIX,
        PROP_V4ADDRESSES,
        PROP_V6ADDRESSES,
        PROP_WIFI_INFO,
        PROP_RULES_ENABLED,
        PROP_ACTIVIATION_MODE,
        PROP_ENABLED,
        PROP_PRIORITY_GROUP,
        PROP_PRIORITY_GROUP_MODE,
        PROP_CONDITIONS
};

static void nwamui_ncu_set_property ( GObject         *object,
                                      guint            prop_id,
                                      const GValue    *value,
                                      GParamSpec      *pspec);

static void nwamui_ncu_get_property ( GObject         *object,
                                      guint            prop_id,
                                      GValue          *value,
                                      GParamSpec      *pspec);

static void nwamui_ncu_finalize (     NwamuiNcu *self);

static gboolean     get_nwam_ncu_boolean_prop( nwam_ncu_handle_t ncu, const char* prop_name );

static gchar*       get_nwam_ncu_string_prop( nwam_ncu_handle_t ncu, const char* prop_name );

static gchar**      get_nwam_ncu_string_array_prop( nwam_ncu_handle_t ncu, const char* prop_name );

static guint64      get_nwam_ncu_uint64_prop( nwam_ncu_handle_t ncu, const char* prop_name );

static guint64*     get_nwam_ncu_uint64_array_prop( nwam_ncu_handle_t ncu, const char* prop_name , guint* out_num );

/* Callbacks */
static void object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data);

static void ip_row_inserted_or_changed_cb (GtkTreeModel *tree_model, GtkTreePath *path, GtkTreeIter *iter, gpointer user_data); 

static void ip_row_deleted_cb (GtkTreeModel *tree_model, GtkTreePath *path, gpointer user_data);


G_DEFINE_TYPE (NwamuiNcu, nwamui_ncu, G_TYPE_OBJECT)

static void
nwamui_ncu_class_init (NwamuiNcuClass *klass)
{
    /* Pointer to GObject Part of Class */
    GObjectClass *gobject_class = (GObjectClass*) klass;
        
    /* Initialise Static Parent Class pointer */
    parent_class = g_type_class_peek_parent (klass);

    /* Override Some Function Pointers */
    gobject_class->set_property = nwamui_ncu_set_property;
    gobject_class->get_property = nwamui_ncu_get_property;
    gobject_class->finalize = (void (*)(GObject*)) nwamui_ncu_finalize;

    /* Create some properties */
    g_object_class_install_property (gobject_class,
                                     PROP_NWAM_NCU,
                                     g_param_spec_pointer ("nwam_ncu",
                                                           _("Nwam Ncu handle"),
                                                           _("Nwam Ncu handle"),
                                                           G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE));

    g_object_class_install_property (gobject_class,
                                     PROP_NCP,
                                     g_param_spec_object ("ncp",
                                                          _("NCP that NCU child of"),
                                                          _("NCP that NCU child of"),
                                                          NWAMUI_TYPE_NCP,
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_VANITY_NAME,
                                     g_param_spec_string ("vanity_name",
                                                          _("vanity_name"),
                                                          _("vanity_name"),
                                                          "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_DEVICE_NAME,
                                     g_param_spec_string ("device_name",
                                                          _("device_name"),
                                                          _("device_name"),
                                                          "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_PHY_ADDRESS,
                                     g_param_spec_string ("phy_address",
                                                          _("phy_address"),
                                                          _("phy_address"),
                                                          "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_NCU_TYPE,
                                     g_param_spec_int   ("ncu_type",
                                                         _("ncu_type"),
                                                         _("ncu_type"),
                                                          NWAMUI_NCU_TYPE_WIRED,
                                                          NWAMUI_NCU_TYPE_LAST-1,
                                                          NWAMUI_NCU_TYPE_WIRED,
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_ACTIVE,
                                     g_param_spec_boolean ("active",
                                                          _("active"),
                                                          _("active"),
                                                          FALSE,
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_SPEED,
                                     g_param_spec_uint ("speed",
                                                       _("speed"),
                                                       _("speed"),
                                                       0,
                                                       G_MAXUINT,
                                                       0,
                                                       G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_IPV4_AUTO_CONF,
                                     g_param_spec_boolean ("ipv4_auto_conf",
                                                          _("ipv4_auto_conf"),
                                                          _("ipv4_auto_conf"),
                                                          FALSE,
                                                          G_PARAM_READWRITE));


    g_object_class_install_property (gobject_class,
                                     PROP_IPV4_ADDRESS,
                                     g_param_spec_string ("ipv4_address",
                                                          _("ipv4_address"),
                                                          _("ipv4_address"),
                                                          "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_IPV4_SUBNET,
                                     g_param_spec_string ("ipv4_subnet",
                                                          _("ipv4_subnet"),
                                                          _("ipv4_subnet"),
                                                          "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_IPV4_GATEWAY,
                                     g_param_spec_string ("ipv4_gateway",
                                                          _("ipv4_gateway"),
                                                          _("ipv4_gateway"),
                                                          "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_IPV6_ACTIVE,
                                     g_param_spec_boolean ("ipv6_active",
                                                           _("ipv6_active"),
                                                           _("ipv6_active"),
                                                          FALSE,
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_IPV6_AUTO_CONF,
                                     g_param_spec_boolean("ipv6_auto_conf",
                                                          _("ipv6_auto_conf"),
                                                          _("ipv6_auto_conf"),
                                                          FALSE,
                                                          G_PARAM_READWRITE));


    g_object_class_install_property (gobject_class,
                                     PROP_IPV6_ADDRESS,
                                     g_param_spec_string ("ipv6_address",
                                                          _("ipv6_address"),
                                                          _("ipv6_address"),
                                                          "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_IPV6_PREFIX,
                                     g_param_spec_string ("ipv6_prefix",
                                                          _("ipv6_prefix"),
                                                          _("ipv6_prefix"),
                                                          "",
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_V4ADDRESSES,
                                     g_param_spec_object ("v4addresses",
                                                          _("v4addresses"),
                                                          _("v4addresses"),
                                                          GTK_TYPE_LIST_STORE,
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_V6ADDRESSES,
                                     g_param_spec_object ("v6addresses",
                                                          _("v6addresses"),
                                                          _("v6addresses"),
                                                          GTK_TYPE_LIST_STORE,
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
                                     PROP_WIFI_INFO,
                                     g_param_spec_object ("wifi_info",
                                                          _("Wi-Fi configuration information."),
                                                          _("Wi-Fi configuration information."),
                                                          NWAMUI_TYPE_WIFI_NET,
                                                          G_PARAM_READWRITE));
    
    g_object_class_install_property (gobject_class,
                                     PROP_CONDITIONS,
                                     g_param_spec_pointer ("conditions",
                                                          _("conditions"),
                                                          _("conditions"),
                                                          G_PARAM_READWRITE));
}


static void
nwamui_ncu_init (NwamuiNcu *self)
{
    self->prv = g_new0 (NwamuiNcuPrivate, 1);
    
    self->prv->vanity_name = NULL;
    self->prv->device_name = NULL;
    self->prv->ncu_type = NWAMUI_NCU_TYPE_WIRED;
    self->prv->active = FALSE;
    self->prv->speed = 0;
    self->prv->ipv4_primary_ip = NULL;
    self->prv->ipv4_gateway = NULL;
    self->prv->ipv6_active = FALSE;
    self->prv->ipv6_primary_ip = NULL;
    self->prv->v4addresses = gtk_list_store_new ( 1, NWAMUI_TYPE_IP);
    self->prv->v6addresses = gtk_list_store_new ( 1, NWAMUI_TYPE_IP);
    self->prv->wifi_info = NULL;
    self->prv->activiation_mode = NWAMUI_COND_ACTIVATION_MODE_MANUAL;
    self->prv->enabled = FALSE;
    self->prv->priority_group = 0;
    self->prv->priority_group_mode = NWAMUI_COND_PRIORITY_GROUP_MODE_EXCLUSIVE;
    self->prv->conditions = NULL;
    
    g_signal_connect(G_OBJECT(self), "notify", (GCallback)object_notify_cb, (gpointer)self);
    g_signal_connect(G_OBJECT(self->prv->v4addresses), "row-deleted", (GCallback)ip_row_deleted_cb, (gpointer)self);
    g_signal_connect(G_OBJECT(self->prv->v4addresses), "row-changed", (GCallback)ip_row_inserted_or_changed_cb, (gpointer)self);
    g_signal_connect(G_OBJECT(self->prv->v4addresses), "row-inserted", (GCallback)ip_row_inserted_or_changed_cb, (gpointer)self);
}

static void
nwamui_ncu_set_property ( GObject         *object,
                                    guint            prop_id,
                                    const GValue    *value,
                                    GParamSpec      *pspec)
{
    NwamuiNcu *self = NWAMUI_NCU(object);
    gchar*      tmpstr = NULL;
    gint        tmpint = 0;

    switch (prop_id) {
        case PROP_NWAM_NCU: {
                g_assert (self->prv->nwam_ncu == NULL);
                self->prv->nwam_ncu = g_value_get_pointer (value);
            }
            break;

    case PROP_NCP: {
            NwamuiNcp* ncp = NWAMUI_NCP( g_value_dup_object( value ) );

            if ( self->prv->ncp != NULL ) {
                    g_object_unref( self->prv->ncp );
            }
            self->prv->ncp = ncp;
        }
        break;

        case PROP_VANITY_NAME: {
                if ( self->prv->vanity_name != NULL ) {
                        g_free( self->prv->vanity_name );
                }
                self->prv->vanity_name = g_strdup( g_value_get_string( value ) );
            }
            break;
        case PROP_DEVICE_NAME: {
                if ( self->prv->device_name != NULL ) {
                        g_free( self->prv->device_name );
                }
                self->prv->device_name = g_strdup( g_value_get_string( value ) );
            }
            break;
        case PROP_PHY_ADDRESS: {
                if ( self->prv->phy_address != NULL ) {
                        g_free( self->prv->phy_address );
                }
                self->prv->phy_address = g_strdup( g_value_get_string( value ) );
            }
            break;
        case PROP_NCU_TYPE: {
                self->prv->ncu_type = g_value_get_int( value );
            }
            break;
        case PROP_ACTIVE: {
                self->prv->active = g_value_get_boolean( value );
            }
            break;
        case PROP_SPEED: {
                self->prv->speed = g_value_get_uint( value );
            }
            break;
        case PROP_IPV4_AUTO_CONF: {
                gboolean auto_conf = g_value_get_boolean( value );

                if ( self->prv->ipv4_primary_ip != NULL ) {
                    nwamui_ip_set_dhcp(self->prv->ipv4_primary_ip, auto_conf );
                }
                else {
                    NwamuiIp*   ip = nwamui_ip_new( self, "", "", FALSE,
                                                    auto_conf, FALSE, FALSE );
                    GtkTreeIter iter;

                    gtk_list_store_insert(self->prv->v4addresses, &iter, 0 );
                    gtk_list_store_set(self->prv->v4addresses, &iter, 0, ip, -1 );
                    self->prv->ipv4_primary_ip = ip;
                }
            }
            break;
        case PROP_IPV4_ADDRESS: {
                const gchar* new_addr  = g_value_get_string( value );
                
                if ( new_addr != NULL ) {
                    if ( self->prv->ipv4_primary_ip != NULL ) {
                        nwamui_ip_set_address(self->prv->ipv4_primary_ip, new_addr );
                    }
                    else {
                        NwamuiIp*   ip = nwamui_ip_new( self, new_addr, "", FALSE,
                                                        FALSE, FALSE, TRUE );
                        GtkTreeIter iter;

                        gtk_list_store_insert(self->prv->v4addresses, &iter, 0 );
                        gtk_list_store_set(self->prv->v4addresses, &iter, 0, ip, -1 );
                        self->prv->ipv4_primary_ip = ip;
                    }
                }
            }
            break;
        case PROP_IPV4_SUBNET: {
                const gchar* new_subnet  = g_value_get_string( value );
                
                if ( new_subnet != NULL ) {
                    if ( self->prv->ipv4_primary_ip != NULL ) {
                        nwamui_ip_set_subnet_prefix(self->prv->ipv4_primary_ip, new_subnet );
                    }
                    else {
                        NwamuiIp*   ip = nwamui_ip_new( self, "", new_subnet, FALSE,
                                                        FALSE, FALSE, TRUE );
                        GtkTreeIter iter;

                        gtk_list_store_insert(self->prv->v4addresses, &iter, 0 );
                        gtk_list_store_set(self->prv->v4addresses, &iter, 0, ip, -1 );
                        self->prv->ipv4_primary_ip = ip;
                    }
                }
            }
            break;
        case PROP_IPV4_GATEWAY: {
                if ( self->prv->ipv4_gateway != NULL ) {
                        g_free( self->prv->ipv4_gateway );
                }
                self->prv->ipv4_gateway = g_strdup( g_value_get_string( value ) );
            }
            break;
        case PROP_IPV6_ACTIVE: {
                self->prv->ipv6_active = g_value_get_boolean( value );
            }
            break;
        case PROP_IPV6_AUTO_CONF: {
                gboolean auto_conf = g_value_get_boolean( value );

                if ( self->prv->ipv6_primary_ip != NULL ) {
                    nwamui_ip_set_dhcp(self->prv->ipv6_primary_ip, auto_conf );
                }
                else {
                    NwamuiIp*   ip = nwamui_ip_new( self, "", "", TRUE,
                                                    auto_conf, FALSE, FALSE );
                    GtkTreeIter iter;

                    gtk_list_store_insert(self->prv->v6addresses, &iter, 0 );
                    gtk_list_store_set(self->prv->v6addresses, &iter, 0, ip, -1 );
                    self->prv->ipv6_primary_ip = ip;
                }
            }
            break;
        case PROP_IPV6_ADDRESS: {
                const gchar* new_addr  = g_value_get_string( value );
                
                if ( new_addr != NULL ) {
                    if ( self->prv->ipv6_primary_ip != NULL ) {
                        nwamui_ip_set_address(self->prv->ipv6_primary_ip, new_addr );
                    }
                    else {
                        NwamuiIp*   ip = nwamui_ip_new( self, new_addr, "", TRUE,
                                                        FALSE, FALSE, TRUE );
                        GtkTreeIter iter;

                        gtk_list_store_insert(self->prv->v6addresses, &iter, 0 );
                        gtk_list_store_set(self->prv->v6addresses, &iter, 0, ip, -1 );
                        self->prv->ipv6_primary_ip = ip;
                    }
                }
            }
            break;
        case PROP_IPV6_PREFIX: {
                const gchar* new_subnet  = g_value_get_string( value );
                
                if ( new_subnet != NULL ) {
                    if ( self->prv->ipv6_primary_ip != NULL ) {
                        nwamui_ip_set_subnet_prefix(self->prv->ipv6_primary_ip, new_subnet );
                    }
                    else {
                        NwamuiIp*   ip = nwamui_ip_new( self, "", new_subnet, TRUE,
                                                        FALSE, FALSE, TRUE );
                        GtkTreeIter iter;

                        gtk_list_store_insert(self->prv->v6addresses, &iter, 0 );
                        gtk_list_store_set(self->prv->v6addresses, &iter, 0, ip, -1 );
                        self->prv->ipv6_primary_ip = ip;
                    }
                }
            }
            break;
        case PROP_V4ADDRESSES: {
                self->prv->v4addresses = GTK_LIST_STORE(g_value_dup_object( value ));
            }
            break;

        case PROP_V6ADDRESSES: {
                self->prv->v6addresses = GTK_LIST_STORE(g_value_dup_object( value ));
            }
            break;
        case PROP_WIFI_INFO: {
                /* Should be a Wireless i/f */
                g_assert( self->prv->ncu_type == NWAMUI_NCU_TYPE_WIRELESS );
                if ( self->prv->wifi_info != NULL ) {
                        g_object_unref( self->prv->wifi_info );
                }
                self->prv->wifi_info = NWAMUI_WIFI_NET( g_value_dup_object( value ) );
            }
            break;

        case PROP_ACTIVIATION_MODE: {
                self->prv->activiation_mode = (nwamui_cond_activation_mode_t)g_value_get_int( value );
            }
            break;

        case PROP_ENABLED: {
                self->prv->enabled = g_value_get_boolean( value );
            }
            break;

        case PROP_PRIORITY_GROUP: {
                self->prv->priority_group = g_value_get_int( value );
            }
            break;

        case PROP_PRIORITY_GROUP_MODE: {
                self->prv->priority_group_mode = (nwamui_cond_priority_group_mode_t)g_value_get_int( value );
            }
            break;

        case PROP_CONDITIONS: {
                if ( self->prv->conditions != NULL ) {
                        g_free( self->prv->conditions );
                }
                self->prv->conditions = g_value_get_pointer( value );
            }
            break;

        default: 
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
nwamui_ncu_get_property (GObject         *object,
                                  guint            prop_id,
                                  GValue          *value,
                                  GParamSpec      *pspec)
{
    NwamuiNcu *self = NWAMUI_NCU(object);

    switch (prop_id) {
        case PROP_NCP: {
                g_value_set_object( value, self->prv->ncp );
            }
            break;
        case PROP_VANITY_NAME: {
                g_value_set_string(value, self->prv->vanity_name);
            }
            break;
        case PROP_DEVICE_NAME: {
                g_value_set_string(value, self->prv->device_name);
            }
            break;
        case PROP_PHY_ADDRESS: {
                g_value_set_string(value, self->prv->phy_address);
            }
            break;
        case PROP_NCU_TYPE: {
                g_value_set_int(value, self->prv->ncu_type);
            }
            break;
        case PROP_ACTIVE: {
                g_value_set_boolean(value, self->prv->active);
            }
            break;
        case PROP_SPEED: {
                g_value_set_uint( value, self->prv->speed );
            }
            break;
        case PROP_IPV4_AUTO_CONF: {
                gboolean auto_conf = FALSE;
                
                if ( self->prv->ipv4_primary_ip != NULL ) {
                    auto_conf = nwamui_ip_is_dhcp(self->prv->ipv4_primary_ip); 
                }
                g_value_set_boolean(value, auto_conf);
            }
            break;
        case PROP_IPV4_ADDRESS: {
                gchar *address = NULL;
                if ( self->prv->ipv4_primary_ip != NULL ) {
                    address = nwamui_ip_get_address(self->prv->ipv4_primary_ip);
                }
                g_value_set_string(value, address);
                if ( address != NULL ) {
                    g_free(address);
                }
            }
            break;
        case PROP_IPV4_SUBNET: {
                gchar *subnet = NULL;
                if ( self->prv->ipv4_primary_ip != NULL ) {
                    subnet = nwamui_ip_get_subnet_prefix(self->prv->ipv4_primary_ip);
                }
                g_value_set_string(value, subnet);
                if ( subnet != NULL ) {
                    g_free(subnet);
                }
            }
            break;
        case PROP_IPV4_GATEWAY: {
                g_value_set_string(value, self->prv->ipv4_gateway);
            }
            break;
        case PROP_IPV6_ACTIVE: {
                g_value_set_boolean(value, self->prv->ipv6_active);
            }
            break;
        case PROP_IPV6_AUTO_CONF: {
                gboolean auto_conf = FALSE;
                
                if ( self->prv->ipv6_primary_ip != NULL ) {
                    auto_conf = nwamui_ip_is_dhcp(self->prv->ipv6_primary_ip); 
                }
                g_value_set_boolean(value, auto_conf);
            }
            break;
        case PROP_IPV6_ADDRESS: {
                gchar *address = NULL;
                if ( self->prv->ipv6_primary_ip != NULL ) {
                    address = nwamui_ip_get_address(self->prv->ipv6_primary_ip);
                }
                g_value_set_string(value, address);
                if ( address != NULL ) {
                    g_free(address);
                }
            }
            break;
        case PROP_IPV6_PREFIX: {
                gchar *prefix = NULL;
                if ( self->prv->ipv6_primary_ip != NULL ) {
                    prefix = nwamui_ip_get_subnet_prefix(self->prv->ipv6_primary_ip);
                }
                g_value_set_string(value, prefix);
                if ( prefix != NULL ) {
                    g_free(prefix);
                }
            }
            break;
        case PROP_V4ADDRESSES: {
                g_value_set_object( value, (gpointer)self->prv->v4addresses);
            }
            break;

        case PROP_V6ADDRESSES: {
                g_value_set_object( value, (gpointer)self->prv->v6addresses);
            }
            break;
        case PROP_WIFI_INFO: {
                /* Should be a Wireless i/f */
                g_assert( self->prv->ncu_type == NWAMUI_NCU_TYPE_WIRELESS );

                g_value_set_object( value, (gpointer)self->prv->wifi_info );
            }
            break;
        case PROP_ACTIVIATION_MODE: {
                g_value_set_int( value, (gint)self->prv->activiation_mode );
            }
            break;

        case PROP_ENABLED: {
                g_value_set_boolean( value, self->prv->enabled );
            }
            break;

        case PROP_PRIORITY_GROUP: {
                g_value_set_int( value, self->prv->priority_group );
            }
            break;

        case PROP_PRIORITY_GROUP_MODE: {
                g_value_set_int( value, (gint)self->prv->priority_group_mode );
            }
            break;
        case PROP_CONDITIONS: {
                g_value_set_pointer( value, self->prv->conditions );
            }
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
populate_link_ncu_data( NwamuiNcu *ncu )
{
    gchar*      mac_addr = get_nwam_ncu_string_prop( ncu->prv->nwam_ncu, NWAM_NCU_PROP_LINK_MAC_ADDR );
    gchar**     autopush = get_nwam_ncu_string_array_prop( ncu->prv->nwam_ncu, NWAM_NCU_PROP_LINK_AUTOPUSH );
    guint64     mtu = get_nwam_ncu_uint64_prop( ncu->prv->nwam_ncu, NWAM_NCU_PROP_LINK_MTU );

    g_object_set( ncu,
                  "phy_address", mac_addr,
                  "autopush", autopush,
                  "mtu", mtu,
                  NULL);

    g_free( mac_addr );
    g_strfreev( autopush );
}

static void
populate_iptun_ncu_data( NwamuiNcu *ncu )
{
    nwam_iptun_type_t tun_type; 
    gchar* tun_tsrc;
    gchar* tun_tdst;
    gchar* tun_encr;
    gchar* tun_encr_auth;
    gchar* tun_auth;

    /* CLASS IPTUN is of type LINK, so has LINK props too */
    populate_link_ncu_data( ncu );

    tun_type = get_nwam_ncu_uint64_prop( ncu->prv->nwam_ncu, NWAM_NCU_PROP_IPTUN_TYPE );
    tun_tsrc = get_nwam_ncu_string_prop( ncu->prv->nwam_ncu, NWAM_NCU_PROP_IPTUN_TSRC );
    tun_tdst = get_nwam_ncu_string_prop( ncu->prv->nwam_ncu, NWAM_NCU_PROP_IPTUN_TDST );
    tun_encr = get_nwam_ncu_string_prop( ncu->prv->nwam_ncu, NWAM_NCU_PROP_IPTUN_ENCR );
    tun_encr_auth = get_nwam_ncu_string_prop( ncu->prv->nwam_ncu, NWAM_NCU_PROP_IPTUN_ENCR_AUTH );
    tun_auth = get_nwam_ncu_string_prop( ncu->prv->nwam_ncu, NWAM_NCU_PROP_IPTUN_AUTH );

    g_object_set( ncu, 
                  "tun_type", tun_type,
                  "tun_tsrc", tun_tsrc,
                  "tun_tdst", tun_tdst,
                  "tun_encr", tun_encr,
                  "tun_encr_auth", tun_encr_auth,
                  "tun_auth", tun_auth,
                  NULL );

    g_free(tun_tsrc);
    g_free(tun_tdst);
    g_free(tun_encr);
    g_free(tun_encr_auth);
    g_free(tun_auth);
}

static void
populate_ip_ncu_data( NwamuiNcu *ncu )
{
    nwam_ip_version_t   ip_version;
    guint               ipv4_addrsrc_num = 0;
    nwam_addrsrc_t*     ipv4_addrsrc = NULL;
    gchar**             ipv4_addr = NULL;
    guint               ipv6_addrsrc_num = 0;
    nwam_addrsrc_t*     ipv6_addrsrc =  NULL;
    gchar**             ipv6_addr = NULL;
    
    ip_version = (nwam_ip_version_t)get_nwam_ncu_uint64_prop(ncu->prv->nwam_ncu, NWAM_NCU_PROP_IP_VERSION );

    if ( (ip_version == NWAM_IP_VERSION_IPV4) || (ip_version == NWAM_IP_VERSION_ALL) )  {
        char**  ptr;
        ipv4_addrsrc = (nwam_addrsrc_t*)get_nwam_ncu_uint64_array_prop( ncu->prv->nwam_ncu, 
                                                                        NWAM_NCU_PROP_IPV4_ADDRSRC, 
                                                                        &ipv4_addrsrc_num );

        ipv4_addr = get_nwam_ncu_string_array_prop(ncu->prv->nwam_ncu, NWAM_NCU_PROP_IPV4_ADDR );

        /* Populate the v4addresses member */
        g_debug( "ipv4_addrsrc_num = %u", ipv4_addrsrc_num );
        ptr = ipv4_addr;

        for( int i = 0; i < ipv4_addrsrc_num; i++ ) {
            NwamuiIp*   ip = nwamui_ip_new( ncu, 
                                            ((ptr != NULL)?(*ptr):(NULL)), 
                                            "", 
                                            FALSE, 
                                            ipv4_addrsrc[i] == NWAM_ADDRSRC_DHCP,
                                            ipv4_addrsrc[i] == NWAM_ADDRSRC_AUTOCONF,
                                            ipv4_addrsrc[i] == NWAM_ADDRSRC_STATIC);

            GtkTreeIter iter;

            gtk_list_store_insert(ncu->prv->v4addresses, &iter, 0 );
            gtk_list_store_set(ncu->prv->v4addresses, &iter, 0, ip, -1 );

            if ( i == 0 ) {
                ncu->prv->ipv4_primary_ip = NWAMUI_IP(g_object_ref(ip));
            }
        }
    }

    if ( (ip_version == NWAM_IP_VERSION_IPV6) || (ip_version == NWAM_IP_VERSION_ALL) )  {
        char**  ptr;

        ipv6_addrsrc = (nwam_addrsrc_t*)get_nwam_ncu_uint64_array_prop( ncu->prv->nwam_ncu, 
                                                                        NWAM_NCU_PROP_IPV6_ADDRSRC, 
                                                                        &ipv6_addrsrc_num );
        
        ipv6_addr = get_nwam_ncu_string_array_prop(ncu->prv->nwam_ncu, NWAM_NCU_PROP_IPV6_ADDR );

        /* Populate the v6addresses member */
        g_debug( "ipv6_addrsrc_num = %u", ipv6_addrsrc_num );
        ptr = ipv6_addr;

        for( int i = 0; i < ipv6_addrsrc_num; i++ ) {
            NwamuiIp*   ip = nwamui_ip_new( ncu, 
                                            ((ptr != NULL)?(*ptr):(NULL)), 
                                            "", 
                                            TRUE, 
                                            ipv6_addrsrc[i] == NWAM_ADDRSRC_DHCPV6,
                                            ipv6_addrsrc[i] == NWAM_ADDRSRC_AUTOCONF,
                                            ipv6_addrsrc[i] == NWAM_ADDRSRC_STATIC);

            GtkTreeIter iter;

            gtk_list_store_insert(ncu->prv->v6addresses, &iter, 0 );
            gtk_list_store_set(ncu->prv->v6addresses, &iter, 0, ip, -1 );

            if ( i == 0 ) {
                ncu->prv->ipv6_primary_ip = NWAMUI_IP(g_object_ref(ip));
            }
        }
    }

}

/* 
 * An NCU name is <type>:<devicename>, extract the devicename part 
 */
static gchar*
get_device_from_ncu_name( const gchar* ncu_name )
{
    gchar*  ptr = NULL;

    if ( ncu_name != NULL ) {
        ptr = strrchr( ncu_name, ':');
        if ( ptr != NULL ) {
            ptr = g_strdup(ptr);
        }
    }

    return( ptr );
}

extern  NwamuiNcu*
nwamui_ncu_new_with_handle( NwamuiNcp* ncp, nwam_ncu_handle_t ncu )
{
    NwamuiNcu*          self = NULL;
    char*               name = NULL;
    char*               over_name = NULL;
    nwam_ncu_class_t    ncu_class;
    nwam_error_t        nerr;
    
    self = NWAMUI_NCU(g_object_new (NWAMUI_TYPE_NCU,
                                    "nwam_ncu", ncu,
                                    "ncp", ncp,
                                    NULL));

    if ( (nerr = nwam_ncu_get_name (ncu, &name)) != NWAM_SUCCESS ) {
        g_debug ("Failed to get name for ncu, error: %s", nwam_strerror (nerr));
    }

    self->prv->vanity_name = g_strdup( name );
    self->prv->device_name = g_strdup( name );

    ncu_class = (nwam_ncu_class_t)get_nwam_ncu_uint64_prop(ncu, NWAM_NCU_PROP_CLASS);

    switch ( ncu_class ) {
        case NWAM_NCU_CLASS_PHYS: {
                /* CLASS PHYS is of type LINK, so has LINK props */
                populate_link_ncu_data( self );
            }
            break;
        case NWAM_NCU_CLASS_IPTUN: {
                populate_iptun_ncu_data( self );
            }
            break;
        case NWAM_NCU_CLASS_IP: {
                populate_ip_ncu_data( self );
            }
            break;
        default:
            g_error("Unexpected ncu class %u", (guint)ncu_class);
    }
    
    return( self );
}

/**
 * nwamui_ncu_new:
 * @returns: a new #NwamuiNcu.
 *
 * Creates a new #NwamuiNcu with info initialised based on the argumens passed.
 **/
extern  NwamuiNcu*          
nwamui_ncu_new (    const gchar*        vanity_name,
                    const gchar*        device_name,
                    const gchar*        phy_address,
                    nwamui_ncu_type_t   ncu_type,
                    gboolean            active,
                    guint               speed,
                    gboolean            ipv4_auto_conf,
                    const gchar*        ipv4_address,
                    const gchar*        ipv4_subnet,
                    const gchar*        ipv4_gateway,
                    gboolean            ipv6_active,
                    gboolean            ipv6_auto_conf,
                    const gchar*        ipv6_address,
                    const gchar*        ipv6_prefix,
                    NwamuiWifiNet*      wifi_info )
{
    NwamuiNcu   *self = NWAMUI_NCU(g_object_new (NWAMUI_TYPE_NCU, NULL));

    g_object_set (  G_OBJECT (self),
                    "vanity_name", vanity_name,
                    "device_name", device_name,
                    "phy_address", phy_address,
                    "ncu_type", ncu_type,
                    "active", active,
                    "speed", speed,
                    "ipv4_auto_conf", ipv4_auto_conf,
                    "ipv4_address", ipv4_address,
                    "ipv4_subnet", ipv4_subnet,
                    "ipv4_gateway", ipv4_gateway,
                    "ipv6_active", ipv6_active,
                    "ipv6_auto_conf", ipv6_auto_conf,
                    "ipv6_address", ipv6_address,
                    "ipv6_prefix", ipv6_prefix,
                    NULL);

    if ( ncu_type == NWAMUI_NCU_TYPE_WIRELESS ) {
        g_object_set (  G_OBJECT (self),
                        "wifi_info", wifi_info,
                        NULL);
    }
    
    return( self );
}

/**
 * nwamui_ncu_get_vanity_name:
 * @returns: null-terminated C String with the vanity name of the the NCU.
 *
 **/
extern gchar*
nwamui_ncu_get_vanity_name ( NwamuiNcu *self )
{
    gchar*  name = NULL;
    
    g_return_val_if_fail (NWAMUI_IS_NCU(self), name); 
    
    g_object_get (G_OBJECT (self),
                  "vanity_name", &name,
                  NULL);

    return( name );
}

/**
 * nwamui_ncu_set_vanity_name:
 * @name: null-terminated C String with the vanity name of the the NCU.
 *
 **/
extern void
nwamui_ncu_set_vanity_name ( NwamuiNcu *self, const gchar* name )
{
    g_return_if_fail (NWAMUI_IS_NCU(self)); 
    
    g_assert (name != NULL );

    if ( name != NULL ) {
        g_object_set (G_OBJECT (self),
                      "vanity_name", name,
                      NULL);
    }
}

/**
 * nwamui_ncu_get_phy_address:
 * @returns: null-terminated C String with the device name of the the NCU.
 *
 **/
extern gchar*
nwamui_ncu_get_phy_address ( NwamuiNcu *self )
{
    gchar*  name = NULL;
    
    g_return_val_if_fail (NWAMUI_IS_NCU(self), name); 
    
    g_object_get (G_OBJECT (self),
                  "phy_address", &name,
                  NULL);

    return( name );
}

/**
 * nwamui_ncu_set_phy_address:
 * @phy_address: null-terminated C String with the device phy_address of the the NCU.
 *
 **/
extern void
nwamui_ncu_set_phy_address ( NwamuiNcu *self, const gchar* phy_address )
{
    g_return_if_fail (NWAMUI_IS_NCU(self)); 
    
    g_assert (phy_address != NULL );

    if ( phy_address != NULL ) {
        g_object_set (G_OBJECT (self),
                      "phy_address", phy_address,
                      NULL);
    }
}

/**
 * nwamui_ncu_get_device_name:
 * @returns: null-terminated C String with the device name of the the NCU.
 *
 **/
extern gchar*
nwamui_ncu_get_device_name ( NwamuiNcu *self )
{
    gchar*  name = NULL;
    
    g_return_val_if_fail (NWAMUI_IS_NCU(self), name); 
    
    g_object_get (G_OBJECT (self),
                  "device_name", &name,
                  NULL);

    return( name );
}

/**
 * nwamui_ncu_set_device_name:
 * @name: null-terminated C String with the device name of the the NCU.
 *
 **/
extern void
nwamui_ncu_set_device_name ( NwamuiNcu *self, const gchar* name )
{
    g_return_if_fail (NWAMUI_IS_NCU(self)); 
    
    g_assert (name != NULL );

    if ( name != NULL ) {
        g_object_set (G_OBJECT (self),
                      "device_name", name,
                      NULL);
    }
}

/**
 * nwamui_ncu_get_display_name:
 * @returns: null-terminated C String with the device name of the the NCU.
 *
 * Use this to get a string of the format "<Vanity Name> (<device name>)"
 **/
extern gchar*               
nwamui_ncu_get_display_name ( NwamuiNcu *self )
{
    gchar*  dname = NULL;
    gchar*  vname = NULL;
    gchar*  disp_name = NULL;
    
    g_return_val_if_fail (NWAMUI_IS_NCU(self), dname); 
    
    g_object_get (G_OBJECT (self),
                  "vanity_name", &vname,
                  "device_name", &dname,
                  NULL);

    if ( vname != NULL & dname != NULL ) {
        disp_name = g_strdup_printf(_("%s (%s)"), vname, dname );
    }
    g_free( vname );
    g_free( dname );
    
    return( disp_name );
}

/** 
 * nwamui_ncu_set_ncu_type:
 * @nwamui_ncu: a #NwamuiNcu.
 * @ncu_type: Sets the NCU's type.
 * 
 **/ 
extern void                 
nwamui_ncu_set_ncu_type ( NwamuiNcu *self, nwamui_ncu_type_t ncu_type )
{
    g_return_if_fail (NWAMUI_IS_NCU (self));
    
    g_assert( ncu_type >= NWAMUI_NCU_TYPE_WIRED && ncu_type < NWAMUI_NCU_TYPE_LAST );

    g_object_set (G_OBJECT (self),
                  "ncu_type", ncu_type,
                  NULL); 
}

/**
 * nwamui_ncu_get_ncu_type
 * @returns: the type of the NCU.
 *
 **/
extern nwamui_ncu_type_t    
nwamui_ncu_get_ncu_type ( NwamuiNcu *self )
{
    nwamui_ncu_type_t    ncu_type = FALSE; 
    
    g_return_val_if_fail (NWAMUI_IS_NCU(self), ncu_type); 
    
    g_object_get (G_OBJECT (self),
                  "ncu_type", &ncu_type,
                  NULL);

    return( ncu_type );
}

/**
 * nwamui_ncu_get_active:
 * @returns: active state.
 *
 **/
extern gboolean
nwamui_ncu_get_active ( NwamuiNcu *self )
{
    gboolean    active = FALSE; 
    
    g_return_val_if_fail (NWAMUI_IS_NCU(self), active); 
    
    g_object_get (G_OBJECT (self),
                  "active", &active,
                  NULL);

    return( active );
}

/** 
 * nwamui_ncu_set_active:
 * @nwamui_ncu: a #NwamuiNcu.
 * @active: Valiue to set active to.
 * 
 **/ 
extern void
nwamui_ncu_set_active (   NwamuiNcu *self,
                              gboolean        active )
{
    g_return_if_fail (NWAMUI_IS_NCU (self));

    g_object_set (G_OBJECT (self),
                  "active", active,
                  NULL);
}

/** 
 * nwamui_ncu_set_speed:
 * @nwamui_ncu: a #NwamuiNcu.
 * @speed: Value to set speed to.
 * 
 **/ 
extern void
nwamui_ncu_set_speed (   NwamuiNcu *self,
                              guint        speed )
{
    g_return_if_fail (NWAMUI_IS_NCU (self));
    g_assert (speed >= 0 && speed <= G_MAXUINT );

    g_object_set (G_OBJECT (self),
                  "speed", speed,
                  NULL);
}

/**
 * nwamui_ncu_get_speed:
 * @nwamui_ncu: a #NwamuiNcu.
 * @returns: the speed.
 *
 **/
extern guint
nwamui_ncu_get_speed (NwamuiNcu *self)
{
    guint  speed = 0; 

    g_return_val_if_fail (NWAMUI_IS_NCU (self), speed);

    g_object_get (G_OBJECT (self),
                  "speed", &speed,
                  NULL);

    return( speed );
}

/** 
 * nwamui_ncu_set_ipv4_auto_conf:
 * @nwamui_ncu: a #NwamuiNcu.
 * @ipv4_auto_conf: Valiue to set ipv4_auto_conf to.
 * 
 **/ 
extern void
nwamui_ncu_set_ipv4_auto_conf (   NwamuiNcu *self,
                              gboolean        ipv4_auto_conf )
{
    g_return_if_fail (NWAMUI_IS_NCU (self));

    g_object_set (G_OBJECT (self),
                  "ipv4_auto_conf", ipv4_auto_conf,
                  NULL);
}

/**
 * nwamui_ncu_get_ipv4_auto_conf:
 * @nwamui_ncu: a #NwamuiNcu.
 * @returns: the ipv4_auto_conf.
 *
 **/
extern gboolean
nwamui_ncu_get_ipv4_auto_conf (NwamuiNcu *self)
{
    gboolean  ipv4_auto_conf = TRUE; 

    g_return_val_if_fail (NWAMUI_IS_NCU (self), ipv4_auto_conf);

    g_object_get (G_OBJECT (self),
                  "ipv4_auto_conf", &ipv4_auto_conf,
                  NULL);

    return( ipv4_auto_conf );
}

/** 
 * nwamui_ncu_set_ipv4_address:
 * @nwamui_ncu: a #NwamuiNcu.
 * @ipv4_address: Valiue to set ipv4_address to.
 * 
 **/ 
extern void
nwamui_ncu_set_ipv4_address (   NwamuiNcu *self,
                              const gchar*  ipv4_address )
{
    g_return_if_fail (NWAMUI_IS_NCU (self));
    g_assert (ipv4_address != NULL );

    if ( ipv4_address != NULL ) {
        g_object_set (G_OBJECT (self),
                      "ipv4_address", ipv4_address,
                      NULL);
    }
}

/**
 * nwamui_ncu_get_ipv4_address:
 * @nwamui_ncu: a #NwamuiNcu.
 * @returns: the ipv4_address.
 *
 **/
extern gchar*
nwamui_ncu_get_ipv4_address (NwamuiNcu *self)
{
    gchar*  ipv4_address = NULL; 

    g_return_val_if_fail (NWAMUI_IS_NCU (self), ipv4_address);

    g_object_get (G_OBJECT (self),
                  "ipv4_address", &ipv4_address,
                  NULL);

    return( ipv4_address );
}

/** 
 * nwamui_ncu_set_ipv4_subnet:
 * @nwamui_ncu: a #NwamuiNcu.
 * @ipv4_subnet: Valiue to set ipv4_subnet to.
 * 
 **/ 
extern void
nwamui_ncu_set_ipv4_subnet (   NwamuiNcu *self,
                              const gchar*  ipv4_subnet )
{
    g_return_if_fail (NWAMUI_IS_NCU (self));
    g_assert (ipv4_subnet != NULL );

    if ( ipv4_subnet != NULL ) {
        g_object_set (G_OBJECT (self),
                      "ipv4_subnet", ipv4_subnet,
                      NULL);
    }
}

/**
 * nwamui_ncu_get_ipv4_subnet:
 * @nwamui_ncu: a #NwamuiNcu.
 * @returns: the ipv4_subnet.
 *
 **/
extern gchar*
nwamui_ncu_get_ipv4_subnet (NwamuiNcu *self)
{
    gchar*  ipv4_subnet = NULL; 

    g_return_val_if_fail (NWAMUI_IS_NCU (self), ipv4_subnet);

    g_object_get (G_OBJECT (self),
                  "ipv4_subnet", &ipv4_subnet,
                  NULL);

    return( ipv4_subnet );
}

/** 
 * nwamui_ncu_set_ipv4_gateway:
 * @nwamui_ncu: a #NwamuiNcu.
 * @ipv4_gateway: Valiue to set ipv4_gateway to.
 * 
 **/ 
extern void
nwamui_ncu_set_ipv4_gateway (   NwamuiNcu *self,
                              const gchar*  ipv4_gateway )
{
    g_return_if_fail (NWAMUI_IS_NCU (self));
    g_assert (ipv4_gateway != NULL );

    if ( ipv4_gateway != NULL ) {
        g_object_set (G_OBJECT (self),
                      "ipv4_gateway", ipv4_gateway,
                      NULL);
    }
}

/**
 * nwamui_ncu_get_ipv4_gateway:
 * @nwamui_ncu: a #NwamuiNcu.
 * @returns: the ipv4_gateway.
 *
 **/
extern gchar*
nwamui_ncu_get_ipv4_gateway (NwamuiNcu *self)
{
    gchar*  ipv4_gateway = NULL; 

    g_return_val_if_fail (NWAMUI_IS_NCU (self), ipv4_gateway);

    g_object_get (G_OBJECT (self),
                  "ipv4_gateway", &ipv4_gateway,
                  NULL);

    return( ipv4_gateway );
}

/** 
 * nwamui_ncu_set_ipv6_active:
 * @nwamui_ncu: a #NwamuiNcu.
 * @ipv6_active: Valiue to set ipv6_active to.
 * 
 **/ 
extern void
nwamui_ncu_set_ipv6_active (   NwamuiNcu *self,
                              gboolean        ipv6_active )
{
    g_return_if_fail (NWAMUI_IS_NCU (self));

    g_object_set (G_OBJECT (self),
                  "ipv6_active", ipv6_active,
                  NULL);
}

/**
 * nwamui_ncu_get_ipv6_active:
 * @nwamui_ncu: a #NwamuiNcu.
 * @returns: the ipv6_active.
 *
 **/
extern gboolean
nwamui_ncu_get_ipv6_active (NwamuiNcu *self)
{
    gboolean  ipv6_active = FALSE; 

    g_return_val_if_fail (NWAMUI_IS_NCU (self), ipv6_active);

    g_object_get (G_OBJECT (self),
                  "ipv6_active", &ipv6_active,
                  NULL);

    return( ipv6_active );
}

/** 
 * nwamui_ncu_set_ipv6_auto_conf:
 * @nwamui_ncu: a #NwamuiNcu.
 * @ipv6_auto_conf: Valiue to set ipv6_auto_conf to.
 * 
 **/ 
extern void
nwamui_ncu_set_ipv6_auto_conf (   NwamuiNcu *self,
                              gboolean        ipv6_auto_conf )
{
    g_return_if_fail (NWAMUI_IS_NCU (self));

    g_object_set (G_OBJECT (self),
                  "ipv6_auto_conf", ipv6_auto_conf,
                  NULL);
}

/**
 * nwamui_ncu_get_ipv6_auto_conf:
 * @nwamui_ncu: a #NwamuiNcu.
 * @returns: the ipv6_auto_conf.
 *
 **/
extern gboolean
nwamui_ncu_get_ipv6_auto_conf (NwamuiNcu *self)
{
    gboolean  ipv6_auto_conf = TRUE; 

    g_return_val_if_fail (NWAMUI_IS_NCU (self), ipv6_auto_conf);

    g_object_get (G_OBJECT (self),
                  "ipv6_auto_conf", &ipv6_auto_conf,
                  NULL);

    return( ipv6_auto_conf );
}

/** 
 * nwamui_ncu_set_ipv6_address:
 * @nwamui_ncu: a #NwamuiNcu.
 * @ipv6_address: Valiue to set ipv6_address to.
 * 
 **/ 
extern void
nwamui_ncu_set_ipv6_address (   NwamuiNcu *self,
                              const gchar*  ipv6_address )
{
    g_return_if_fail (NWAMUI_IS_NCU (self));
    g_assert (ipv6_address != NULL );

    if ( ipv6_address != NULL ) {
        g_object_set (G_OBJECT (self),
                      "ipv6_address", ipv6_address,
                      NULL);
    }
}

/**
 * nwamui_ncu_get_ipv6_address:
 * @nwamui_ncu: a #NwamuiNcu.
 * @returns: the ipv6_address.
 *
 **/
extern gchar*
nwamui_ncu_get_ipv6_address (NwamuiNcu *self)
{
    gchar*  ipv6_address = NULL; 

    g_return_val_if_fail (NWAMUI_IS_NCU (self), ipv6_address);

    g_object_get (G_OBJECT (self),
                  "ipv6_address", &ipv6_address,
                  NULL);

    return( ipv6_address );
}

/** 
 * nwamui_ncu_set_ipv6_prefix:
 * @nwamui_ncu: a #NwamuiNcu.
 * @ipv6_prefix: Valiue to set ipv6_prefix to.
 * 
 **/ 
extern void
nwamui_ncu_set_ipv6_prefix (   NwamuiNcu *self,
                              const gchar*  ipv6_prefix )
{
    g_return_if_fail (NWAMUI_IS_NCU (self));
    g_assert (ipv6_prefix != NULL );

    if ( ipv6_prefix != NULL ) {
        g_object_set (G_OBJECT (self),
                      "ipv6_prefix", ipv6_prefix,
                      NULL);
    }
}

/**
 * nwamui_ncu_get_ipv6_prefix:
 * @nwamui_ncu: a #NwamuiNcu.
 * @returns: the ipv6_prefix.
 *
 **/
extern gchar*
nwamui_ncu_get_ipv6_prefix (NwamuiNcu *self)
{
    gchar*  ipv6_prefix = NULL; 

    g_return_val_if_fail (NWAMUI_IS_NCU (self), ipv6_prefix);

    g_object_get (G_OBJECT (self),
                  "ipv6_prefix", &ipv6_prefix,
                  NULL);

    return( ipv6_prefix );
}

/** 
 * nwamui_ncu_set_v4addresses:
 * @nwamui_ncu: a #NwamuiNcu.
 * @v4addresses: Value to set v4addresses to.
 * 
 **/ 
extern void
nwamui_ncu_set_v4addresses (   NwamuiNcu *self,
                              GtkListStore*  v4addresses )
{
    g_return_if_fail (NWAMUI_IS_NCU (self));
    g_assert (v4addresses != NULL );

    if ( v4addresses != NULL ) {
        g_object_set (G_OBJECT (self),
                      "v4addresses", v4addresses,
                      NULL);
    }
}

/**
 * nwamui_ncu_get_v4addresses:
 * @nwamui_ncu: a #NwamuiNcu.
 * @returns: the v4addresses.
 *
 **/
extern GtkListStore*
nwamui_ncu_get_v4addresses (NwamuiNcu *self)
{
    GtkListStore*  v4addresses = NULL; 

    g_return_val_if_fail (NWAMUI_IS_NCU (self), v4addresses);

    g_object_get (G_OBJECT (self),
                  "v4addresses", &v4addresses,
                  NULL);

    return( v4addresses );
}

/** 
 * nwamui_ncu_set_v6addresses:
 * @nwamui_ncu: a #NwamuiNcu.
 * @v6addresses: Value to set v6addresses to.
 * 
 **/ 
extern void
nwamui_ncu_set_v6addresses (   NwamuiNcu *self,
                              GtkListStore*  v6addresses )
{
    g_return_if_fail (NWAMUI_IS_NCU (self));
    g_assert (v6addresses != NULL );

    if ( v6addresses != NULL ) {
        g_object_set (G_OBJECT (self),
                      "v6addresses", v6addresses,
                      NULL);
    }
}

/**
 * nwamui_ncu_get_v6addresses:
 * @nwamui_ncu: a #NwamuiNcu.
 * @returns: the v6addresses.
 *
 **/
extern GtkListStore*
nwamui_ncu_get_v6addresses (NwamuiNcu *self)
{
    GtkListStore*  v6addresses = NULL; 

    g_return_val_if_fail (NWAMUI_IS_NCU (self), v6addresses);

    g_object_get (G_OBJECT (self),
                  "v6addresses", &v6addresses,
                  NULL);

    return( v6addresses );
}

/**
 * nwamui_ncu_get_wifi_info:
 * @returns: #NwamuiWifiNet object
 *
 * For a wireless interface you can get the NwamuiWifiNet object that describes the
 * current wireless configuration.
 *
 * May return NULL if not configuration exists.
 *
 **/
extern NwamuiWifiNet*
nwamui_ncu_get_wifi_info ( NwamuiNcu *self )
{
    NwamuiWifiNet*  wifi_info = NULL;
    
    g_return_val_if_fail (NWAMUI_IS_NCU(self), wifi_info); 
    
    g_object_get (G_OBJECT (self),
                  "wifi_info", &wifi_info,
                  NULL);

    return( NWAMUI_WIFI_NET(wifi_info) );
}

/**
 * nwamui_ncu_set_wifi_info:
 * @wifi_info: #NwamuiWifiNet object
 *
 * For a wireless interface you can set the NwamuiWifiNet object that describes the
 * current wireless configuration.
 *
 **/
extern void
nwamui_ncu_set_wifi_info ( NwamuiNcu *self, NwamuiWifiNet* wifi_info )
{
    g_return_if_fail (NWAMUI_IS_NCU(self)); 

    g_assert( NWAMUI_IS_WIFI_NET( wifi_info ) );
    
    g_object_set (G_OBJECT (self),
                  "wifi_info", wifi_info,
                  NULL);
}

/**
 * nwamui_ncu_get_wifi_signal_strength:
 * @returns: signal strength
 *
 * For a wireless interface get the signal_strength
 *
 **/
extern nwamui_wifi_signal_strength_t
nwamui_ncu_get_wifi_signal_strength ( NwamuiNcu *self )
{
    nwamui_wifi_signal_strength_t   signal_strength = NWAMUI_WIFI_STRENGTH_NONE;
    
    g_return_val_if_fail (NWAMUI_IS_NCU(self), signal_strength); 
    
    if ( self->prv->wifi_info != NULL ) {
        signal_strength = nwamui_wifi_net_get_signal_strength(self->prv->wifi_info);
    }
    
    return( signal_strength );
}

/** 
 * nwamui_ncu_set_activiation_mode:
 * @nwamui_ncu: a #NwamuiNcu.
 * @activiation_mode: Value to set activiation_mode to.
 * 
 **/ 
extern void
nwamui_ncu_set_activiation_mode (   NwamuiNcu                      *self,
                                    nwamui_cond_activation_mode_t        activiation_mode )
{
    g_return_if_fail (NWAMUI_IS_NCU (self));
    g_assert (activiation_mode >= NWAMUI_COND_ACTIVATION_MODE_MANUAL && activiation_mode <= NWAMUI_COND_ACTIVATION_MODE_LAST );

    g_object_set (G_OBJECT (self),
                  "activiation_mode", (gint)activiation_mode,
                  NULL);
}

/**
 * nwamui_ncu_get_activiation_mode:
 * @nwamui_ncu: a #NwamuiNcu.
 * @returns: the activiation_mode.
 *
 **/
extern nwamui_cond_activation_mode_t
nwamui_ncu_get_activiation_mode (NwamuiNcu *self)
{
    gint  activiation_mode = NWAMUI_COND_ACTIVATION_MODE_MANUAL; 

    g_return_val_if_fail (NWAMUI_IS_NCU (self), activiation_mode);

    g_object_get (G_OBJECT (self),
                  "activiation_mode", &activiation_mode,
                  NULL);

    return( (nwamui_cond_activation_mode_t)activiation_mode );
}

/** 
 * nwamui_ncu_set_enabled:
 * @nwamui_ncu: a #NwamuiNcu.
 * @enabled: Value to set enabled to.
 * 
 **/ 
extern void
nwamui_ncu_set_enabled (   NwamuiNcu      *self,
                           gboolean        enabled )
{
    g_return_if_fail (NWAMUI_IS_NCU (self));

    g_object_set (G_OBJECT (self),
                  "enabled", enabled,
                  NULL);
}

/**
 * nwamui_ncu_get_enabled:
 * @nwamui_ncu: a #NwamuiNcu.
 * @returns: the enabled.
 *
 **/
extern gboolean
nwamui_ncu_get_enabled (NwamuiNcu *self)
{
    gboolean  enabled = FALSE; 

    g_return_val_if_fail (NWAMUI_IS_NCU (self), enabled);

    g_object_get (G_OBJECT (self),
                  "enabled", &enabled,
                  NULL);

    return( enabled );
}

/** 
 * nwamui_ncu_set_priority_group:
 * @nwamui_ncu: a #NwamuiNcu.
 * @priority_group: Value to set priority_group to.
 * 
 **/ 
extern void
nwamui_ncu_set_priority_group (   NwamuiNcu *self,
                                  gint       priority_group )
{
    g_return_if_fail (NWAMUI_IS_NCU (self));
    g_assert (priority_group >= 0 && priority_group <= G_MAXUINT );

    g_object_set (G_OBJECT (self),
                  "priority_group", priority_group,
                  NULL);
}

/**
 * nwamui_ncu_get_priority_group:
 * @nwamui_ncu: a #NwamuiNcu.
 * @returns: the priority_group.
 *
 **/
extern gint
nwamui_ncu_get_priority_group (NwamuiNcu *self)
{
    gint  priority_group = 0; 

    g_return_val_if_fail (NWAMUI_IS_NCU (self), priority_group);

    g_object_get (G_OBJECT (self),
                  "priority_group", &priority_group,
                  NULL);

    return( priority_group );
}

/** 
 * nwamui_ncu_set_priority_group_mode:
 * @nwamui_ncu: a #NwamuiNcu.
 * @priority_group_mode: Value to set priority_group_mode to.
 * 
 **/ 
extern void
nwamui_ncu_set_priority_group_mode (   NwamuiNcu                       *self,
                                       nwamui_cond_priority_group_mode_t     priority_group_mode )
{
    g_return_if_fail (NWAMUI_IS_NCU (self));
    g_assert (priority_group_mode >= NWAMUI_COND_PRIORITY_GROUP_MODE_EXCLUSIVE && priority_group_mode <= NWAMUI_COND_PRIORITY_GROUP_MODE_LAST );

    g_object_set (G_OBJECT (self),
                  "priority_group_mode", (gint)priority_group_mode,
                  NULL);
}

/**
 * nwamui_ncu_get_priority_group_mode:
 * @nwamui_ncu: a #NwamuiNcu.
 * @returns: the priority_group_mode.
 *
 **/
extern nwamui_cond_priority_group_mode_t
nwamui_ncu_get_priority_group_mode (NwamuiNcu *self)
{
    gint  priority_group_mode = NWAMUI_COND_PRIORITY_GROUP_MODE_EXCLUSIVE; 

    g_return_val_if_fail (NWAMUI_IS_NCU (self), priority_group_mode);

    g_object_get (G_OBJECT (self),
                  "priority_group_mode", &priority_group_mode,
                  NULL);

    return( (nwamui_cond_priority_group_mode_t)priority_group_mode );
}


/** 
 * nwamui_ncu_set_selection_conditions:
 * @nwamui_ncu: a #NwamuiNcu.
 * @selected_ncus: A GList of NCUs that are to be checked.
 * 
 **/ 
extern void
nwamui_ncu_set_selection_conditions( NwamuiNcu*                   self,
                                     GList*                       conditions )
{
    g_return_if_fail (NWAMUI_IS_NCU (self));

    g_object_set (G_OBJECT (self),
              "conditions", conditions,
              NULL);
    
}

/** 
 * nwamui_ncu_set_selection_conditions:
 * @nwamui_ncu: a #NwamuiNcu.
 * @selected_ncus: A GList of NCUs that are to be checked.
 * 
 **/ 
extern void
nwamui_ncu_selection_conditions_add( NwamuiNcu*  self,
                                    NwamuiCond*  condition_to_add )
{
    GList*  selected_conds;
    
    g_return_if_fail (NWAMUI_IS_NCU (self));

    g_object_get (G_OBJECT (self),
              "conditions", &selected_conds,
              NULL);
    
    if ( g_list_find( selected_conds, (gpointer)condition_to_add) == NULL ) {

        selected_conds = g_list_insert(selected_conds, (gpointer)g_object_ref(condition_to_add), 0 );

        g_object_set (G_OBJECT (self),
                  "conditions", selected_conds,
                  NULL);
    }
}

extern void
nwamui_ncu_selection_conditions_remove( NwamuiNcu*   self,
                                        NwamuiCond*  condition_to_remove )
{
    GList*  selected_conds = NULL;
    GList*  node = NULL ;
    
    g_return_if_fail (NWAMUI_IS_NCU (self));

    g_object_get (G_OBJECT (self),
              "conditions", &selected_conds,
              NULL);

    if ( (node = g_list_find( selected_conds, (gpointer)condition_to_remove)) != NULL ) {

        selected_conds = g_list_delete_link(selected_conds, node );

        g_object_unref( condition_to_remove );

        g_object_set (G_OBJECT (self),
                  "conditions", selected_conds,
                  NULL);
    }
    
}

extern gboolean
nwamui_ncu_selection_conditions_contains( NwamuiNcu*  self,
                                          NwamuiCond* cond_to_find )
{
    g_return_val_if_fail (NWAMUI_IS_NCU (self), FALSE );

    return( g_list_find( self->prv->conditions, cond_to_find) != NULL );
}

static void
nwamui_ncu_finalize (NwamuiNcu *self)
{
    if (self->prv->v4addresses != NULL ) {
        g_object_unref( G_OBJECT(self->prv->v4addresses) );
    }

    if (self->prv->v6addresses != NULL ) {
        g_object_unref( G_OBJECT(self->prv->v6addresses) );
    }
    if (self->prv->conditions != NULL ) {
        nwamui_util_free_obj_list(self->prv->conditions);
    }

    g_free (self->prv); 
    self->prv = NULL;

    parent_class->finalize (G_OBJECT (self));
}

static gchar*
get_nwam_ncu_string_prop( nwam_ncu_handle_t ncu, const char* prop_name )
{
    nwam_error_t        nerr;
    nwam_value_type_t   nwam_type;
    nwam_value_t        nwam_data;
    gchar*              retval = NULL;
    char*               value = NULL;

    g_return_val_if_fail( prop_name != NULL, retval );

    if ( (nerr = nwam_ncu_get_prop_type( prop_name, &nwam_type ) ) != NWAM_SUCCESS 
         || nwam_type != NWAM_VALUE_TYPE_STRING ) {
        g_warning("Unexpected type for ncu property %s\n", prop_name );
        return retval;
    }

    if ( (nerr = nwam_ncu_get_prop_value (ncu, prop_name, &nwam_data)) != NWAM_SUCCESS ) {
        g_debug("No value for ncu property %s, error = %s\n", prop_name, nwam_strerror( nerr ) );
        return retval;
    }

    if ( (nerr = nwam_value_get_string(nwam_data, &value )) != NWAM_SUCCESS ) {
        g_debug("Unable to get string value for ncu property %s, error = %s\n", prop_name, nwam_strerror( nerr ) );
        return retval;
    }

    if ( value != NULL ) {
        retval  = g_strdup ( value );
        free (value);
    }

    nwam_value_free(nwam_data);
    
    return( retval );
}

static gchar**
get_nwam_ncu_string_array_prop( nwam_ncu_handle_t ncu, const char* prop_name )
{
    nwam_error_t        nerr;
    nwam_value_type_t   nwam_type;
    nwam_value_t        nwam_data;
    gchar**             retval = NULL;
    char**              value = NULL;
    uint_t              num = 0;

    g_return_val_if_fail( prop_name != NULL, retval );

    if ( (nerr = nwam_ncu_get_prop_type( prop_name, &nwam_type ) ) != NWAM_SUCCESS 
         || nwam_type != NWAM_VALUE_TYPE_STRING ) {
        g_warning("Unexpected type for ncu property %s\n", prop_name );
        return retval;
    }

    if ( (nerr = nwam_ncu_get_prop_value (ncu, prop_name, &nwam_data)) != NWAM_SUCCESS ) {
        g_debug("No value for ncu property %s, error = %s\n", prop_name, nwam_strerror( nerr ) );
        return retval;
    }

    if ( (nerr = nwam_value_get_string_array(nwam_data, &value, &num )) != NWAM_SUCCESS ) {
        g_debug("Unable to get string value for ncu property %s, error = %s\n", prop_name, nwam_strerror( nerr ) );
        return retval;
    }

    if ( value != NULL && num > 0 ) {
        /* Create a NULL terminated list of stirngs, allocate 1 extra place
         * for NULL termination. */
        retval = (gchar**)g_malloc0( sizeof(gchar*) * (num+1) );

        for (int i = 0; i < num; i++ ) {
            retval[i]  = g_strdup ( value[i] );
        }
        retval[num]=NULL;
    }

    nwam_value_free(nwam_data);
    
    return( retval );
}

static gboolean
get_nwam_ncu_boolean_prop( nwam_ncu_handle_t ncu, const char* prop_name )
{
    nwam_error_t        nerr;
    nwam_value_type_t   nwam_type;
    nwam_value_t        nwam_data;
    boolean_t           value = FALSE;

    g_return_val_if_fail( prop_name != NULL, value );

    if ( (nerr = nwam_ncu_get_prop_type( prop_name, &nwam_type ) ) != NWAM_SUCCESS 
         || nwam_type != NWAM_VALUE_TYPE_BOOLEAN ) {
        g_warning("Unexpected type for ncu property %s\n", prop_name );
        return value;
    }

    if ( (nerr = nwam_ncu_get_prop_value (ncu, prop_name, &nwam_data)) != NWAM_SUCCESS ) {
        g_debug("No value for ncu property %s, error = %s\n", prop_name, nwam_strerror( nerr ) );
        return value;
    }

    if ( (nerr = nwam_value_get_boolean(nwam_data, &value )) != NWAM_SUCCESS ) {
        g_debug("Unable to get boolean value for ncu property %s, error = %s\n", prop_name, nwam_strerror( nerr ) );
        return value;
    }

    nwam_value_free(nwam_data);

    return( (gboolean)value );
}

static guint64
get_nwam_ncu_uint64_prop( nwam_ncu_handle_t ncu, const char* prop_name )
{
    nwam_error_t        nerr;
    nwam_value_type_t   nwam_type;
    nwam_value_t        nwam_data;
    uint64_t            value = 0;

    g_return_val_if_fail( prop_name != NULL, value );

    if ( (nerr = nwam_ncu_get_prop_type( prop_name, &nwam_type ) ) != NWAM_SUCCESS 
         || nwam_type != NWAM_VALUE_TYPE_UINT64 ) {
        g_warning("Unexpected type for ncu property %s\n", prop_name );
        return value;
    }

    if ( (nerr = nwam_ncu_get_prop_value (ncu, prop_name, &nwam_data)) != NWAM_SUCCESS ) {
        g_debug("No value for ncu property %s, error = %s\n", prop_name, nwam_strerror( nerr ) );
        return value;
    }

    if ( (nerr = nwam_value_get_uint64(nwam_data, &value )) != NWAM_SUCCESS ) {
        g_debug("Unable to get uint64 value for ncu property %s, error = %s\n", prop_name, nwam_strerror( nerr ) );
        return value;
    }

    nwam_value_free(nwam_data);

    return( (guint64)value );
}

static guint64* 
get_nwam_ncu_uint64_array_prop( nwam_ncu_handle_t ncu, const char* prop_name , guint *out_num )
{
    nwam_error_t        nerr;
    nwam_value_type_t   nwam_type;
    nwam_value_t        nwam_data;
    uint64_t           *value = NULL;
    uint_t              num = 0;
    guint64            *retval = NULL;

    g_return_val_if_fail( prop_name != NULL && out_num != NULL, retval );

    if ( (nerr = nwam_ncu_get_prop_type( prop_name, &nwam_type ) ) != NWAM_SUCCESS 
         || nwam_type != NWAM_VALUE_TYPE_UINT64 ) {
        g_warning("Unexpected type for ncu property %s\n", prop_name );
        return value;
    }

    if ( (nerr = nwam_ncu_get_prop_value (ncu, prop_name, &nwam_data)) != NWAM_SUCCESS ) {
        g_debug("No value for ncu property %s, error = %s\n", prop_name, nwam_strerror( nerr ) );
        return value;
    }

    if ( (nerr = nwam_value_get_uint64_array(nwam_data, &value, &num )) != NWAM_SUCCESS ) {
        g_debug("Unable to get uint64 value for ncu property %s, error = %s\n", prop_name, nwam_strerror( nerr ) );
        return value;
    }

    retval = (guint64*)g_malloc( sizeof(guint64) * num );
    for ( int i = 0; i < num; i++ ) {
        retval[i] = (guint64)value[i];
    }

    nwam_value_free(nwam_data);

    *out_num = num;

    return( retval );
}


/* Callbacks */

static void
object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data)
{
    NwamuiNcu* self = NWAMUI_NCU(data);
    gchar*     name = self->prv->device_name;

    g_debug("NwamuiNcu: %s: notify %s changed\n", name?name:"", arg1->name);
}

static void 
ip_row_inserted_or_changed_cb (GtkTreeModel *tree_model, GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
    NwamuiNcu* self = NWAMUI_NCU(data);
    gboolean is_v6 = (tree_model == GTK_TREE_MODEL(self->prv->v6addresses));

    /* Chain events in the tree to the NCU object*/
    g_object_notify(G_OBJECT(self), is_v6?"v6addresses":"v4addresses");
}

static void 
ip_row_deleted_cb (GtkTreeModel *tree_model, GtkTreePath *path, gpointer data)
{
    NwamuiNcu* self = NWAMUI_NCU(data);
    gboolean is_v6 = (tree_model == GTK_TREE_MODEL(self->prv->v6addresses));

    /* Chain events in the tree to the NCU object*/
    g_object_notify(G_OBJECT(self), is_v6?"v6addresses":"v4addresses");
}


