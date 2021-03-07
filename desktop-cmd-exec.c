/*

desktop-cmd-exec.c - Desktop Command Execution Widget

Copyright (c) 2010 cpscotti (Clovis Peruchi Scotti)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

[cpscotti, roboscotti, scotti@ieee.org]

Sincere thanks to No!No!No!Yes!@talk.maemo.org for support for multiple lines
For more info:
http://talk.maemo.org/showthread.php?t=39177

*/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "desktop-cmd-exec.h"
#include <hildon/hildon.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <libintl.h>
#include <stdlib.h>

#include <conic.h>

#define HOME_DIR g_get_home_dir()

#define _(String) dgettext("hildon-libs", String)

#define DESKTOP_CMD_EXEC_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE (obj, DESKTOP_CMD_EXEC_TYPE, DesktopCmdExecPrivate))

#define DESKTOP_CMD_EXEC_SETTINGS_FILE "/.desktop_cmd_exec"
#define DESKTOP_CMD_EXEC_SETTINGS_VERSION 0.7

//main settings window
#define NON_GTK_RESPONSE_ADD_CMD 10
#define NON_GTK_RESPONSE_EDIT_CMD 20
#define NON_GTK_RESPONSE_GET_CMDS 30
//save is GTK_RESPONSE_OK

//Add/Edit/Delete window
#define NON_GTK_RESPONSE_DELETE_CMD 30
//save is GTK_RESPONSE_OK

#define NETWORK__UNRELATED 0
#define NETWORK__ONLY_CONNECTED 1
#define NETWORK__ONLY_DISCONNECTED 2


#define SIZE_WIDTH_ALL 800
#define SIZE_WIDTH_MIN_RATIO 0.118

#define SIZE_HEIGHT_LINE 31
#define SIZE_HEIGHT_RATIO_DEFAULT 1.29

void desktop_cmd_exec_write_settings (DesktopCmdExec *self, gboolean newAll, gboolean newInstance);
gboolean desktop_cmd_exec_update_content (DesktopCmdExec *self);

//Dialogs prototypes:
void desktop_cmd_exec_edit_add_dialog ( DesktopCmdExec *self, gboolean new, gint curr);
void desktop_cmd_exec_settings (HDHomePluginItem *hitem, DesktopCmdExec *self);

//Auxiliary functions' prototypes:
void AddCommand(DesktopCmdExec *self, gchar * s_title, gchar * s_command);
void EditCommand(DesktopCmdExec *self, gchar * s_title, gchar * s_command, int index);
void DelCommand(DesktopCmdExec *self, int index);
guint GetSeconds(guint index);

struct _DesktopCmdExecPrivate
{

	GtkWidget *homeWidget;
	GtkWidget *event;
	GtkWidget *contents;
	
	GtkWidget *cmdTitle_lb;
	GtkWidget *cmdResult_lb;

	gboolean isPressed;

	/* widget's instance identification str */
	gchar * widgetID;

	//config data

	//global data
		gchar ** c_titles;
		gchar ** c_commands;
		guint c_size;

	//instance data
		gboolean updOnClick;
		gboolean updOnDesktop;
		gboolean updOnBoot;

		gboolean flagBooted;
		
		guint updNeworkPolicy;
		gdouble widthRatio;
		gdouble heightRatio;//AP

		gboolean displayTitle;
		
		gchar * instanceTitle;
		gchar * instanceCmd;

		//Update on timer data
		guint delaySeconds;
		guint delayIndex;//index of delaySeconds in the array defined inside "desktop_cmd_exec_settings"

		//timer process id; used to stop when closing app or changing delay
		guint updateTimerID;

		//network connection query ptr
		ConIcConnection *connection;
		gboolean isConnected;
};

HD_DEFINE_PLUGIN_MODULE (DesktopCmdExec, desktop_cmd_exec, HD_TYPE_HOME_PLUGIN_ITEM);


//Settings file functions

void desktop_cmd_exec_read_settings ( DesktopCmdExec *self )
{
	gchar *filename;
	gboolean fileExists;
	GKeyFile *keyFile;

	gboolean noSettingsFlag;
	gboolean noInstanceKeysFlag;
	
	noSettingsFlag = FALSE;
	noInstanceKeysFlag = FALSE;

	if(self->priv->widgetID == NULL)
	{
		g_warning("Widget instance not initialized, not reading settings..");
		return;
	}
	

	keyFile = g_key_file_new();
	filename = g_strconcat (HOME_DIR, DESKTOP_CMD_EXEC_SETTINGS_FILE, NULL);
	fileExists = g_key_file_load_from_file (keyFile, filename, G_KEY_FILE_KEEP_COMMENTS, NULL);

	
	if (fileExists) {
		GError *error=NULL;

		gdouble settingsVer = g_key_file_get_double (keyFile, "config", "version", &error);
		if (error || settingsVer != DESKTOP_CMD_EXEC_SETTINGS_VERSION) {
			noSettingsFlag = TRUE;
			noInstanceKeysFlag = TRUE;
			g_error_free (error);
			error = NULL;
			g_warning("Settings file missing or incompatible version");
		}
		else
		{

			/*gets "global" data/commands list */
			//BEGIN get list
			g_strfreev(self->priv->c_commands);
			self->priv->c_commands = (gchar **) g_key_file_get_string_list (keyFile, "config", "c_commands", &(self->priv->c_size) ,&error);
			if (error) {
				noSettingsFlag = TRUE;
				g_error_free (error);
				error = NULL;
			}

			guint consistencyCheck = -1;
			g_strfreev(self->priv->c_titles);
			self->priv->c_titles = g_key_file_get_string_list (keyFile, "config", "c_titles", &consistencyCheck ,&error);
			if (error) {
				noSettingsFlag = TRUE;
				g_error_free (error);
				error = NULL;
			}

			if(consistencyCheck != self->priv->c_size)
			{
				g_warning("Settings file corrupted!");
				noSettingsFlag = TRUE;
			}
			//END


			/*getting instance specific data*/
			//BEGIN
			self->priv->widthRatio = g_key_file_get_double (keyFile, self->priv->widgetID, "widthRatio", &error);
			if (error) {
				//no value? assume std
				noInstanceKeysFlag = TRUE;
				self->priv->widthRatio = 0.4;

				g_warning("no widthRatio on key file, assuming 0.4");

				g_error_free (error);
				error = NULL;
			}

/* AP; No!No!No!Yes!, thanks! */
			self->priv->heightRatio = g_key_file_get_double (keyFile, self->priv->widgetID, "heightRatio", &error);
			if (error) {
				//no value? assume std
				noInstanceKeysFlag = TRUE;
				self->priv->heightRatio = SIZE_HEIGHT_RATIO_DEFAULT;

				g_warning("no heightRatio on key file, assuming SIZE_HEIGHT_RATIO_DEFAULT");
				g_error_free (error);
				error = NULL;
			}

			g_free(self->priv->instanceTitle);
			self->priv->instanceTitle = g_key_file_get_string (keyFile, self->priv->widgetID, "instanceTitle", &error);
			if (error) {
				//no value? what now? assume one "fake" for now
				noInstanceKeysFlag = TRUE;
				self->priv->instanceTitle = NULL;

				g_warning("no title found");

				g_error_free (error);
				error = NULL;
			}

			g_free(self->priv->instanceCmd);
			self->priv->instanceCmd = g_key_file_get_string (keyFile, self->priv->widgetID, "instanceCmd", &error);
			if (error) {
				//no value? what now? assume one "fake" for now
				noInstanceKeysFlag = TRUE;
				self->priv->instanceCmd = NULL;

				g_warning("no command found");

				g_error_free (error);
				error = NULL;
			}

			self->priv->displayTitle = g_key_file_get_boolean (keyFile, self->priv->widgetID, "displayTitle", &error);
			if (error) {
				self->priv->displayTitle = TRUE;
				g_error_free (error);
				error = NULL;
			}

			//BEGIN update policy
			self->priv->updOnClick = g_key_file_get_boolean (keyFile, self->priv->widgetID, "updOnClick", &error);
			if (error) {

				noInstanceKeysFlag = TRUE;
				g_error_free (error);
				error = NULL;
			}

			self->priv->updOnDesktop = g_key_file_get_boolean (keyFile, self->priv->widgetID, "updOnDesktop", &error);
			if (error) {

				noInstanceKeysFlag = FALSE;
				g_error_free (error);
				error = NULL;
			}

			self->priv->updOnBoot = g_key_file_get_boolean (keyFile, self->priv->widgetID, "updOnBoot", &error);
			if (error) {

				noInstanceKeysFlag = FALSE;
				g_error_free (error);
				error = NULL;
			}

			self->priv->delayIndex = (guint) g_key_file_get_integer (keyFile, self->priv->widgetID, "delayIndex", &error);
			if (error) {

				noInstanceKeysFlag = TRUE;
				g_error_free (error);
				error = NULL;
			}
			else
				self->priv->delaySeconds = GetSeconds(self->priv->delayIndex);

			self->priv->updNeworkPolicy = (guint) g_key_file_get_integer (keyFile, self->priv->widgetID, "updNeworkPolicy", &error);
			if (error || self->priv->updNeworkPolicy < 0 || self->priv->updNeworkPolicy > 2) {
				//Not important; in order not to break with older key-file versions, just assume NETWORK__UNRELATED and move on..
				self->priv->updNeworkPolicy = NETWORK__UNRELATED;
				noInstanceKeysFlag = TRUE;

				g_error_free (error);
				error = NULL;
			}

			//END
		}
		
		//END
		g_key_file_free (keyFile);
		g_free (filename);
	}
	else
		noSettingsFlag = TRUE;


	if(noInstanceKeysFlag)
	{
		self->priv->updOnClick = TRUE;
		self->priv->updOnDesktop = TRUE;
		self->priv->updOnBoot = TRUE;
		self->priv->updNeworkPolicy = NETWORK__UNRELATED;
		self->priv->delayIndex = 0;
		self->priv->delaySeconds = 0;
		self->priv->displayTitle = TRUE;

		self->priv->widthRatio = 0.4;
		self->priv->heightRatio = SIZE_HEIGHT_RATIO_DEFAULT;

		self->priv->instanceTitle = NULL;
		self->priv->instanceCmd = NULL;
	}

	if(noSettingsFlag)
	{
		self->priv->c_commands = NULL;
		self->priv->c_titles = NULL;
		self->priv->c_size = 0;
	}

	if(noSettingsFlag || noInstanceKeysFlag)
	{
		g_warning ("Problems loading settings. Resetting all");
		desktop_cmd_exec_write_settings(self, noSettingsFlag, noInstanceKeysFlag);
	}
// 	else
// 		g_warning("Settings file loaded successfully");
}

void desktop_cmd_exec_write_settings (DesktopCmdExec *self, gboolean newAll, gboolean newInstance)
{
//  	g_warning ("desktop_cmd_exec_write_settings");
	GKeyFile *keyFile;
	gboolean fileExists;

	gchar *fileData;
	FILE *iniFile;
	gsize size;
	gchar *filename;

	if(self->priv->widgetID == NULL)
	{
		g_warning("Widget instance not initialized, not writing settings..");
		return;
	}
	
	keyFile = g_key_file_new();

	filename = g_strconcat (HOME_DIR, DESKTOP_CMD_EXEC_SETTINGS_FILE, NULL);
	fileExists = g_key_file_load_from_file (keyFile, filename, G_KEY_FILE_KEEP_COMMENTS, NULL);
	
// 	if (fileExists) {
// 	}

	if(newAll)
	{
//  		g_warning("records empty, filling with pre-loaded vals");
		//records empty, filling with pre-loaded vals
		self->priv->c_size = 9;
		gchar * p_titles[] = { "Uptime:", "Battery(%):", "Battery(mAh):", "Boot Reason:", "Boot Count:", "External IP:", "Internal IP:", "Rootfs(%):", "Free Rootfs:", NULL};
		gchar * p_commands[] = {
		"uptime|cut -d\" \" -f4-|sed 's/\\, *load.*//'",
		"hal-device bme | awk -F\"[. ]\" '$5 == \"is_charging\" {chrg = $7}; $5 == \"percentage\" {perc = $7} END if (chrg == \"false\") {print perc \"%\"} else {print \"Chrg\"}'",
		"hal-device bme | grep battery.reporting | awk -F. '{print $3}' | sort | awk '$1 == \"current\" { current = $3}; $1== \"design\" {print current \"/\" $3}'",
		"cat /proc/bootreason",
		"cat /var/lib/dsme/boot_count",
		"wget --timeout=10 -q -O - api.myiptest.com | awk -F \"\\\"\" '{print $4}'",
		"/sbin/ifconfig | grep \"inet addr\" | awk -F: '{print $2}' | awk '{print $1}'",
		"df | awk '$1 == \"rootfs\" {print $5}'",
		"df -h | awk ' $1 == \"rootfs\" {print $4\"B\"}'", NULL};

		//clean possible oldies
		g_strfreev(self->priv->c_titles);
		self->priv->c_titles = NULL;
		g_strfreev(self->priv->c_commands);
		self->priv->c_commands = NULL;
		//assign new from stack values
		self->priv->c_titles = g_strdupv(p_titles);
		self->priv->c_commands = g_strdupv(p_commands);
	}

	if(newInstance)
	{
		g_free(self->priv->instanceTitle);
		g_free(self->priv->instanceCmd);
		
		if((self->priv->c_titles != NULL)
			&& (self->priv->c_commands != NULL)
			&& (self->priv->c_size > 0))
		{
			self->priv->instanceTitle = g_strdup(self->priv->c_titles[0]);
			self->priv->instanceCmd = g_strdup(self->priv->c_commands[0]);
		}
		
		self->priv->widthRatio = 0.4;
		self->priv->heightRatio = SIZE_HEIGHT_RATIO_DEFAULT;
		self->priv->displayTitle = TRUE;
		self->priv->updOnClick = TRUE;
		self->priv->updOnDesktop = TRUE;
		self->priv->updOnBoot = TRUE;
		self->priv->updNeworkPolicy = NETWORK__UNRELATED;
		self->priv->delayIndex = 0;
	}

	g_key_file_set_double (keyFile, "config", "version", DESKTOP_CMD_EXEC_SETTINGS_VERSION);

	if(self->priv->c_titles != NULL && self->priv->c_commands != NULL)
	{
		g_key_file_set_string_list(keyFile, "config", "c_titles", (const gchar **)(self->priv->c_titles),self->priv->c_size);
		g_key_file_set_string_list(keyFile, "config", "c_commands", (const gchar **)(self->priv->c_commands),self->priv->c_size);
	}

	g_key_file_set_double (keyFile, self->priv->widgetID, "widthRatio", self->priv->widthRatio);
	g_key_file_set_double (keyFile, self->priv->widgetID, "heightRatio", self->priv->heightRatio);

	if(self->priv->instanceTitle != NULL && self->priv->instanceCmd != NULL)
	{
		g_key_file_set_string (keyFile, self->priv->widgetID, "instanceTitle", self->priv->instanceTitle);
		g_key_file_set_string (keyFile, self->priv->widgetID, "instanceCmd", self->priv->instanceCmd);
	}

	g_key_file_set_boolean (keyFile, self->priv->widgetID, "displayTitle", self->priv->displayTitle);
	g_key_file_set_boolean (keyFile, self->priv->widgetID, "updOnClick", self->priv->updOnClick);
	g_key_file_set_boolean (keyFile, self->priv->widgetID, "updOnDesktop", self->priv->updOnDesktop);
	g_key_file_set_boolean (keyFile, self->priv->widgetID, "updOnBoot", self->priv->updOnBoot);
	g_key_file_set_integer (keyFile, self->priv->widgetID, "delayIndex", self->priv->delayIndex);
	g_key_file_set_integer (keyFile, self->priv->widgetID, "updNeworkPolicy", self->priv->updNeworkPolicy);
	
	filename = g_strconcat (HOME_DIR, DESKTOP_CMD_EXEC_SETTINGS_FILE, NULL);
	fileData = g_key_file_to_data (keyFile, &size, NULL);
	iniFile = fopen (filename, "w");
	fputs (fileData, iniFile);
	fclose (iniFile);
	g_key_file_free (keyFile);
	g_free (fileData);
	g_free (filename);

	if(newAll || newInstance)
	{
		g_strfreev(self->priv->c_titles);
		self->priv->c_titles = NULL;
		g_strfreev(self->priv->c_commands);
		self->priv->c_commands = NULL;
		
		g_free(self->priv->instanceTitle);
		self->priv->instanceTitle = NULL;
		g_free(self->priv->instanceCmd);
		self->priv->instanceCmd = NULL;
		desktop_cmd_exec_read_settings(self);
	}
}



//widget/touchscreen interaction callbacks
void desktop_cmd_exec_button_press (GtkWidget *widget, GdkEventButton *event, DesktopCmdExec *self)
{
// 	g_warning ("desktop_cmd_exec_button_press");
	if (self->priv->updOnClick) {
		self->priv->isPressed = TRUE;
		gtk_widget_queue_draw (GTK_WIDGET (self));
	}

// 	if(self->priv->widgetID != NULL)
// 	{
// 		g_warning("Pressed on %s", self->priv->widgetID);
// 	}
}

void desktop_cmd_exec_button_release (GtkWidget *widget, GdkEventButton *event, DesktopCmdExec *self)
{
// 	g_warning ("desktop_cmd_exec_button_release");
	if (self->priv->updOnClick) {
		self->priv->isPressed = FALSE;
		gtk_widget_queue_draw (GTK_WIDGET (self));
		desktop_cmd_exec_update_content(self);
	}
}

void desktop_cmd_exec_leave_event (GtkWidget *widget, GdkEventCrossing *event, DesktopCmdExec *self)
{
// 	g_warning ("desktop_cmd_exec_leave_event");
	if (self->priv->updOnClick) {
		self->priv->isPressed = FALSE;
		gtk_widget_queue_draw (GTK_WIDGET (self));
	}
}

static void desktop_cmd_exec_check_desktop (GObject *gobject, GParamSpec *pspec, DesktopCmdExec *self)
{
	if (self->priv->updOnDesktop) {
		desktop_cmd_exec_update_content(self);
	}
	// 	g_warning ("desktop_cmd_exec_check_desktop");
}


gboolean desktop_cmd_exec_connection_event (ConIcConnection *connection, ConIcConnectionEvent *event, DesktopCmdExec *self)
{
	ConIcConnectionStatus status = con_ic_connection_event_get_status(event);
	if(status == CON_IC_STATUS_CONNECTED)
	{
		self->priv->isConnected = TRUE;
	}
	else
	{
		self->priv->isConnected = FALSE;
	}
	if(self->priv->updNeworkPolicy != NETWORK__UNRELATED)
		desktop_cmd_exec_update_content(self);
	return TRUE;
}


//Content/widget creation/update
void desktop_cmd_exec_content_create (DesktopCmdExec *self)
{
// 	g_warning ("desktop_cmd_exec_content_create");
	self->priv->contents = gtk_event_box_new ();
	gtk_event_box_set_visible_window (GTK_EVENT_BOX (self->priv->contents), FALSE);
	gtk_container_set_border_width (GTK_CONTAINER (self->priv->contents), 0);
// 	GtkSizeGroup *group = GTK_SIZE_GROUP (gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL));

	if(self->priv->displayTitle)
		self->priv->cmdTitle_lb = gtk_label_new ("title:");
	else
		self->priv->cmdTitle_lb = gtk_label_new ("");
	
	self->priv->cmdResult_lb = gtk_label_new ("result");

	//gtk_label_set_line_wrap (GTK_LABEL (self->priv->cmdResult_lb), TRUE);
	// wraps when unnecessary and doesn't wrap when necessary =/..
	/* some insight on this at: http://library.gnome.org/devel/gtk/2.15/GtkLabel.html#gtk-label-set-line-wrap */
	
	GtkWidget *box = gtk_hbox_new (FALSE, 0);
	
	gtk_box_pack_start (GTK_BOX (box), self->priv->cmdTitle_lb, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (box), self->priv->cmdResult_lb, TRUE, TRUE, 0);
	
	hildon_helper_set_logical_color (self->priv->cmdResult_lb, GTK_RC_FG, GTK_STATE_NORMAL, "ActiveTextColor");

	//Alignment!! (...., (left/right), (top/bottom))
	gtk_misc_set_alignment (GTK_MISC (self->priv->cmdTitle_lb), 0, 0);
	gtk_misc_set_alignment (GTK_MISC (self->priv->cmdResult_lb), 1, 1);

	gtk_misc_set_padding (GTK_MISC (self->priv->cmdTitle_lb), HILDON_MARGIN_DEFAULT, HILDON_MARGIN_HALF);
	gtk_misc_set_padding (GTK_MISC (self->priv->cmdResult_lb), HILDON_MARGIN_DEFAULT, HILDON_MARGIN_HALF);

	gtk_container_add (GTK_CONTAINER (self->priv->contents), box);
	gtk_box_pack_start (GTK_BOX (self->priv->homeWidget), self->priv->contents, FALSE, FALSE, 0);

	//widget drawing signal connections
// 	g_signal_connect (self->priv->contents, "button-release-event", G_CALLBACK (desktop_cmd_exec_button_release), self);
// 	g_signal_connect (self->priv->contents, "button-press-event", G_CALLBACK (desktop_cmd_exec_button_press), self);
// 	g_signal_connect (self->priv->contents, "leave-notify-event", G_CALLBACK (desktop_cmd_exec_leave_event), self);

	g_signal_connect (GTK_WIDGET (self), "button-release-event", G_CALLBACK (desktop_cmd_exec_button_release), self);
	g_signal_connect (GTK_WIDGET (self), "button-press-event", G_CALLBACK (desktop_cmd_exec_button_press), self);
	g_signal_connect (GTK_WIDGET (self), "leave-notify-event", G_CALLBACK (desktop_cmd_exec_leave_event), self);
	
	gtk_widget_show_all (self->priv->homeWidget);
	gtk_widget_show (self->priv->cmdTitle_lb);
}


gboolean desktop_cmd_exec_resize_hack(DesktopCmdExec *self)
{
// 	g_warning("HACK");
	if(self->priv->widgetID != NULL)
	{
		desktop_cmd_exec_read_settings (self);
		desktop_cmd_exec_update_content (self);

// 		gtk_widget_set_size_request (GTK_WIDGET (self), (int)(SIZE_WIDTH_ALL*self->priv->widthRatio), SIZE_HEIGHT_LINE);
// 		gtk_window_resize (GTK_WINDOW (self), (int)(SIZE_WIDTH_ALL*self->priv->widthRatio), SIZE_HEIGHT_LINE);

/* AP; No!No!No!Yes!, thanks! */
		gtk_widget_set_size_request (GTK_WIDGET (self), (int)(SIZE_WIDTH_ALL*self->priv->widthRatio), (int)(SIZE_HEIGHT_LINE*self->priv->heightRatio));
		gtk_window_resize (GTK_WINDOW (self), (int)(SIZE_WIDTH_ALL*self->priv->widthRatio), (int)(SIZE_HEIGHT_LINE*self->priv->heightRatio));
		
	}
// 	else
// 	{
// 		g_warning("HACK unsuccessful");
// 	}

	return FALSE;
}

gboolean desktop_cmd_exec_update_content (DesktopCmdExec *self)
{
// 	g_warning ("desktop_cmd_exec_update_content");

	if(self->priv->widgetID == NULL)
	{
		g_warning("Widget instance unknown... aborting");
		return;
	}

	if(self->priv->updNeworkPolicy == NETWORK__ONLY_CONNECTED && self->priv->isConnected == FALSE)
		return TRUE;
	
	if(self->priv->updNeworkPolicy == NETWORK__ONLY_DISCONNECTED && self->priv->isConnected == TRUE)
		return FALSE;
	
	FILE *fp;
	gchar line[2048];
	size_t l;//AP
	gchar *result;
	
	gboolean found = FALSE;
	
	if(self->priv->instanceCmd == NULL || self->priv->instanceTitle == NULL)
	{
		gtk_label_set_text (GTK_LABEL (self->priv->cmdTitle_lb), "Error:");
		gtk_label_set_text (GTK_LABEL (self->priv->cmdResult_lb), "No commands");
		return 0;
	}

	if(self->priv->displayTitle)
		gtk_label_set_text (GTK_LABEL (self->priv->cmdTitle_lb), self->priv->instanceTitle);
	else
		gtk_label_set_text (GTK_LABEL (self->priv->cmdTitle_lb), "");

	if(self->priv->flagBooted || self->priv->updOnBoot)
	{
		fp = popen (self->priv->instanceCmd, "r");
		
		/* AP; No!No!No!Yes!, thanks! */
		//	while (fgets (line, sizeof line, fp)) {//Needed to change this "line-based" management for output //AP
		if (l=fread (line, 1, sizeof line, fp)) {//to this "buffer-based" for multiline handling in widget //AP
			line[l-1]='\000';//AP
			
			gtk_label_set_text (GTK_LABEL (self->priv->cmdResult_lb), line);//AP
			found = TRUE;
		}
		pclose (fp);

		if (!found) {
			gtk_label_set_text (GTK_LABEL (self->priv->cmdResult_lb), "Invalid Command");
		}
	}
	else
	{
		self->priv->flagBooted = TRUE;
		gtk_label_set_text (GTK_LABEL (self->priv->cmdResult_lb), "");
	}

	if( (self->priv->updateTimerID == 0) && (self->priv->delaySeconds > 0))
	{
		self->priv->updateTimerID = g_timeout_add_seconds (self->priv->delaySeconds, (GSourceFunc)desktop_cmd_exec_update_content, self);
	}
	
	return found;
}




static void desktop_cmd_exec_dispose (GObject *object)
{
// 	g_warning ("desktop_cmd_exec_dispose");
// 	DesktopCmdExec *self = DESKTOP_CMD_EXEC (object);

	G_OBJECT_CLASS (desktop_cmd_exec_parent_class)->dispose (object);
}

static void desktop_cmd_exec_finalize (GObject *object)
{
// 	g_warning ("desktop_cmd_exec_finalize");
	DesktopCmdExec *self = DESKTOP_CMD_EXEC (object);

	if (self->priv->updateTimerID) {
		g_source_remove (self->priv->updateTimerID);
	}

	g_object_unref (self->priv->connection);

	g_strfreev(self->priv->c_titles);
	self->priv->c_titles = NULL;
	
	g_strfreev(self->priv->c_commands);
	self->priv->c_commands = NULL;

	
	g_free(self->priv->instanceTitle);
	self->priv->instanceTitle = NULL;
	g_free(self->priv->instanceCmd);
	self->priv->instanceCmd = NULL;

	G_OBJECT_CLASS (desktop_cmd_exec_parent_class)->finalize (object);
}

static void desktop_cmd_exec_realize (GtkWidget *widget)
{
// 	g_warning ("desktop_cmd_exec_realize");
	GdkScreen *screen;

	screen = gtk_widget_get_screen (widget);
	gtk_widget_set_colormap (widget, gdk_screen_get_rgba_colormap (screen));

	gtk_widget_set_app_paintable (widget, TRUE);

	GTK_WIDGET_CLASS (desktop_cmd_exec_parent_class)->realize (widget);

	DesktopCmdExec *self = DESKTOP_CMD_EXEC (widget);
	self->priv->widgetID = hd_home_plugin_item_get_applet_id (HD_HOME_PLUGIN_ITEM (widget));
}

static gboolean desktop_cmd_exec_expose_event (GtkWidget *widget, GdkEventExpose *event)
{
	DesktopCmdExec *self = DESKTOP_CMD_EXEC (widget);

	cairo_t *cr;
	
	cr = gdk_cairo_create(GDK_DRAWABLE (widget->window));

	GdkColor color;
	if (!self->priv->isPressed) {
		gtk_style_lookup_color (gtk_rc_get_style(widget), "DefaultBackgroundColor", &color);
		cairo_set_source_rgba (cr, color.red/65535.0, color.green/65335.0, color.blue/65535.0, 0.75);
	} else {
		gtk_style_lookup_color (gtk_rc_get_style(widget), "SelectionColor", &color);
		cairo_set_source_rgba (cr, color.red/65535.0, color.green/65335.0, color.blue/65535.0, 0.6);
	}
	
	gint width, height, x, y;
	gint radius = 5;
	width = widget->allocation.width;
	height = widget->allocation.height;
	x = widget->allocation.x;
	y = widget->allocation.y;

	cairo_move_to(cr, x + radius, y);
	cairo_line_to(cr, x + width - radius, y);
	cairo_curve_to(cr, x + width - radius, y, x + width, y, x + width,y + radius);
	cairo_line_to(cr, x + width, y + height - radius);
	cairo_curve_to(cr, x + width, y + height - radius, x + width,y + height, x + width - radius, y + height);
	cairo_line_to(cr, x + radius, y + height);
	cairo_curve_to(cr, x + radius, y + height, x, y + height, x,y + height - radius);
	cairo_line_to(cr, x, y + radius);
	cairo_curve_to(cr, x, y + radius, x, y, x + radius, y);

	cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);

	cairo_fill_preserve(cr);
	
	gtk_style_lookup_color (gtk_rc_get_style(widget), "ActiveTextColor", &color);
	cairo_set_source_rgba (cr, color.red/65535.0, color.green/65335.0, color.blue/65535.0, 0.5);
	cairo_set_line_width (cr, 1);
	cairo_stroke (cr);
	
	cairo_destroy(cr);


	return GTK_WIDGET_CLASS (desktop_cmd_exec_parent_class)->expose_event (widget, event);
}

static void desktop_cmd_exec_class_init (DesktopCmdExecClass *klass)
{
// 	g_warning ("desktop_cmd_exec_class_init");
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->dispose = desktop_cmd_exec_dispose;
	object_class->finalize = desktop_cmd_exec_finalize;
	
	widget_class->realize = desktop_cmd_exec_realize;
	widget_class->expose_event = desktop_cmd_exec_expose_event;

	g_type_class_add_private (klass, sizeof (DesktopCmdExecPrivate));
}

static void desktop_cmd_exec_class_finalize (DesktopCmdExecClass *klass G_GNUC_UNUSED)
{
}

static void desktop_cmd_exec_init (DesktopCmdExec *self)
{
// 	g_warning ("desktop_cmd_exec_init");
	self->priv = DESKTOP_CMD_EXEC_GET_PRIVATE (self);
	self->priv->updateTimerID = 0;
	self->priv->isPressed = FALSE;

	self->priv->widgetID = NULL;

	self->priv->flagBooted = FALSE;

	self->priv->connection = con_ic_connection_new ();
	g_signal_connect (self->priv->connection, "connection-event", G_CALLBACK (desktop_cmd_exec_connection_event), self);
	g_object_set (self->priv->connection, "automatic-connection-events", TRUE);
	self->priv->isConnected = FALSE;

	self->priv->cmdResult_lb = NULL;
	
	hd_home_plugin_item_set_settings (&self->parent, TRUE);
	g_signal_connect (&self->parent, "show-settings", G_CALLBACK (desktop_cmd_exec_settings), self);

	gtk_window_set_default_size (GTK_WINDOW (self), SIZE_WIDTH_ALL, SIZE_HEIGHT_LINE);
	
	self->priv->homeWidget = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (self), self->priv->homeWidget);
	gtk_widget_show (self->priv->homeWidget);
	
	GdkGeometry hints;

	hints.min_width = (int)(SIZE_WIDTH_ALL*0.05);
	hints.min_height = SIZE_HEIGHT_LINE;
	hints.max_width = (int)(SIZE_WIDTH_ALL);
	hints.max_height = SIZE_HEIGHT_LINE*10;

	gtk_window_set_geometry_hints (GTK_WINDOW (self), self->priv->homeWidget, &hints, GDK_HINT_MIN_SIZE | GDK_HINT_MAX_SIZE);

	gtk_widget_set_size_request (GTK_WIDGET (self), (int)(SIZE_WIDTH_ALL*SIZE_WIDTH_MIN_RATIO), SIZE_HEIGHT_LINE);
	gtk_window_resize (GTK_WINDOW (self), (int)(SIZE_WIDTH_ALL*SIZE_WIDTH_MIN_RATIO), SIZE_HEIGHT_LINE);

	g_timeout_add (500, (GSourceFunc)desktop_cmd_exec_resize_hack, self);

// 	gtk_widget_set_size_request (GTK_WIDGET (self), SIZE_WIDTH_ALL, SIZE_HEIGHT_LINE);
// 	gtk_window_resize (GTK_WINDOW (self), (int)(SIZE_WIDTH_ALL*self->priv->widthRatio), SIZE_HEIGHT_LINE);
// 	gtk_window_resize (GTK_WINDOW (self), SIZE_WIDTH_ALL, SIZE_HEIGHT_LINE);

	desktop_cmd_exec_content_create (self);
	
	g_signal_connect (self, "notify::is-on-current-desktop", G_CALLBACK (desktop_cmd_exec_check_desktop), self);
}

DesktopCmdExec* desktop_cmd_exec_new (void)
{
	return g_object_new (DESKTOP_CMD_EXEC_TYPE, NULL);
}


//Separate window dialogs
void desktop_cmd_exec_settings (HDHomePluginItem *hitem, DesktopCmdExec *self)
{
	// 	g_warning ("desktop_cmd_exec_settings");
	
	int settingsRunning = 1;
	while(settingsRunning)
	{
		GtkWidget *dialog = gtk_dialog_new_with_buttons ("Desktop Command Execution Widget Settings", NULL, 0,
																										 _("wdgt_bd_save"), GTK_RESPONSE_ACCEPT,
																										 "Add Cmd", NON_GTK_RESPONSE_ADD_CMD,
																										 "Edit Cmd", NON_GTK_RESPONSE_EDIT_CMD,
																										 "Scripts Wiki", NON_GTK_RESPONSE_GET_CMDS,
																										 NULL);

// 		GtkWidget *content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));


		GtkWidget *settingsArea = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
		GtkWidget *scroll = hildon_pannable_area_new ();
		g_object_set (scroll, "hscrollbar-policy", GTK_POLICY_NEVER, NULL);
		GtkWidget *content_area = gtk_vbox_new (FALSE, 0);
		hildon_pannable_area_add_with_viewport (HILDON_PANNABLE_AREA (scroll), content_area);
		gtk_container_add (GTK_CONTAINER (settingsArea), scroll);
		gtk_window_set_default_size (GTK_WINDOW (dialog), -1, 380);


		GtkSizeGroup *group = GTK_SIZE_GROUP (gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL));

		GtkWidget *cmdSelectionLabel = gtk_label_new ("");
		gtk_label_set_markup (GTK_LABEL (cmdSelectionLabel), "<small>Command Selection:</small>");
		gtk_container_add (GTK_CONTAINER (content_area), cmdSelectionLabel);

		desktop_cmd_exec_read_settings(self);
		
		//BEGIN Command Selector
		
		GtkWidget *selector = hildon_touch_selector_new_text ();
		int i;
		int cmdOnListMatch = -1;
		for(i=0;i<self->priv->c_size;i++)
		{
			hildon_touch_selector_append_text (HILDON_TOUCH_SELECTOR (selector), self->priv->c_titles[i]);
			if(g_strcmp0(self->priv->c_titles[i],self->priv->instanceTitle) == 0)
			{
// 				if(g_strcmp0(self->priv->c_commands[i],self->priv->instanceCmd) == 0)
// 				{
					hildon_touch_selector_set_active (HILDON_TOUCH_SELECTOR (selector), 0, i);
					cmdOnListMatch = i;
// 				}
			}
		}
		if(cmdOnListMatch < 0)
		{
			hildon_touch_selector_append_text (HILDON_TOUCH_SELECTOR (selector), "Not found, pick on list!");
			hildon_touch_selector_set_active (HILDON_TOUCH_SELECTOR (selector), 0, self->priv->c_size);
		}
		
		
		GtkWidget * cmdSelector = hildon_picker_button_new (HILDON_SIZE_AUTO_WIDTH | HILDON_SIZE_FINGER_HEIGHT, HILDON_BUTTON_ARRANGEMENT_HORIZONTAL);
		hildon_button_set_title (HILDON_BUTTON (cmdSelector), "Commands: ");
		hildon_button_add_title_size_group (HILDON_BUTTON (cmdSelector), group);
		hildon_button_set_alignment (HILDON_BUTTON (cmdSelector), 0, 0.5, 0, 0);
		hildon_picker_button_set_selector (HILDON_PICKER_BUTTON (cmdSelector), HILDON_TOUCH_SELECTOR (selector));
		gtk_container_add (GTK_CONTAINER (content_area), cmdSelector);
		//END

		GtkWidget *cmdHelpLabel = gtk_label_new ("");
		gtk_label_set_markup (GTK_LABEL (cmdHelpLabel), "<small>(Add multiple widgets to your desktop to use more than one command)</small>");
		gtk_misc_set_alignment (GTK_MISC (cmdHelpLabel), 0, 0);
		gtk_label_set_line_wrap (GTK_LABEL (cmdHelpLabel), TRUE);
		gtk_container_add (GTK_CONTAINER (content_area), cmdHelpLabel);

		//BEGIN
		//layout opts

		GtkWidget *widthBox = gtk_hbox_new (FALSE, 0);

		GtkWidget *widthLabel = gtk_label_new ("");
		gtk_label_set_markup (GTK_LABEL (widthLabel), "Width:\n<small>(relative to screen)</small>");
		gtk_misc_set_alignment (GTK_MISC (widthLabel), 0, 0.5);
		gtk_misc_set_padding (GTK_MISC (widthLabel), HILDON_MARGIN_DOUBLE, 0);
		gtk_size_group_add_widget (group, widthLabel);

		GtkWidget * widthSelect = gtk_hscale_new_with_range ((gdouble)0.05, (gdouble)1.0, (gdouble)0.05);
		gtk_range_set_value(GTK_RANGE(widthSelect), self->priv->widthRatio);
		
		gtk_box_pack_start (GTK_BOX (widthBox), widthLabel, FALSE, FALSE, 0);
		gtk_box_pack_start (GTK_BOX (widthBox), widthSelect, TRUE, TRUE, 0);

		gtk_container_add (GTK_CONTAINER (content_area), widthBox);

		//layout opts heightBox //AP
		GtkWidget *heightBox = gtk_hbox_new (FALSE, 0);//AP
		
		GtkWidget *heightLabel = gtk_label_new ("");//AP
		
		gtk_label_set_markup (GTK_LABEL (heightLabel), "Height:\n<small>(number of lines)</small>");
		
		gtk_misc_set_alignment (GTK_MISC (heightLabel), 0, 0.5);//AP
		gtk_misc_set_padding (GTK_MISC (heightLabel), HILDON_MARGIN_DOUBLE, 0);//AP
		gtk_size_group_add_widget (group, heightLabel);//AP
		
		GtkWidget * heightSelect = gtk_hscale_new_with_range ((gdouble)1.0, (gdouble)10.0, (gdouble)0.1);//AP
		

	
		gtk_range_set_value(GTK_RANGE(heightSelect), self->priv->heightRatio);//AP
		
		gtk_box_pack_start (GTK_BOX (heightBox), heightLabel, FALSE, FALSE, 0);//AP
		gtk_box_pack_start (GTK_BOX (heightBox), heightSelect, TRUE, TRUE, 0);//AP
		
		gtk_container_add (GTK_CONTAINER (content_area), heightBox);//AP

		GtkWidget *checkDisplayTitle = hildon_check_button_new (HILDON_SIZE_AUTO_WIDTH | HILDON_SIZE_FINGER_HEIGHT);
		gtk_button_set_label (GTK_BUTTON (checkDisplayTitle), "Display Title");
		gtk_container_add (GTK_CONTAINER (content_area), checkDisplayTitle);
		hildon_check_button_set_active (HILDON_CHECK_BUTTON (checkDisplayTitle), self->priv->displayTitle);
		//END

		//BEGIN Update Policy
		GtkWidget *updPolicyLabel = gtk_label_new ("");
		gtk_label_set_markup (GTK_LABEL (updPolicyLabel), "<small>Update Policy:</small>");
		gtk_container_add (GTK_CONTAINER (content_area), updPolicyLabel);

		GtkWidget *checkBtBoot = hildon_check_button_new (HILDON_SIZE_AUTO_WIDTH | HILDON_SIZE_FINGER_HEIGHT);
		gtk_button_set_label (GTK_BUTTON (checkBtBoot), "Update on Boot");
		gtk_container_add (GTK_CONTAINER (content_area), checkBtBoot);
		hildon_check_button_set_active (HILDON_CHECK_BUTTON (checkBtBoot), self->priv->updOnBoot);
		
		GtkWidget *checkBtClick = hildon_check_button_new (HILDON_SIZE_AUTO_WIDTH | HILDON_SIZE_FINGER_HEIGHT);
		gtk_button_set_label (GTK_BUTTON (checkBtClick), "Update when clicked");
		gtk_container_add (GTK_CONTAINER (content_area), checkBtClick);
		hildon_check_button_set_active (HILDON_CHECK_BUTTON (checkBtClick), self->priv->updOnClick);

		GtkWidget *checkBtDesktop = hildon_check_button_new (HILDON_SIZE_AUTO_WIDTH | HILDON_SIZE_FINGER_HEIGHT);
		gtk_button_set_label (GTK_BUTTON (checkBtDesktop), "Update when switched to the desktop");
		gtk_container_add (GTK_CONTAINER (content_area), checkBtDesktop);
		hildon_check_button_set_active (HILDON_CHECK_BUTTON (checkBtDesktop), self->priv->updOnDesktop);

		GtkWidget *intervalSelector = hildon_touch_selector_new_text ();
		hildon_touch_selector_append_text (HILDON_TOUCH_SELECTOR (intervalSelector), "Disabled");
		hildon_touch_selector_append_text (HILDON_TOUCH_SELECTOR (intervalSelector), "30 Seconds");
		hildon_touch_selector_append_text (HILDON_TOUCH_SELECTOR (intervalSelector), "1 Minute");
		hildon_touch_selector_append_text (HILDON_TOUCH_SELECTOR (intervalSelector), "5 Minutes");
		hildon_touch_selector_append_text (HILDON_TOUCH_SELECTOR (intervalSelector), "30 Minutes");
		hildon_touch_selector_append_text (HILDON_TOUCH_SELECTOR (intervalSelector), "1 Hour");
		hildon_touch_selector_append_text (HILDON_TOUCH_SELECTOR (intervalSelector), "6 Hours");
		hildon_touch_selector_append_text (HILDON_TOUCH_SELECTOR (intervalSelector), "12 Hours");
		hildon_touch_selector_append_text (HILDON_TOUCH_SELECTOR (intervalSelector), "Daily");
		
		hildon_touch_selector_set_active (HILDON_TOUCH_SELECTOR (intervalSelector), 0, self->priv->delayIndex);
		
		GtkWidget * intervalSelBt = hildon_picker_button_new (HILDON_SIZE_AUTO_WIDTH | HILDON_SIZE_FINGER_HEIGHT, HILDON_BUTTON_ARRANGEMENT_HORIZONTAL);
		hildon_button_set_title (HILDON_BUTTON (intervalSelBt), "Update Interval: ");
		hildon_button_add_title_size_group (HILDON_BUTTON (intervalSelBt), group);
		hildon_button_set_alignment (HILDON_BUTTON (intervalSelBt), 0, 0.5, 0, 0);
		hildon_picker_button_set_selector (HILDON_PICKER_BUTTON (intervalSelBt), HILDON_TOUCH_SELECTOR (intervalSelector));
		gtk_container_add (GTK_CONTAINER (content_area), intervalSelBt);

		
// 		GtkWidget *updIntervalLabel = gtk_label_new ("");
// 		gtk_label_set_markup (GTK_LABEL (updIntervalLabel), "<small>'0' disables periodic updating.</small>");
// 		gtk_misc_set_alignment (GTK_MISC (updIntervalLabel), 0, 0);
// 		gtk_container_add (GTK_CONTAINER (content_area), updIntervalLabel);


		GtkWidget *networkPolicySel = hildon_touch_selector_new_text ();
		hildon_touch_selector_append_text (HILDON_TOUCH_SELECTOR (networkPolicySel), "Disabled (Doesn't Matter)");
		hildon_touch_selector_append_text (HILDON_TOUCH_SELECTOR (networkPolicySel), "When connected");
		hildon_touch_selector_append_text (HILDON_TOUCH_SELECTOR (networkPolicySel), "When disconnected");
		hildon_touch_selector_set_active (HILDON_TOUCH_SELECTOR (networkPolicySel), 0, self->priv->updNeworkPolicy);
		
		GtkWidget * networkPolBt = hildon_picker_button_new (HILDON_SIZE_AUTO_WIDTH | HILDON_SIZE_FINGER_HEIGHT, HILDON_BUTTON_ARRANGEMENT_HORIZONTAL);
		hildon_button_set_title (HILDON_BUTTON (networkPolBt), "Network Presence: ");
		hildon_button_add_title_size_group (HILDON_BUTTON (networkPolBt), group);
		hildon_button_set_alignment (HILDON_BUTTON (networkPolBt), 0, 0.5, 0, 0);
		hildon_picker_button_set_selector (HILDON_PICKER_BUTTON (networkPolBt), HILDON_TOUCH_SELECTOR (networkPolicySel));
		gtk_container_add (GTK_CONTAINER (content_area), networkPolBt);

		GtkWidget *networkPolBtHelp = gtk_label_new ("");
		gtk_label_set_markup (GTK_LABEL (networkPolBtHelp), "<small>If enabled, the selected network event will trigger the command.</small>");
		gtk_misc_set_alignment (GTK_MISC (networkPolBtHelp), 0, 0);
		gtk_label_set_line_wrap (GTK_LABEL (networkPolBtHelp), TRUE);
		gtk_container_add (GTK_CONTAINER (content_area), networkPolBtHelp);
		
		
		//END

		gtk_widget_show_all (dialog);

		//BEGIN result processing

		int setDialogReturn = 0;

		setDialogReturn = gtk_dialog_run (GTK_DIALOG (dialog));
		if (setDialogReturn == GTK_RESPONSE_ACCEPT || setDialogReturn == NON_GTK_RESPONSE_EDIT_CMD)
		{
			int cmdSel = hildon_touch_selector_get_active (HILDON_TOUCH_SELECTOR (selector), 0);
			if(cmdSel < self->priv->c_size && cmdSel >= 0)
			{
				self->priv->instanceTitle = g_strdup(self->priv->c_titles[cmdSel]);
				self->priv->instanceCmd = g_strdup(self->priv->c_commands[cmdSel]);
			}

			self->priv->widthRatio = gtk_range_get_value(GTK_RANGE(widthSelect));
			self->priv->heightRatio = gtk_range_get_value(GTK_RANGE(heightSelect));

// 			gtk_widget_set_size_request (GTK_WIDGET (self), (int)(SIZE_WIDTH_ALL*self->priv->widthRatio), SIZE_HEIGHT_LINE);
// 			gtk_window_resize (GTK_WINDOW (self), (int)(SIZE_WIDTH_ALL*self->priv->widthRatio), SIZE_HEIGHT_LINE);
			gtk_widget_set_size_request (GTK_WIDGET (self), (int)(SIZE_WIDTH_ALL*self->priv->widthRatio), (int)(SIZE_HEIGHT_LINE*self->priv->heightRatio));//AP
			gtk_window_resize (GTK_WINDOW (self), (int)(SIZE_WIDTH_ALL*self->priv->widthRatio), (int)(SIZE_HEIGHT_LINE*self->priv->heightRatio));//AP

			self->priv->displayTitle = hildon_check_button_get_active (HILDON_CHECK_BUTTON (checkDisplayTitle));
			
			self->priv->updOnClick = hildon_check_button_get_active (HILDON_CHECK_BUTTON (checkBtClick));
			self->priv->updOnDesktop = hildon_check_button_get_active (HILDON_CHECK_BUTTON (checkBtDesktop));
			self->priv->updOnBoot = hildon_check_button_get_active (HILDON_CHECK_BUTTON (checkBtBoot));
			
			self->priv->delayIndex = hildon_touch_selector_get_active (HILDON_TOUCH_SELECTOR (intervalSelector), 0);
			self->priv->delaySeconds = GetSeconds(self->priv->delayIndex);

			self->priv->updNeworkPolicy = hildon_touch_selector_get_active (HILDON_TOUCH_SELECTOR (networkPolicySel), 0);
			
			if (self->priv->updateTimerID != 0) {
				g_source_remove (self->priv->updateTimerID);
				self->priv->updateTimerID = 0;
			}

			desktop_cmd_exec_write_settings (self, FALSE, FALSE);
		}

		if (setDialogReturn == GTK_RESPONSE_ACCEPT)
		{
			desktop_cmd_exec_update_content (self);
			settingsRunning = 0;
		}

		if(setDialogReturn == NON_GTK_RESPONSE_GET_CMDS)
		{
			system("dbus-send --system --type=method_call --dest='com.nokia.osso_browser' /com/nokia/osso_browser/request com.nokia.osso_browser.open_new_window string:'http://wiki.maemo.org/Desktop_Command_Execution_Widget_scripts'");
			desktop_cmd_exec_update_content (self);
			settingsRunning = 0;
		}

		if(setDialogReturn == NON_GTK_RESPONSE_ADD_CMD)
		{
			desktop_cmd_exec_edit_add_dialog(self, TRUE, 0);
			desktop_cmd_exec_write_settings (self, FALSE, FALSE);
		}

		if(setDialogReturn == NON_GTK_RESPONSE_EDIT_CMD)
		{
			int cmdSel = hildon_touch_selector_get_active (HILDON_TOUCH_SELECTOR (selector), 0);
			if(cmdSel < self->priv->c_size && cmdSel >= 0)
			{
				desktop_cmd_exec_edit_add_dialog(self,FALSE, cmdSel);
				desktop_cmd_exec_write_settings (self, FALSE, FALSE);
			}

		}
		else
			settingsRunning = 0;
		
		gtk_widget_destroy (dialog);

		//END
	}
}

void desktop_cmd_exec_edit_add_dialog ( DesktopCmdExec *self, gboolean new, gint curr)
{
	GtkWidget *dialog;
	gchar * title;
	if(new)
		title = g_strconcat ("Add new command", NULL);
	else
		title = g_strconcat ("Edit ", self->priv->c_titles[curr] ," command", NULL);

	if(new)
		dialog = gtk_dialog_new_with_buttons (title, NULL, 0, _("wdgt_bd_save"), GTK_RESPONSE_ACCEPT, NULL);
	else
		dialog = gtk_dialog_new_with_buttons (title, NULL, 0, _("wdgt_bd_save"), GTK_RESPONSE_ACCEPT, _("wdgt_bd_delete"), NON_GTK_RESPONSE_DELETE_CMD, NULL);
	
	GtkWidget *content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
	GtkSizeGroup *group = GTK_SIZE_GROUP (gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL));

	//BEGIN title
	GtkWidget *titleBox = gtk_hbox_new (FALSE, 0);
	
	GtkWidget *titleLabel = gtk_label_new ("Title:");
	gtk_misc_set_alignment (GTK_MISC (titleLabel), 0, 0.5);
	gtk_misc_set_padding (GTK_MISC (titleLabel), HILDON_MARGIN_DOUBLE, 0);
	gtk_size_group_add_widget (group, titleLabel);
	
	GtkWidget *titleEntry = hildon_entry_new (HILDON_SIZE_AUTO_WIDTH | HILDON_SIZE_FINGER_HEIGHT);
	gtk_box_pack_start (GTK_BOX (titleBox), titleLabel, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (titleBox), titleEntry, TRUE, TRUE, 0);
	if(!new)
	{
		hildon_entry_set_text(HILDON_ENTRY (titleEntry), self->priv->c_titles[curr]);
		gtk_widget_set_sensitive(GTK_WIDGET(titleEntry), FALSE);
	}
	
	
	gtk_container_add (GTK_CONTAINER (content_area), titleBox);
	//END

	//BEGIN command
	GtkWidget *commandBox = gtk_hbox_new (FALSE, 0);
	
	GtkWidget *commandLabel = gtk_label_new ("Command:");
	gtk_misc_set_alignment (GTK_MISC (commandLabel), 0, 0.5);
	gtk_misc_set_padding (GTK_MISC (commandLabel), HILDON_MARGIN_DOUBLE, 0);
	gtk_size_group_add_widget (group, commandLabel);
	
	GtkWidget *commandEntry = hildon_entry_new (HILDON_SIZE_AUTO_WIDTH | HILDON_SIZE_FINGER_HEIGHT);
	//BEGIN Disable automatic capitalisation
	HildonGtkInputMode input_mode = hildon_gtk_entry_get_input_mode (GTK_ENTRY (commandEntry));
	input_mode &= ~HILDON_GTK_INPUT_MODE_AUTOCAP;
	//input_mode &= ~(HILDON_GTK_INPUT_MODE_AUTOCAP | HILDON_GTK_INPUT_MODE_DICTIONARY);
	hildon_gtk_entry_set_input_mode (GTK_ENTRY (commandEntry), input_mode);
	//END: Disable automatic capitalisation
	
	gtk_box_pack_start (GTK_BOX (commandBox), commandLabel, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (commandBox), commandEntry, TRUE, TRUE, 0);
	if(!new)
		hildon_entry_set_text(HILDON_ENTRY (commandEntry), self->priv->c_commands[curr]);
	
	gtk_container_add (GTK_CONTAINER (content_area), commandBox);
	//END

	gtk_widget_show_all (dialog);
	int dialogRunResponse = gtk_dialog_run (GTK_DIALOG (dialog));
	int i=0;
	switch(dialogRunResponse)
	{
		case GTK_RESPONSE_ACCEPT:
			if(g_strcmp0((gchar *)"",hildon_entry_get_text (HILDON_ENTRY (titleEntry))) == 0)
			{
				hildon_entry_set_text(HILDON_ENTRY (titleEntry), "[No Title]");
			}
			
			if(new)
			{
				AddCommand(self, g_strdup(hildon_entry_get_text (HILDON_ENTRY (titleEntry))), g_strdup(hildon_entry_get_text (HILDON_ENTRY (commandEntry))));
			}
			else
			{
				EditCommand(self, g_strdup(hildon_entry_get_text (HILDON_ENTRY (titleEntry))), g_strdup(hildon_entry_get_text (HILDON_ENTRY (commandEntry))), curr);
			}
			break;

		case NON_GTK_RESPONSE_DELETE_CMD:
			DelCommand(self,curr);
			break;

	}
	gtk_widget_destroy (dialog);
	g_free(title);
}





//Auxiliary functions
void AddCommand(DesktopCmdExec *self, gchar * s_title, gchar * s_command)
{
	gchar ** newTitles = malloc( sizeof(gchar*)*(self->priv->c_size+2) );
	gchar ** newCommands = malloc( sizeof(gchar*)*(self->priv->c_size+2) );
	
	int i;
	for(i=0;i<self->priv->c_size;i++)
	{
		newTitles[i] = g_strdup(self->priv->c_titles[i]);
		newCommands[i] = g_strdup(self->priv->c_commands[i]);
	}
	
	g_strfreev(self->priv->c_titles);
	self->priv->c_titles = NULL;
	g_strfreev(self->priv->c_commands);
	self->priv->c_commands = NULL;
	
	newTitles[self->priv->c_size] = s_title;
	newCommands[self->priv->c_size] = s_command;

	newTitles[self->priv->c_size+1] = NULL;
	newCommands[self->priv->c_size+1] = NULL;

	g_free(self->priv->instanceTitle);
	self->priv->instanceTitle = NULL;
	g_free(self->priv->instanceCmd);
	self->priv->instanceCmd = NULL;
	
	self->priv->instanceTitle = g_strdup(newTitles[self->priv->c_size]);
	self->priv->instanceCmd = g_strdup(newCommands[self->priv->c_size]);

	
	self->priv->c_size += 1;
	
	self->priv->c_titles = newTitles;
	self->priv->c_commands = newCommands;
}

void EditCommand(DesktopCmdExec *self, gchar * s_title, gchar * s_command, int index)
{
	//Clean old
	g_free(self->priv->c_titles[index]);
	g_free(self->priv->c_commands[index]);

	self->priv->c_titles[index] = s_title;
	self->priv->c_commands[index] = s_command;
	/*
	........ check the thing
	self->priv->c_titles[index] = s_title;
	self->priv->c_commands[index] = s_command;
	//vvvv BUG try to fix bug when NULL selector when back from command edit
	g_free(self->priv->instanceTitle);
	g_free(self->priv->instanceCmd);
	self->priv->instanceTitle = g_strdup(s_title);
	self->priv->instanceCmd = g_strdup(s_command);
	//^^^^ BUG try to fix bug when NULL selector when back from command edit
	
	*/
}

void DelCommand(DesktopCmdExec *self, int index)
{
	gchar ** newTitles = malloc( sizeof(gchar*)*(self->priv->c_size) );
	gchar ** newCommands = malloc( sizeof(gchar*)*(self->priv->c_size) );

	int i;
	for(i=0;i<self->priv->c_size;i++)
	{
		//index is the delete target
		if(i < index)
		{
			newTitles[i] = g_strdup(self->priv->c_titles[i]);
			newCommands[i] = g_strdup(self->priv->c_commands[i]);
		}
		else if(i > index)
		{
			newTitles[i-1] = g_strdup(self->priv->c_titles[i]);
			newCommands[i-1] = g_strdup(self->priv->c_commands[i]);
		}
	}
	newTitles[self->priv->c_size-1] = NULL;
	newCommands[self->priv->c_size-1] = NULL;

	//clear old
	g_strfreev(self->priv->c_titles);
	self->priv->c_titles = NULL;
	g_strfreev(self->priv->c_commands);
	self->priv->c_commands = NULL;

	//assign new
	self->priv->c_titles = newTitles;
	self->priv->c_commands = newCommands;

	g_free(self->priv->instanceTitle);
	self->priv->instanceTitle = NULL;
	g_free(self->priv->instanceCmd);
	self->priv->instanceCmd = NULL;

	if(self->priv->c_size > 1)
	{
		self->priv->instanceTitle = g_strdup(newTitles[0]);
		self->priv->instanceCmd = g_strdup(newCommands[0]);
	}
	
	self->priv->c_size -= 1;//decrement size
}

guint GetSeconds(guint index)
{
	if(index > 0 && index < 9)
	{
		guint intervalTimes[] = {0, 30, 60, 300, 1800, 3600, 21600, 43200, 86400};
		return intervalTimes[index];
	}
	return 0;
}
