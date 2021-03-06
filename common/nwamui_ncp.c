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
 * File:   nwamui_ncp.c
 *
 */
#include <stdlib.h>

#include <glib-object.h>
#include <glib/gi18n.h>
#include <strings.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtktreestore.h>

#include "libnwamui.h"

#include <errno.h>
#include <sys/dlpi.h>
#include <libdllink.h>

static NwamuiNcp       *instance        = NULL;
static gint _num_wireless = 0; /* Count wireless i/fs */

enum {
    PROP_PRIORITY_GROUP = 1,
    PROP_NCU_LIST,
    PROP_NCU_LIST_STORE,
    PROP_ACTIVE_NCU,
    PROP_WIRELESS_LINK_NUM
};

typedef struct {
    /* Input */
    gint64          current_prio;

    /* Output */
    guint32         num_manual_enabled;
    guint32         num_manual_online;
    guint32         num_prio_excl;
    guint32         num_prio_excl_online;
    guint32         num_prio_shared;
    guint32         num_prio_shared_online;
    guint32         num_prio_all;
    guint32         num_prio_all_online;
    NwamuiNcu      *needs_wifi_selection;
    NwamuiWifiNet  *needs_wifi_key;
    GString        *report;
} check_online_info_t;

struct _NwamuiNcpPrivate {
    nwam_ncp_handle_t nwam_ncp;
    gchar*            name;
    gboolean          nwam_ncp_modified;
    gint              wireless_link_num;

    GList*        ncu_list;
    GtkListStore* ncu_list_store;

    GList* temp_list; /* Used to temporarily track not found objects in walkers */

    /* Cached Priority Group */
    gint   priority_group;
};

#define NWAMUI_NCP_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE((o), NWAMUI_TYPE_NCP, NwamuiNcpPrivate))

static void nwamui_ncp_set_property ( GObject         *object,
                                      guint            prop_id,
                                      const GValue    *value,
                                      GParamSpec      *pspec);

static void nwamui_ncp_get_property ( GObject         *object,
                                      guint            prop_id,
                                      GValue          *value,
                                      GParamSpec      *pspec);

static void nwamui_ncp_finalize (     NwamuiNcp *self);

static gint         nwamui_object_real_open(NwamuiObject *object, const gchar *name, gint flag);
static nwam_state_t  nwamui_object_real_get_nwam_state(NwamuiObject *object, nwam_aux_state_t* aux_state_p, const gchar**aux_state_string_p);
static const gchar*  nwamui_object_real_get_name ( NwamuiObject *object );
static gboolean      nwamui_object_real_can_rename(NwamuiObject *object);
static gboolean      nwamui_object_real_set_name(NwamuiObject *object, const gchar* name);
static void          nwamui_object_real_set_active ( NwamuiObject *object, gboolean active );
static gboolean      nwamui_object_real_get_active( NwamuiObject *object );
static gboolean      nwamui_object_real_validate(NwamuiObject *object, gchar **prop_name_ret);
static gboolean      nwamui_object_real_commit( NwamuiObject *object );
static void          nwamui_object_real_reload(NwamuiObject* object);
static gboolean      nwamui_object_real_destroy( NwamuiObject* object );
static gboolean      nwamui_object_real_is_modifiable(NwamuiObject *object);
static gboolean      nwamui_object_real_has_modifications(NwamuiObject* object);
static NwamuiObject* nwamui_object_real_clone(NwamuiObject *object, const gchar *name, NwamuiObject *parent);
static void          nwamui_object_real_add(NwamuiObject *object, NwamuiObject *child);
static void          nwamui_object_real_remove(NwamuiObject *object, NwamuiObject *child);

/* Callbacks */
static int nwam_ncu_walker_cb (nwam_ncu_handle_t ncu, void *data);
static void ncu_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data);
static void row_deleted_cb (GtkTreeModel *tree_model, GtkTreePath *path, gpointer user_data);
static void row_inserted_cb (GtkTreeModel *tree_model, GtkTreePath *path, GtkTreeIter *iter, gpointer user_data);
static void rows_reordered_cb(GtkTreeModel *tree_model, GtkTreePath *path, GtkTreeIter *iter, gpointer arg3, gpointer user_data);

/* GList callbacks */
static gint find_ncu_by_device_name(gconstpointer data, gconstpointer user_data);
static gint find_first_wifi_net(gconstpointer data, gconstpointer user_data);
static void find_wireless_ncu(gpointer obj, gpointer user_data);

G_DEFINE_TYPE (NwamuiNcp, nwamui_ncp, NWAMUI_TYPE_OBJECT)

static void
nwamui_ncp_class_init (NwamuiNcpClass *klass)
{
    /* Pointer to GObject Part of Class */
    GObjectClass *gobject_class = (GObjectClass*) klass;
    NwamuiObjectClass *nwamuiobject_class = NWAMUI_OBJECT_CLASS(klass);
    
    /* Override Some Function Pointers */
    gobject_class->set_property = nwamui_ncp_set_property;
    gobject_class->get_property = nwamui_ncp_get_property;
    gobject_class->finalize = (void (*)(GObject*)) nwamui_ncp_finalize;

    nwamuiobject_class->open = nwamui_object_real_open;
    nwamuiobject_class->get_name = nwamui_object_real_get_name;
    nwamuiobject_class->set_name = nwamui_object_real_set_name;
    nwamuiobject_class->can_rename = nwamui_object_real_can_rename;
    nwamuiobject_class->get_active = nwamui_object_real_get_active;
    nwamuiobject_class->set_active = nwamui_object_real_set_active;
    nwamuiobject_class->get_nwam_state = nwamui_object_real_get_nwam_state;
    nwamuiobject_class->validate = nwamui_object_real_validate;
    nwamuiobject_class->commit = nwamui_object_real_commit;
    nwamuiobject_class->reload = nwamui_object_real_reload;
    nwamuiobject_class->destroy = nwamui_object_real_destroy;
    nwamuiobject_class->is_modifiable = nwamui_object_real_is_modifiable;
    nwamuiobject_class->has_modifications = nwamui_object_real_has_modifications;
    nwamuiobject_class->clone = nwamui_object_real_clone;

    nwamuiobject_class->add = nwamui_object_real_add;
    nwamuiobject_class->remove = nwamui_object_real_remove;

	g_type_class_add_private(klass, sizeof(NwamuiNcpPrivate));

    /* Create some properties */
    g_object_class_install_property (gobject_class,
      PROP_PRIORITY_GROUP,
      g_param_spec_int64("priority_group",
        _("priority group"),
        _("priority group"),
        0,
        G_MAXINT64,
        0,
        G_PARAM_READABLE));

    g_object_class_install_property (gobject_class,
                                     PROP_NCU_LIST,
                                     g_param_spec_pointer ("ncu_list",
                                                          _("GList of NCUs in the NCP"),
                                                          _("GList of NCUs in the NCP"),
                                                           G_PARAM_READABLE));

    g_object_class_install_property (gobject_class,
                                     PROP_NCU_LIST_STORE,
                                     g_param_spec_object ("ncu_list_store",
                                                          _("ListStore of NCUs in the NCP"),
                                                          _("ListStore of NCUs in the NCP"),
                                                          GTK_TYPE_LIST_STORE,
                                                          G_PARAM_READABLE));


    g_object_class_install_property (gobject_class,
                                     PROP_WIRELESS_LINK_NUM,
                                     g_param_spec_int("wireless_link_num",
                                                      _("wireless_link_num"),
                                                      _("wireless_link_num"),
                                                      0,
                                                      G_MAXINT,
                                                      0,
                                                      G_PARAM_READABLE));

    /* Create some signals */
}


static void
nwamui_ncp_init ( NwamuiNcp *self)
{
    NwamuiNcpPrivate *prv = NWAMUI_NCP_GET_PRIVATE(self);
    nwam_error_t      nerr;
    int64_t           pg  = 0;

    self->prv = prv;
    
    /* Used to store the list of NCUs that are added or removed in the UI but
     * not yet committed.
     */

    /* Init pri group. */
    if ( (nerr = nwam_ncp_get_active_priority_group( &pg )) == NWAM_SUCCESS ) {
        prv->priority_group = (gint64)pg;
    } else {
        nwamui_debug("Error getting active priortiy group: %d (%s)", 
          nerr, nwam_strerror(nerr) );
    }

    prv->ncu_list_store = gtk_list_store_new ( 1, NWAMUI_TYPE_NCU);

    g_signal_connect(prv->ncu_list_store, "row_deleted", G_CALLBACK(row_deleted_cb), (gpointer)self);
    g_signal_connect(prv->ncu_list_store, "row_inserted", G_CALLBACK(row_inserted_cb), (gpointer)self);
    g_signal_connect(prv->ncu_list_store, "rows_reordered", G_CALLBACK(rows_reordered_cb), (gpointer)self);
}

static void
nwamui_ncp_set_property (   GObject         *object,
                            guint            prop_id,
                            const GValue    *value,
                            GParamSpec      *pspec)
{
    NwamuiNcp*      self = NWAMUI_NCP(object);
    gchar*          tmpstr = NULL;
    gint            tmpint = 0;
    nwam_error_t    nerr;
    gboolean        read_only = FALSE;

    read_only = !nwamui_object_is_modifiable(NWAMUI_OBJECT(self));

    if (read_only) {
        g_warning("Attempting to modify read-only ncp %s", self->prv->name);
        return;
    }

    switch (prop_id) {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
nwamui_ncp_get_property (   GObject         *object,
                            guint            prop_id,
                            GValue          *value,
                            GParamSpec      *pspec)
{
    NwamuiNcp *self = NWAMUI_NCP(object);

    switch (prop_id) {
    case PROP_PRIORITY_GROUP:
        g_value_set_int64(value, nwamui_ncp_get_prio_group(self));
        break;
        case PROP_NCU_LIST: {
                g_value_set_pointer( value, nwamui_util_copy_obj_list( self->prv->ncu_list ) );
            }
            break;
        case PROP_NCU_LIST_STORE: {
                g_value_set_object( value, self->prv->ncu_list_store );
            }
            break;
        case PROP_WIRELESS_LINK_NUM:
            g_value_set_int( value, nwamui_ncp_get_wireless_link_num(self));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
nwamui_ncp_finalize (NwamuiNcp *self)
{

    g_free( self->prv->name);
    
    if ( self->prv->ncu_list != NULL ) {
        nwamui_util_free_obj_list(self->prv->ncu_list);
    }
    
    if ( self->prv->ncu_list_store != NULL ) {
        gtk_list_store_clear(self->prv->ncu_list_store);
        g_object_unref(self->prv->ncu_list_store);
    }

    if ( self->prv->nwam_ncp != NULL ) {
        nwam_ncp_free( self->prv->nwam_ncp );
    }
    
    self->prv = NULL;

	G_OBJECT_CLASS(nwamui_ncp_parent_class)->finalize(G_OBJECT(self));
}

static nwam_state_t
nwamui_object_real_get_nwam_state(NwamuiObject *object, nwam_aux_state_t* aux_state_p, const gchar**aux_state_string_p)
{
    nwam_state_t    rstate = NWAM_STATE_UNINITIALIZED;

    g_return_val_if_fail(NWAMUI_IS_NCP( object ), rstate);

    if ( aux_state_p ) {
        *aux_state_p = NWAM_AUX_STATE_UNINITIALIZED;
    }

    if ( aux_state_string_p ) {
        *aux_state_string_p = (const gchar*)nwam_aux_state_to_string( NWAM_AUX_STATE_UNINITIALIZED );
    }

    {
        NwamuiNcp   *ncp = NWAMUI_NCP(object);
        nwam_state_t        state;
        nwam_aux_state_t    aux_state;


        /* First look at the LINK state, then the IP */
        if ( ncp->prv->nwam_ncp &&
             nwam_ncp_get_state( ncp->prv->nwam_ncp, &state, &aux_state ) == NWAM_SUCCESS ) {

            rstate = state;
            if ( aux_state_p ) {
                *aux_state_p = aux_state;
            }
            if ( aux_state_string_p ) {
                *aux_state_string_p = (const gchar*)nwam_aux_state_to_string( aux_state );
            }
        }
    }

    return(rstate);
}

extern NwamuiObject*
nwamui_ncp_new(const gchar* name )
{
    NwamuiObject* self = NWAMUI_OBJECT(g_object_new (NWAMUI_TYPE_NCP, NULL));

    nwamui_object_set_name(NWAMUI_OBJECT(self), name);

    /* Try open only, do not create. */
    if (nwamui_object_real_open(self, name, NWAMUI_OBJECT_OPEN) != 0) {
        g_debug("NCP %s open handle failed", name);
    }

    return NWAMUI_OBJECT(self);
}

/**
 * nwamui_ncp_new_with_handle
 * @returns: a #NwamuiNcp.
 *
 **/
extern NwamuiObject*
nwamui_ncp_new_with_handle (nwam_ncp_handle_t ncp)
{
    nwam_error_t  nerr;
    char         *name   = NULL;
    NwamuiObject *object = NULL;

    if ((nerr = nwam_ncp_get_name (ncp, &name)) != NWAM_SUCCESS) {
        g_debug("Failed to get name for ncp, error: %s", nwam_strerror(nerr));
        return NULL;
    }

    object = g_object_new(NWAMUI_TYPE_NCP, NULL);
    g_assert(NWAMUI_IS_NCP(object));

    /* Will update handle. */
    nwamui_object_set_name(object, name);

    nwamui_object_real_open(object, name, NWAMUI_OBJECT_OPEN);

    nwamui_object_real_reload(object);

    free(name);

    return object;
}

/**
 * nwamui_object_real_clone:
 * @returns: a copy of an existing #NwamuiNcp, with the name specified.
 *
 * Creates a new #NwamuiNcp and copies properties.
 *
 **/
static NwamuiObject*
nwamui_object_real_clone(NwamuiObject *object, const gchar *name, NwamuiObject *parent)
{
    NwamuiNcp         *self    = NWAMUI_NCP(object);
    NwamuiObject      *new_ncp = NULL;;
    nwam_ncp_handle_t  new_ncp_h;
    nwam_error_t       nerr;
    NwamuiNcpPrivate *new_prv;

    g_return_val_if_fail(NWAMUI_IS_NCP(object), NULL);
    g_return_val_if_fail(name != NULL, NULL);

    nerr = nwam_ncp_copy (self->prv->nwam_ncp, name, &new_ncp_h);

    if ( nerr != NWAM_SUCCESS ) { 
        nwamui_warning("Failed to clone new NCP %s from existing NCP %s: %s",
          name, self->prv->name, nwam_strerror(nerr) );
        return new_ncp;
    }

    /* new_ncp = nwamui_ncp_new_with_handle(new_ncp_h); */
    new_ncp = NWAMUI_OBJECT(g_object_new(NWAMUI_TYPE_NCP,
        "name", name,
        NULL));
    new_prv = NWAMUI_NCP_GET_PRIVATE(new_ncp);
    new_prv->nwam_ncp = new_ncp_h;
    new_prv->nwam_ncp_modified = TRUE;

    return new_ncp;
}

/**
 *  Check for new NCUs - useful after reactivation of daemon or signal for new
 *  NCUs.
 **/
static void
nwamui_object_real_reload(NwamuiObject* object)
{
    NwamuiNcpPrivate  *prv                  = NWAMUI_NCP_GET_PRIVATE(object);
    int                cb_ret               = 0;
    nwam_error_t       nerr;

    g_return_if_fail(NWAMUI_IS_NCP(object));

    nwamui_object_real_open(object, prv->name, NWAMUI_OBJECT_OPEN);

    g_return_if_fail(prv->nwam_ncp != NULL );

    g_object_freeze_notify(G_OBJECT(object));
    g_object_freeze_notify(G_OBJECT(prv->ncu_list_store));

    prv->temp_list = g_list_copy(prv->ncu_list);

    _num_wireless = 0;

    g_debug ("### nwam_ncp_walk_ncus start ###");
    nerr = nwam_ncp_walk_ncus( prv->nwam_ncp, nwam_ncu_walker_cb, (void*)object,
      NWAM_FLAG_NCU_TYPE_CLASS_ALL, &cb_ret );
    if (nerr == NWAM_SUCCESS) {
        for(;
            prv->temp_list != NULL;
            prv->temp_list = g_list_delete_link(prv->temp_list, prv->temp_list) ) {
            nwamui_object_remove(object, NWAMUI_OBJECT(prv->temp_list->data));
        }
    } else {
        nwamui_warning("nwam_ncp_walk_ncus %s for ncp '%s'", nwam_strerror(nerr), prv->name);
        g_list_free(prv->temp_list);
        prv->temp_list = NULL;
    }
    g_debug ("### nwam_ncp_walk_ncus  end ###");

    if ( prv->wireless_link_num != _num_wireless ) {
        prv->wireless_link_num = _num_wireless;
        g_object_notify(G_OBJECT(object), "wireless_link_num" );
    }

    g_object_thaw_notify(G_OBJECT(prv->ncu_list_store));
    g_object_thaw_notify(G_OBJECT(object));
}

extern nwam_ncp_handle_t
nwamui_ncp_get_nwam_handle( NwamuiNcp* self )
{
    return (self->prv->nwam_ncp);
}

static gint
nwamui_object_real_open(NwamuiObject *object, const gchar *name, gint flag)
{
    NwamuiNcpPrivate *prv = NWAMUI_NCP_GET_PRIVATE(object);
    nwam_error_t      nerr;

    g_assert(name);

    if (flag == NWAMUI_OBJECT_CREATE) {

        nerr = nwam_ncp_create(name, NULL, &prv->nwam_ncp);
        if (nerr == NWAM_SUCCESS) {
            prv->nwam_ncp_modified = TRUE;
        } else {
            g_warning("nwamui_ncp_create error creating nwam_ncp_handle %s", name);
            prv->nwam_ncp = NULL;
        }
    } else if (flag == NWAMUI_OBJECT_OPEN) {
        nwam_ncp_handle_t  handle;

        nerr = nwam_ncp_read(name, 0, &handle);
        if (nerr == NWAM_SUCCESS) {
            if (prv->nwam_ncp) {
                nwam_ncp_free(prv->nwam_ncp);
            }
            prv->nwam_ncp = handle;
        } else if (nerr == NWAM_ENTITY_NOT_FOUND) {
            /* Most likely only exists in memory right now, so we should use
             * handle passed in as parameter. In clone mode, the new handle
             * gets from nwam_ncp_copy can't be read again.
             */
            g_debug("Failed to read ncp information for %s error: %s", name, nwam_strerror(nerr));
        } else {
            g_warning("Failed to read ncp information for %s error: %s", name, nwam_strerror(nerr));
            prv->nwam_ncp = NULL;
        }
    } else {
        g_assert_not_reached();
    }
    return nerr;
}

/**
 * nwamui_object_real_get_name:
 * @returns: null-terminated C String with name of the the NCP.
 *
 **/
static const gchar*
nwamui_object_real_get_name ( NwamuiObject *object )
{
    NwamuiNcp    *self       = NWAMUI_NCP(object);
    g_return_val_if_fail (NWAMUI_IS_NCP(self), NULL); 
    
    return self->prv->name;
}

static gboolean
nwamui_object_real_can_rename(NwamuiObject *object)
{
    NwamuiNcp *self = NWAMUI_NCP(object);
    NwamuiNcpPrivate *prv = NWAMUI_NCP(object)->prv;

    g_return_val_if_fail (NWAMUI_IS_NCP(object), FALSE);
    /* if ( prv->nwam_ncp != NULL ) { */
    /*     if (nwam_ncp_can_set_name( prv->nwam_ncp )) { */
    /*         return( TRUE ); */
    /*     } */
    /* } */
    /* return FALSE; */

    return (prv->nwam_ncp == NULL);
}

static gboolean
nwamui_object_real_set_name( NwamuiObject *object, const gchar* name )
{
    NwamuiNcpPrivate *prv  = NWAMUI_NCP_GET_PRIVATE(object);
    nwam_error_t      nerr;

    g_return_val_if_fail(NWAMUI_IS_NCP(object), FALSE);
    g_return_val_if_fail(name, FALSE);

    /* Initially set name or rename. */
    if (prv->name) {
        /* /\* we may rename here *\/ */
        /* if (prv->nwam_ncp != NULL) { */
        /*     nerr = nwam_ncp_set_name (prv->nwam_ncp, name); */
        /*     if (nerr != NWAM_SUCCESS) { */
        /*         g_debug ("nwam_ncp_set_name %s error: %s", name, nwam_strerror (nerr)); */
        /*     } */
        /* } */
        /* else { */
        /*     g_warning("Unexpected null ncp handle"); */
        /* } */

        if (prv->nwam_ncp == NULL) {
            nerr = nwam_ncp_read (name, 0, &prv->nwam_ncp);
            if (nerr == NWAM_SUCCESS) {
                /* g_debug ("nwamui_ncp_read found nwam_ncp_handle %s", name); */
                /* NCP is existed. */
                nwam_ncp_free(prv->nwam_ncp);
                prv->nwam_ncp = NULL;
                return FALSE;
            }
        }
        prv->nwam_ncp_modified = TRUE;
        g_free(prv->name);
    }

    prv->name = g_strdup(name);
    return TRUE;
}

/**
 * nwamui_ncp_is_active:
 * @nwamui_ncp: a #NwamuiNcp.
 * @returns: TRUE if the ncp is online.
 *
 **/
extern gboolean
nwamui_object_real_get_active(NwamuiObject *object)
{
    NwamuiNcpPrivate *prv       = NWAMUI_NCP_GET_PRIVATE(object);
    nwam_state_t      state     = NWAM_STATE_OFFLINE;
    nwam_aux_state_t  aux_state = NWAM_AUX_STATE_UNINITIALIZED;

    g_return_val_if_fail(NWAMUI_IS_NCP(object), FALSE);

    if (prv->nwam_ncp) {
        state = nwamui_object_get_nwam_state(object, &aux_state, NULL);
    }

    return (state == NWAM_STATE_ONLINE);
}

/** 
 * nwamui_object_real_set_active:
 * @nwamui_ncp: a #NwamuiEnv.
 * @active: Immediately activates/deactivates the ncp.
 * 
 **/ 
extern void
nwamui_object_real_set_active (NwamuiObject *object, gboolean active)
{
    NwamuiNcp    *self       = NWAMUI_NCP(object);
    /* Activate immediately */
    nwam_state_t        state = NWAM_STATE_OFFLINE;
    nwam_aux_state_t    aux_state = NWAM_AUX_STATE_UNINITIALIZED;

    g_return_if_fail (NWAMUI_IS_NCP (self));

    state = nwamui_object_get_nwam_state(object, &aux_state, NULL);

    if ( state != NWAM_STATE_ONLINE && active ) {
        nwam_error_t nerr;
        if ( (nerr = nwam_ncp_enable (self->prv->nwam_ncp)) != NWAM_SUCCESS ) {
            g_warning("Failed to enable ncp due to error: %s", nwam_strerror(nerr));
        }
    }
    else {
        g_warning("Cannot disable an NCP, enable another one to do this");
    }
}

/**
 * nwamui_object_real_is_modifiable:
 * @nwamui_ncp: a #NwamuiNcp.
 * @returns: the modifiable.
 *
 **/
extern gboolean
nwamui_object_real_is_modifiable(NwamuiObject *object)
{
    NwamuiNcp    *self       = NWAMUI_NCP(object);
    nwam_error_t  nerr;
    gboolean      modifiable = FALSE; 
    boolean_t     readonly;

    g_assert(NWAMUI_IS_NCP (self));
    if (self->prv->nwam_ncp == NULL ) {
        return TRUE;
    }

    if ((nerr = nwam_ncp_get_read_only( self->prv->nwam_ncp, &readonly )) == NWAM_SUCCESS) {
        modifiable = readonly?FALSE:TRUE;
    } else {
        g_warning("Error getting ncp read-only status: %s", nwam_strerror( nerr ) );
    }

    /* if (self->prv->name && strcmp( self->prv->name, NWAM_NCP_NAME_AUTOMATIC) == 0 ) { */
    /*     modifiable = FALSE; */
    /* } */
    /* else { */
    /*     modifiable = TRUE; */
    /* } */

    return( modifiable );
}

static gboolean
nwamui_object_real_has_modifications(NwamuiObject* object)
{
    NwamuiNcpPrivate *prv  = NWAMUI_NCP_GET_PRIVATE(object);

    /* Always return true, because NCUs need be iterated for committing. */
    return TRUE;
}

extern gint64
nwamui_ncp_get_prio_group( NwamuiNcp* self )
{
    g_return_val_if_fail(NWAMUI_IS_NCP(self), (gint64)0);

    return self->prv->priority_group;
}

extern void
nwamui_ncp_set_prio_group( NwamuiNcp* self, gint64 new_prio ) 
{
    g_return_if_fail(NWAMUI_IS_NCP(self));

    if (self->prv->priority_group != new_prio) {
        self->prv->priority_group = new_prio;
        g_object_notify(G_OBJECT(self), "priority-group");
    }
}

static void
check_ncu_online( gpointer obj, gpointer user_data )
{
	NwamuiNcu           *ncu = NWAMUI_NCU(obj);
	check_online_info_t *info_p = (check_online_info_t*)user_data;
    nwam_state_t         state;
    nwam_aux_state_t     aux_state;
    gboolean             online = FALSE;
    nwamui_cond_activation_mode_t activation_mode;
    nwamui_cond_priority_group_mode_t prio_group_mode;


    if ( ncu == NULL || !NWAMUI_IS_NCU(ncu) ) {
        return;
    }

    activation_mode = nwamui_object_get_activation_mode(NWAMUI_OBJECT(ncu));
    state = nwamui_object_get_nwam_state( NWAMUI_OBJECT(ncu), &aux_state, NULL);

    if ( state == NWAM_STATE_ONLINE && aux_state == NWAM_AUX_STATE_UP ) {
        online = TRUE;
    }

    g_string_append_printf(info_p->report, " %s(%s),", nwamui_object_get_name(NWAMUI_OBJECT(ncu)), online?"ON":"OFF");

    switch (activation_mode) { 
        case NWAMUI_COND_ACTIVATION_MODE_MANUAL: {
            if ( nwamui_object_get_enabled(NWAMUI_OBJECT(ncu)) ) {
                    /* Only count if expected to be enabled. */
                    info_p->num_manual_enabled++;
                    if ( online ) {
                        info_p->num_manual_online++;
                    }
                }
            }
            break;
        case NWAMUI_COND_ACTIVATION_MODE_PRIORITIZED: {
                prio_group_mode = nwamui_ncu_get_priority_group_mode( ncu );

                if ( info_p->current_prio != nwamui_ncu_get_priority_group( ncu ) ) {
                    /* Skip objects not in current_prio group */
                    return;
                }

                switch (prio_group_mode) {
                    case NWAMUI_COND_PRIORITY_GROUP_MODE_EXCLUSIVE:
                        info_p->num_prio_excl++;
                        if ( online ) {
                            info_p->num_prio_excl_online++;
                        }
                        break;
                    case NWAMUI_COND_PRIORITY_GROUP_MODE_SHARED:
                        info_p->num_prio_shared++;
                        if ( online ) {
                            info_p->num_prio_shared_online++;
                        }
                        break;
                    case NWAMUI_COND_PRIORITY_GROUP_MODE_ALL:
                        info_p->num_prio_all++;
                        if ( online ) {
                            info_p->num_prio_all_online++;
                        }
                        break;
                }
            }
            break;
        default:
            break;
    }

    /* For link events */
    state = nwamui_ncu_get_link_nwam_state(ncu, &aux_state, NULL);

    if ( aux_state == NWAM_AUX_STATE_LINK_WIFI_NEED_SELECTION ) {
        if ( info_p->needs_wifi_selection == NULL ) {
            info_p->needs_wifi_selection = NWAMUI_NCU(g_object_ref(ncu));
        }
    }

    if ( aux_state == NWAM_AUX_STATE_LINK_WIFI_NEED_KEY) {
        if ( info_p->needs_wifi_key == NULL ) {
            info_p->needs_wifi_key = nwamui_ncu_get_wifi_info(ncu);
        }
    }

}

/**
 * nwamui_ncp_all_ncus_online:
 * @nwamui_ncp: a #NwamuiNcp.
 *
 * Sets needs_wifi_selection to point to the NCU needing selection, or NULL.
 * Sets needs_wifi_key to point to the WifiNet needing selection, or NULL.
 *
 * @returns: TRUE if all the expected NCUs in the NCP are online
 *
 **/
extern gboolean
nwamui_ncp_all_ncus_online (NwamuiNcp       *self,
                            NwamuiNcu      **needs_wifi_selection,
                            NwamuiWifiNet  **needs_wifi_key )
{
    nwam_error_t            nerr;
    gboolean                all_online = TRUE;
    check_online_info_t     info;

    if (!NWAMUI_IS_NCP (self) && self->prv->nwam_ncp == NULL ) {
        return( FALSE );
    }

    bzero((void*)&info, sizeof(check_online_info_t));
    
    info.current_prio = nwamui_ncp_get_prio_group( self );

    if ( self->prv->ncu_list == NULL ) {
        /* If there are no NCUs then something is wrong and 
         * we are not on-line 
         */
        return( FALSE );
    }

    info.report = g_string_new("");
    g_string_append_printf(info.report, "NCP %s:", nwamui_object_get_name(NWAMUI_OBJECT(self)));
    g_list_foreach(self->prv->ncu_list, check_ncu_online, &info );
    nwamui_debug("%s", info.report->str);
    g_string_free(info.report, TRUE);

    if ( info.num_manual_enabled != info.num_manual_online ) {
        all_online = FALSE;
    }
    else if ( info.num_prio_excl > 0 && info.num_prio_excl_online == 0 ) {
        all_online = FALSE;
    }
    else if (info.num_prio_shared > 0 && (info.num_prio_shared < info.num_prio_shared_online || info.num_prio_shared_online < 1)) {
        all_online = FALSE;
    }
    else if ( info.num_prio_all != info.num_prio_all_online ) {
        all_online = FALSE;
    }

    if ( ! all_online ) {
        /* Only care about these values if we see something off-line that
         * should be on-line.
         */
        if ( needs_wifi_selection != NULL ) {
            /* No need to ref, since will already by ref-ed */
            *needs_wifi_selection = info.needs_wifi_selection;
        }
        if ( needs_wifi_key != NULL ) {
            /* No need to ref, since will already by ref-ed */
            *needs_wifi_key = info.needs_wifi_key;
        }
    }
    else {
        if ( info.needs_wifi_selection != NULL ) {
            g_object_unref(G_OBJECT(info.needs_wifi_selection));
        }
        if ( info.needs_wifi_key != NULL ) {
            g_object_unref(G_OBJECT(info.needs_wifi_key));
        }
    }

    return( all_online );
}

extern gint
nwamui_ncp_get_ncu_num(NwamuiNcp *self)
{
    g_return_val_if_fail (NWAMUI_IS_NCP(self), 0); 

    return g_list_length(self->prv->ncu_list);
}

extern  GList*
nwamui_ncp_find_ncu( NwamuiNcp *self, GCompareFunc func, gconstpointer data)
{
    g_return_val_if_fail(NWAMUI_IS_NCP(self), NULL);

    if (func)
        return g_list_find_custom(self->prv->ncu_list, data, func);
    else
        return g_list_find(self->prv->ncu_list, data);
}

/**
 * nwamui_ncp_get_ncu_list_store_store:
 * @returns: GList containing NwamuiNcu elements
 *
 **/
extern GtkListStore*
nwamui_ncp_get_ncu_list_store( NwamuiNcp *self )
{
    GtkListStore* ncu_list_store = NULL;
    
    if ( self == NULL ) {
        return( NULL );
    }

    g_return_val_if_fail (NWAMUI_IS_NCP(self), ncu_list_store); 
    
    g_object_get (G_OBJECT (self),
                  "ncu_list_store", &ncu_list_store,
                  NULL);

    return( ncu_list_store );
}


/**
 * nwamui_ncp_foreach_ncu
 * 
 * Calls func for each NCU in the NCP
 *
 **/
extern void
nwamui_ncp_foreach_ncu(NwamuiNcp *self, GFunc func, gpointer user_data)
{
    NwamuiNcpPrivate *prv  = NWAMUI_NCP_GET_PRIVATE(self);
    g_return_if_fail(func);
    g_list_foreach(prv->ncu_list, func, user_data);
}

static void
foreach_wireless_ncu_foreach_wifi_net(gpointer data, gpointer user_data)
{
    gpointer  *data_set = (gpointer *)user_data;
	NwamuiNcu *ncu  = data;

    g_return_if_fail(NWAMUI_IS_NCU(ncu));

	if (nwamui_ncu_get_ncu_type(ncu) == NWAMUI_NCU_TYPE_WIRELESS) {
        nwamui_ncu_wifi_hash_foreach(ncu, (GHFunc)data_set[1], data_set[2]);
	}
}

extern void
nwamui_ncp_foreach_ncu_foreach_wifi_info(NwamuiNcp *self, GHFunc func, gpointer user_data)
{
    NwamuiNcpPrivate *prv  = NWAMUI_NCP_GET_PRIVATE(self);
    gpointer data_set[] = { self, (gpointer)func, user_data };

    g_return_if_fail(func);
    g_list_foreach(prv->ncu_list, foreach_wireless_ncu_foreach_wifi_net, data_set);
}

/**
 * nwamui_ncp_get_ncu_by_device_name
 * 
 * Returns a pointer to an NCU given the device name.
 * Unref is needed
 *
 **/
extern  NwamuiObject*
nwamui_ncp_get_ncu_by_device_name( NwamuiNcp *self, const gchar* device_name )
{
    NwamuiNcpPrivate *prv             = NWAMUI_NCP_GET_PRIVATE(self);
    NwamuiObject     *ret_ncu         = NULL;
    gchar            *ncu_device_name = NULL;
    GList            *found_list;

    g_return_val_if_fail (device_name, ret_ncu ); 

    found_list = g_list_find_custom(self->prv->ncu_list, (gpointer)device_name, find_ncu_by_device_name);

    if (found_list) {
        ret_ncu = NWAMUI_OBJECT(g_object_ref(found_list->data));
        /* nwamui_debug("NCP %s found NCU %s (0x%p) OK", prv->name, device_name, found_list->data); */
    } else {
        nwamui_debug("NCP %s found NCU %s FAILED", prv->name, device_name);
    }
    return ret_ncu;
}

static void
nwamui_object_real_remove(NwamuiObject *object, NwamuiObject *child)
{
    NwamuiNcpPrivate *prv        = NWAMUI_NCP_GET_PRIVATE(object);
    NwamuiNcp        *self       = NWAMUI_NCP(object);
    GtkTreeIter       iter;
    gboolean          valid_iter = FALSE;

    g_return_if_fail (NWAMUI_IS_NCP(self) && NWAMUI_IS_NCU(child) );

    g_object_freeze_notify(G_OBJECT(self));
    g_object_freeze_notify(G_OBJECT(prv->ncu_list_store));

    for (valid_iter = gtk_tree_model_get_iter_first( GTK_TREE_MODEL(prv->ncu_list_store), &iter);
         valid_iter;
         valid_iter = gtk_tree_model_iter_next( GTK_TREE_MODEL(prv->ncu_list_store), &iter)) {
        NwamuiNcu      *_ncu;

        gtk_tree_model_get( GTK_TREE_MODEL(prv->ncu_list_store), &iter, 0, &_ncu, -1);


        if ( _ncu == (gpointer)child ) {
            gtk_list_store_remove(GTK_LIST_STORE(prv->ncu_list_store), &iter);

            if ( nwamui_ncu_get_ncu_type( _ncu ) == NWAMUI_NCU_TYPE_WIRELESS ) {
                prv->wireless_link_num--;
                g_object_notify(G_OBJECT(self), "wireless_link_num" );
            }

            g_object_notify(G_OBJECT(self), "ncu_list_store" );
            g_object_unref(_ncu);
            break;
        }
        g_object_unref(_ncu);
    }

    prv->ncu_list = g_list_remove(prv->ncu_list, child);
    g_debug("Remove '%s(0x%p)' from '%s'", nwamui_object_get_name(child), child, nwamui_object_get_name(object));
    g_object_unref(child);

    g_object_thaw_notify(G_OBJECT(prv->ncu_list_store));
    g_object_thaw_notify(G_OBJECT(self));
}

static void
nwamui_object_real_add(NwamuiObject *object, NwamuiObject *child)
{
    NwamuiNcpPrivate *prv         = NWAMUI_NCP_GET_PRIVATE(object);
    NwamuiNcp        *self        = NWAMUI_NCP(object);
    GtkTreeIter       iter;
    gboolean          valid_iter  = FALSE;

    g_return_if_fail (NWAMUI_IS_NCP(self) && NWAMUI_IS_NCU(child) );

    if (g_list_find(prv->ncu_list, child)) {
        nwamui_warning("Found existing '%s(0x%p)' for '%s'", nwamui_object_get_name(child), child, nwamui_object_get_name(object));
        g_signal_stop_emission_by_name(object, "add");
        return;
    }

    g_object_freeze_notify(G_OBJECT(self));
    g_object_freeze_notify(G_OBJECT(prv->ncu_list_store));

    if ( nwamui_ncu_get_ncu_type(NWAMUI_NCU(child)) == NWAMUI_NCU_TYPE_WIRELESS ) {
        prv->wireless_link_num++;
        g_object_notify(G_OBJECT (self), "wireless_link_num" );
    }

    /* NCU isn't already in the list, so add it */
    prv->ncu_list = g_list_insert_sorted(prv->ncu_list, g_object_ref(child), (GCompareFunc)nwamui_object_sort_by_name);
    g_debug("Add '%s(0x%p)' to '%s'", nwamui_object_get_name(child), child, nwamui_object_get_name(object));

    gtk_list_store_append( prv->ncu_list_store, &iter );
    gtk_list_store_set( prv->ncu_list_store, &iter, 0, child, -1 );

    g_signal_connect(G_OBJECT(child), "notify",
                     (GCallback)ncu_notify_cb, (gpointer)self);


    g_object_thaw_notify(G_OBJECT(prv->ncu_list_store));
    g_object_thaw_notify(G_OBJECT(self));
}

static gboolean
nwamui_object_real_validate(NwamuiObject *object, gchar **prop_name_ret)
{
    return TRUE;
}

static gboolean
nwamui_object_real_commit( NwamuiObject *object )
{
    NwamuiNcpPrivate *prv      = NWAMUI_NCP_GET_PRIVATE(object);
    gboolean          rval     = TRUE;
    nwam_error_t      nerr;

    g_return_val_if_fail (NWAMUI_IS_NCP(object), FALSE);
    /* NCP doesn't have a commit function, it will commit once it is created or
     * copied.
     */
    if (prv->nwam_ncp == NULL) {
        /* This is a new added NCP */
        nerr = nwam_ncp_create (prv->name, NULL, &prv->nwam_ncp);
        if (nerr == NWAM_SUCCESS) {
        } else {
            g_warning("nwamui_ncp_create error creating nwam_ncp_handle %s", prv->name);
            return FALSE;
        }
        prv->nwam_ncp_modified = FALSE;
    }

    if (nwamui_object_real_is_modifiable(object)) {
        GList* ncu_item;

        for(ncu_item = g_list_first(prv->ncu_list);
            ncu_item;
            ncu_item = g_list_next(ncu_item)) {
            NwamuiObject *ncu = NWAMUI_OBJECT(ncu_item->data);

            if (nwamui_object_has_modifications(ncu) && !nwamui_object_commit(ncu)) {
                nwamui_debug("Commit FAILED for %s : %s", prv->name, nwamui_object_get_name(object));
                rval = FALSE;
                break;
            }
        }

        /* I assume commit will cause REFRESH event, so comment out this line. */
        /* nwamui_object_real_reload(object); */
    } else {
        nwamui_debug("NCP : %s is not modifiable", prv->name );
    }
    prv->nwam_ncp_modified = FALSE;

    return rval;
}

static gboolean
nwamui_object_real_destroy( NwamuiObject *object )
{
    NwamuiNcpPrivate *prv  = NWAMUI_NCP_GET_PRIVATE(object);
    nwam_error_t    nerr;

    g_assert(NWAMUI_IS_NCP(object));

    if (prv->nwam_ncp != NULL) {

        if ((nerr = nwam_ncp_destroy(prv->nwam_ncp, 0)) != NWAM_SUCCESS) {
            g_warning("Failed when destroying NCP for %s", prv->name);
            return( FALSE );
        }
        prv->nwam_ncp = NULL;
    }

    return( TRUE );
}

/* After discussion with Alan, it makes sense that we only show devices that
 * only have a physical presence on the system - on the basis that to configure
 * anything else would have no effect. 
 *
 * Tunnels are the only possible exception, but until it is implemented by
 * Seb where tunnels have a physical link id, then this will omit them too.
 */
static gboolean
device_exists_on_system( gchar* device_name )
{
    dladm_handle_t              handle;
    datalink_id_t               linkid;
    uint32_t                    flags = 0;
    gboolean                    rval = FALSE;

    if ( device_name != NULL ) {
        if ( dladm_open( &handle ) == DLADM_STATUS_OK ) {
            /* Interfaces that exist have a mapping, but also the OPT_ACTIVE
             * flag set, this could be unset if the device was removed from
             * the system (e.g. USB / PCMCIA)
             */
            if ( dladm_name2info( handle, device_name, &linkid, &flags, NULL, NULL ) == DLADM_STATUS_OK &&
                 (flags & DLADM_OPT_ACTIVE) ) {
                rval = TRUE;
            }
            else {
                g_debug("Unable to map device '%s' to linkid", device_name );
            }

            dladm_close( handle );
        }
    }

    return( rval );
}

static nwam_ncu_type_t
get_nwam_ncu_type( nwam_ncu_handle_t ncu )
{
    nwam_error_t        nerr;
    nwam_value_type_t   nwam_type;
    nwam_value_t        nwam_data;
    uint64_t            value = 0;
    nwam_ncu_type_t     rval = NWAM_NCU_TYPE_LINK;

    if ( ncu == NULL ) {
        return( value );
    }

    if ( (nerr = nwam_ncu_get_prop_type( NWAM_NCU_PROP_TYPE, &nwam_type ) ) != NWAM_SUCCESS 
         || nwam_type != NWAM_VALUE_TYPE_UINT64 ) {
        g_warning("Unexpected type for ncu property %s - got %d\n", NWAM_NCU_PROP_TYPE, nwam_type );
        return rval;
    }

    if ( (nerr = nwam_ncu_get_prop_value (ncu, NWAM_NCU_PROP_TYPE, &nwam_data)) != NWAM_SUCCESS ) {
        g_debug("No value for ncu property %s, error = %s", NWAM_NCU_PROP_TYPE, nwam_strerror( nerr ) );
        return rval;
    }

    if ( (nerr = nwam_value_get_uint64(nwam_data, &value )) != NWAM_SUCCESS ) {
        g_debug("Unable to get uint64 value for ncu property %s, error = %s", NWAM_NCU_PROP_TYPE, nwam_strerror( nerr ) );
        return rval;
    }

    nwam_value_free(nwam_data);

    rval = (nwam_ncu_type_t)value;

    return( rval );
}

static int
nwam_ncu_walker_cb (nwam_ncu_handle_t ncu, void *data)
{
    char*               name;
    nwam_error_t        nerr;
    NwamuiObject*       new_ncu;
    GtkTreeIter         iter;
    NwamuiNcp*          ncp = NWAMUI_NCP(data);
    NwamuiNcpPrivate*   prv = ncp->prv;
    nwam_ncu_type_t     nwam_ncu_type;

    if ((nerr = nwam_ncu_get_name(ncu, &name)) != NWAM_SUCCESS) {
        g_warning("Failed to get name for ncu, error: %s", nwam_strerror (nerr));
        return 0;
    }

    /* This function (NCU walker cb) will be called multiple time for different
     * NCU types. e.g. phys, ip, iptun...
     */

    if (!device_exists_on_system(name)) {
        /* Skip device that don't have a physical equivalent */
        free(name);
        return 0;
    }

    if(name) {
        if ((new_ncu = nwamui_ncp_get_ncu_by_device_name(ncp, name)) != NULL) {
            /* Reload */
            /* nwamui_object_set_handle(new_ncu, ncu); */
            nwamui_object_reload(NWAMUI_OBJECT(new_ncu));
            /* Found it, so remove from temp list of ones to be removed */
            prv->temp_list = g_list_remove(prv->temp_list, new_ncu);
        } else {
            new_ncu = nwamui_ncu_new_with_handle(NWAMUI_NCP(ncp), ncu);
            nwamui_object_add(NWAMUI_OBJECT(ncp), NWAMUI_OBJECT(new_ncu));
        }
        free(name);
    }

    /* Only count if it's a LINK class (to avoid double count) */
    nwam_ncu_type = get_nwam_ncu_type(ncu);
    if (nwam_ncu_type == NWAM_NCU_TYPE_LINK && new_ncu
      && nwamui_ncu_get_ncu_type(NWAMUI_NCU(new_ncu)) == NWAMUI_NCU_TYPE_WIRELESS ) {
        _num_wireless++;
    }

    if (new_ncu != NULL) {
        g_object_unref(new_ncu);
    }

    return 0;
}

static void
find_wireless_ncu( gpointer obj, gpointer user_data )
{
	NwamuiNcu           *ncu = NWAMUI_NCU(obj);
    GList              **glist_p = (GList**)user_data;

    if ( ncu == NULL || !NWAMUI_IS_NCU(ncu) ) {
        return;
    }

    if ( nwamui_ncu_get_ncu_type( ncu ) == NWAMUI_NCU_TYPE_WIRELESS ) {
        (*glist_p) = g_list_append( (*glist_p), (gpointer)g_object_ref(ncu) );
    }
}

extern GList*
nwamui_ncp_get_wireless_ncus( NwamuiNcp* self )
{
    GList*  ncu_list = NULL;

    g_return_val_if_fail( NWAMUI_IS_NCP(self), ncu_list );

    if ( self->prv->wireless_link_num > 0 ) {
        g_list_foreach( self->prv->ncu_list, find_wireless_ncu, &ncu_list );
    }

    return( ncu_list );
}

static gint
find_ncu_by_device_name(gconstpointer data, gconstpointer user_data)
{
    NwamuiNcu*  ncu             = NWAMUI_NCU(data);
    gchar      *ncu_device_name = nwamui_ncu_get_device_name( ncu );
    gchar      *device_name     = (gchar *)user_data;
    gint        found           = 1;

    g_return_val_if_fail(NWAMUI_IS_NCU(data), 1);
    g_return_val_if_fail(user_data, 1);

    if (g_ascii_strcasecmp(ncu_device_name, device_name) == 0) {
        found = 0;
    } else {
        found = 1;
    }
    g_free(ncu_device_name);
    return found;
}

static gint
find_first_wifi_net(gconstpointer data, gconstpointer user_data)
{
	NwamuiNcu  *ncu          = (NwamuiNcu *) data;
    NwamuiNcu **wireless_ncu = (NwamuiNcu **) user_data;
	
    g_return_val_if_fail(NWAMUI_IS_NCU(ncu), 1);
    g_return_val_if_fail(user_data, 1);
    
	if (nwamui_ncu_get_ncu_type(ncu) == NWAMUI_NCU_TYPE_WIRELESS ) {
        *wireless_ncu = g_object_ref(ncu);
        return 0;
	}
    return 1;
}

extern NwamuiNcu*
nwamui_ncp_get_first_wireless_ncu(NwamuiNcp *self)
{
    NwamuiNcu *wireless_ncu = NULL;

    g_return_val_if_fail(NWAMUI_IS_NCP(self), NULL);

    if (self->prv->wireless_link_num > 0) {
        nwamui_ncp_find_ncu(self, find_first_wifi_net, &wireless_ncu);
    }

    return wireless_ncu;
}

extern gint
nwamui_ncp_get_wireless_link_num( NwamuiNcp* self )
{
    g_return_val_if_fail( NWAMUI_IS_NCP(self), 0);

    return self->prv->wireless_link_num;
}

static void
freeze_thaw( gpointer obj, gpointer data ) {
    if ( obj ) {
        if ( (gboolean) data ) {
            g_object_freeze_notify(G_OBJECT(obj));
        }
        else {
            g_object_thaw_notify(G_OBJECT(obj));
        }
    }
}

extern void
nwamui_ncp_freeze_notify_ncus( NwamuiNcp* self )
{
    nwamui_ncp_foreach_ncu( self, (GFunc)freeze_thaw, (gpointer)TRUE );
}

extern void
nwamui_ncp_thaw_notify_ncus( NwamuiNcp* self )
{
    nwamui_ncp_foreach_ncu( self, (GFunc)freeze_thaw, (gpointer)FALSE );
}

/* Callbacks */

static void
ncu_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data)
{
    NwamuiNcp* self = NWAMUI_NCP(data);
    GtkTreeIter     iter;
    gboolean        valid_iter = FALSE;

    for (valid_iter = gtk_tree_model_get_iter_first( GTK_TREE_MODEL(self->prv->ncu_list_store), &iter);
         valid_iter;
         valid_iter = gtk_tree_model_iter_next( GTK_TREE_MODEL(self->prv->ncu_list_store), &iter)) {
        NwamuiNcu      *ncu;

        gtk_tree_model_get( GTK_TREE_MODEL(self->prv->ncu_list_store), &iter, 0, &ncu, -1);

        if ( (gpointer)ncu == (gpointer)gobject ) {
            GtkTreePath *path;

            valid_iter = FALSE;

            path = gtk_tree_model_get_path(GTK_TREE_MODEL(self->prv->ncu_list_store),
              &iter);
            gtk_tree_model_row_changed(GTK_TREE_MODEL(self->prv->ncu_list_store),
              path,
              &iter);
            gtk_tree_path_free(path);
        }
        if ( ncu )
            g_object_unref(ncu);
    }
}

static void
row_deleted_cb (GtkTreeModel *model, GtkTreePath *path, gpointer user_data) 
{
    NwamuiNcp     *ncp = NWAMUI_NCP(user_data);

    if ( model == GTK_TREE_MODEL(ncp->prv->ncu_list_store )) {
        g_debug("NCU Removed from List, but not propagated.");
    }
}

static void
row_inserted_cb(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer user_data)
{
    NwamuiNcp     *ncp = NWAMUI_NCP(user_data);

    if ( model == GTK_TREE_MODEL(ncp->prv->ncu_list_store )) {
        g_debug("NCU Inserted in List, but not propagated.");
    }
}

static void
rows_reordered_cb(GtkTreeModel *tree_model, GtkTreePath *path, GtkTreeIter *iter, gpointer arg3, gpointer user_data)   
{
    NwamuiNcp     *ncp = NWAMUI_NCP(user_data);
    gchar         *path_str = gtk_tree_path_to_string(path);

    g_debug("NwamuiNcp: NCU List: Rows Reordered: %s", path_str?path_str:"NULL");

    if ( tree_model == GTK_TREE_MODEL(ncp->prv->ncu_list_store )) {
        g_debug("NCU Re-ordered in List, but not propagated.");
    }
}

