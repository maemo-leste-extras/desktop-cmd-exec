/*

desktop-cmd-exec.h - Desktop Command Execution Widget

Copyright (c) 2010 cpscotti (Clovis Peruchi Scotti)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

[cpscotti, roboscotti, scotti@ieee.org]

*/

#ifndef _DESKTOP_CMD_EXEC
#define _DESKTOP_CMD_EXEC

#include <libhildondesktop/libhildondesktop.h>
#include <time.h> /* For tm. */

G_BEGIN_DECLS

#define DESKTOP_CMD_EXEC_TYPE desktop_cmd_exec_get_type()

#define DESKTOP_CMD_EXEC(obj) \
(G_TYPE_CHECK_INSTANCE_CAST ((obj), DESKTOP_CMD_EXEC_TYPE, DesktopCmdExec))

#define DESKTOP_CMD_EXEC_CLASS(klass) \
(G_TYPE_CHECK_CLASS_CAST ((klass), DESKTOP_CMD_EXEC_TYPE, DesktopCmdExecClass))

#define DESKTOP_CMD_EXEC_IS(obj) \
(G_TYPE_CHECK_INSTANCE_TYPE ((obj), DESKTOP_CMD_EXEC_TYPE))

#define DESKTOP_CMD_EXEC_IS_CLASS(klass) \
(G_TYPE_CHECK_CLASS_TYPE ((klass), DESKTOP_CMD_EXEC_TYPE))

#define DESKTOP_CMD_EXEC_GET_CLASS(obj) \
(G_TYPE_INSTANCE_GET_CLASS ((obj), DESKTOP_CMD_EXEC_TYPE, DesktopCmdExecClass))

typedef struct _DesktopCmdExec        DesktopCmdExec;
typedef struct _DesktopCmdExecClass   DesktopCmdExecClass;
typedef struct _DesktopCmdExecPrivate DesktopCmdExecPrivate;

struct _DesktopCmdExec
{
  HDHomePluginItem parent;

	DesktopCmdExecPrivate *priv;
};

struct _DesktopCmdExecClass
{
  HDHomePluginItemClass  parent;
};

GType desktop_cmd_exec_get_type (void);

G_END_DECLS

#endif /* _DESKTOP_CMD_EXEC */

