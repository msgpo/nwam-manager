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
 */
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <libgnomeui/libgnomeui.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#include "status_icon.h"
#include "notify.h"
#include "libnwamui.h"
#include "nwam-menu.h"

static GtkWidget *my_menu = NULL;
static GtkStatusIcon *status_icon = NULL;

#define NWAM_MANAGER_LOCK	"nwam-manger-lock"

gboolean
detect_and_set_singleton ()
{
    enum { NORMAL_SESSION = 1, SUNRAY_SESSION };
    static gint session_type = 0;
    gchar *lockfile = NULL;
    int fd;
    
    if (session_type == 0) {
        const gchar *sunray_token;
        g_assert (lockfile == NULL);
        if ((sunray_token = g_getenv ("SUN_SUNRAY_TOKEN")) == NULL) {
            session_type = NORMAL_SESSION;
        } else {
            g_debug ("$SUN_SUNRAY_TOKEN = %s\n", sunray_token);
            session_type = SUNRAY_SESSION;
        }
    }
    
    switch (session_type) {
    case NORMAL_SESSION:
        lockfile = g_build_filename ("/tmp", NWAM_MANAGER_LOCK, NULL);
        break;
    case SUNRAY_SESSION:
        lockfile = g_build_filename ("/tmp", g_get_user_name (),
          NWAM_MANAGER_LOCK, NULL);
        break;
    default:
        g_debug ("Unknown session type.");
        exit (1);
    }

    fd = open (lockfile, O_CREAT | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        g_debug ("open lock file %s error", lockfile);
        exit (1);
    } else {
        struct flock pidlock;
        pidlock.l_type = F_WRLCK;
        pidlock.l_whence = SEEK_SET;
        pidlock.l_start = 0;
        pidlock.l_len = 0;
        if (fcntl (fd, F_SETLK, &pidlock) == -1) {
            if (errno == EAGAIN || errno == EACCES) {
                g_free (lockfile);
                return FALSE;
            } else {
                g_debug ("obtain lock file %s error", lockfile);
                exit (1);
            }
        }
    }
    g_free (lockfile);
    return TRUE;
}

static void 
cleanup_and_exit(int sig, siginfo_t *sip, void *data)
{
    gtk_main_quit ();
}

static void
on_blink_change(GtkStatusIcon *widget, 
        gpointer data)
{
    gboolean blink = GPOINTER_TO_UINT(data);
    g_debug("Set blinking %s", (blink) ? "on" : "off");
    gtk_status_icon_set_blinking(GTK_STATUS_ICON(status_icon), blink);
}

static gboolean
get_state(  GtkTreeModel *model,
            GtkTreePath *path,
            GtkTreeIter *iter,
            gpointer data)
{
	GString *str = (GString *)data;
	NwamuiNcu *ncu = NULL;
	gchar *format = NULL;
	gchar *name = NULL;
	
  	gtk_tree_model_get(model, iter, 0, &ncu, -1);

    g_assert( NWAMUI_IS_NCU(ncu) );
    
	switch (nwamui_ncu_get_ncu_type (ncu)) {
		case NWAMUI_NCU_TYPE_WIRED:
			format = _("\nWired (%s): %s");
			break;
		case NWAMUI_NCU_TYPE_WIRELESS:
			format = _("\nWireless (%s): %s");
			break;
	}
	name = nwamui_ncu_get_device_name (ncu);
	g_string_append_printf (str, format, name, "test again");
    g_free (name);
    
    g_object_unref( ncu );
    
    return( TRUE );
}

static void
on_trigger_network_changed (NwamuiDaemon* daemon, NwamuiEnv* env, gint sicon_id)
{
	GString *str;
	gchar *env_name = NULL;
	NwamuiNcp *ncp = NULL;
	
	env_name = nwamui_env_get_name (env);
	str = g_string_new (_("Environment: "));
	g_string_append_printf (str, "%s", env_name);
	g_free (env_name);
	
	ncp = nwamui_daemon_get_active_ncp (daemon);
	nwamui_ncp_foreach_ncu (ncp, get_state, (gpointer)str);
	nwam_status_icon_set_tooltip (sicon_id, str->str);
	g_string_free (str, TRUE);
}

static void
activate_cb ()
{
    /*
    nwam_notification_show_message("This is the summary", "This is the body...\nPara1", NULL );
    */
    nwam_exec ("-e");
}

int main( int argc, 
      char* argv[] )
{
    GnomeProgram *program;
    GnomeClient *client;
    gint    status_icon_index = 0;
    NwamMenu *menu;
    NwamuiDaemon* daemon;
    struct sigaction act;
    
#ifdef ENABLE_NLS
    bindtextdomain (GETTEXT_PACKAGE, NWAM_MANAGER_LOCALEDIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);
#endif

    program = gnome_program_init (PACKAGE, VERSION, LIBGNOMEUI_MODULE,
                                  argc, argv,
                                  GNOME_PARAM_APP_DATADIR,
                                  NWAM_MANAGER_DATADIR,
                                  NULL);

    act.sa_sigaction = cleanup_and_exit;
    sigemptyset (&act.sa_mask);
    act.sa_flags = SA_SIGINFO;
    sigaction (SIGKILL, &act, NULL);
    sigaction (SIGINT, &act, NULL);

    if (!detect_and_set_singleton ()) {
        g_debug ("Another process is running.\n");
        exit (0);
    }

#ifndef DEBUG
    client = gnome_master_client ();
    gnome_client_set_restart_command (client, argc, argv);
    gnome_client_set_restart_style (client, GNOME_RESTART_IMMEDIATELY);
#endif

    gtk_init( &argc, &argv );

    daemon = nwamui_daemon_get_instance ();
    status_icon_index = nwam_status_icon_create();
    menu = get_nwam_menu_instance();
    
    nwam_notification_init( nwam_status_icon_get_widget(status_icon_index) );
    nwam_notification_connect (daemon);

    nwam_status_icon_set_activate_callback( status_icon_index, G_CALLBACK(activate_cb) );

    nwam_status_icon_show( status_icon_index );
    g_signal_connect(daemon, "active_env_changed",
	    G_CALLBACK(on_trigger_network_changed),
	    (gpointer)status_icon_index);
    
    /* first time app trigger tooltip */
    on_trigger_network_changed (daemon, nwamui_daemon_get_active_env (daemon), status_icon_index);
    gtk_main();

    g_debug ("exiting...\n");
    nwam_notification_cleanup();
    g_object_unref (daemon);
    g_object_unref (G_OBJECT (menu));
    g_object_unref (G_OBJECT (program));
    
    return 0;
}
