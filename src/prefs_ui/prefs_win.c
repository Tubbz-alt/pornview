/*----------------------------------------------------------------------------*
 *                                                                 .---.      *
 *                           PornView                             (_,/\ \     *
 *           photo/movie collection viewer and manager           (`a a(  )    *
 *                    trem0r <trem0r@tlen.pl>                    ) \=  ) (    *
 *                        (c) 2002,2003                         (.--' '--.)   *
 *                                                              / (_)^(_) \   *
 *----------------------------------------------------------------------------*/

/*
 * These codes are taken from GImageView.
 * GImageView author: Takuro Ashie
 */

#include "pornview.h"

#include "gedo-hpaned.h"
#include "gtk2-compat.h"
#include "gtkcellrendererpixmap.h"

#include "prefs_win.h"
#include "prefs_ui_common.h"
#include "prefs_ui_dirview.h"
#include "prefs_ui_imageview.h"
#include "prefs_ui_plugin.h"
#include "prefs_ui_progs.h"
#include "prefs_ui_thumbview.h"
#include "prefs_ui_etc.h"
#include "browser.h"

typedef struct PrefsWin_Tag
{
    GtkWidget *notebook;
    GtkWidget *tree;
    gboolean ok_pressed;
}
PrefsWin;

typedef struct PrefsWinPagePrivate_Tag
{
    PrefsWinPage *page;
    GtkWidget *widget;
#ifndef ENABLE_TREEVIEW
    GtkCTreeNode *node;
#endif				/* ENABLE_TREEVIEW */
}
PrefsWinPagePrivate;

static PrefsWinPage prefs_pages[] = {
    {N_("/Common"), NULL, NULL, NULL, prefs_common_page, NULL},
    {N_("/Common/Filtering"), NULL, NULL, NULL, prefs_filter_page,
     prefs_filter_apply},
    {N_("/Common/Character Set"), NULL, NULL, NULL, prefs_charset_page, NULL},
    {N_("Directory View"), NULL, NULL, NULL, prefs_dirview_page,
     prefs_dirview_apply},
    {N_("Image View"), NULL, NULL, NULL, prefs_imageview_page,
     prefs_imageview_apply},
    {N_("Thumbnail View"), NULL, NULL, NULL, prefs_thumbview_page,
     prefs_thumbview_apply},
    {N_("Comment"), NULL, NULL, NULL, prefs_comment_page,
     prefs_comment_apply},
    {N_("Slideshow"), NULL, NULL, NULL, prefs_slideshow_page, NULL},
    {N_("/External Program"), NULL, NULL, NULL, prefs_progs_page, NULL},
    {N_("/External Program/Scripts"), NULL, NULL, NULL, prefs_scripts_page,
     NULL},
    {N_("/Plugin"), NULL, NULL, NULL, prefs_plugin_page, prefs_plugin_apply},
};

static gint prefs_pages_num = sizeof (prefs_pages) / sizeof (prefs_pages[0]);

Config *config_changed = NULL;
Config *config_prechanged = NULL;

GtkWidget *prefs_window;

static PrefsWin prefs_win = {
    notebook:NULL,
    tree:NULL,
    ok_pressed:FALSE,
};
static GList *priv_page_list = NULL;

/*
 *-------------------------------------------------------------------
 * private functions
 *-------------------------------------------------------------------
 */

/*  prefs_win_apply_config:
 *     @ Apply changed config to existing window.
 *
 *  src  : source config data.
 *  dest : destination config data.
 */
static void
prefs_win_apply_config (Config * src, Config * dest, PrefsWinButton state)
{
    gint    i;

    for (i = 0; i < prefs_pages_num; i++)
    {
	if (prefs_pages[i].apply_fn)
	    prefs_pages[i].apply_fn (src, dest, state);
    }
}

static void
prefs_win_free_strings (gboolean value_changed)
{
    gint    i;
    gchar **strings[] = {
	&conf.startup_dir,
	&conf.imgtype_disables,
	&conf.imgtype_user_defs,
	&conf.comment_key_list,
	&conf.comment_charset,
	&conf.charset_locale,
	&conf.charset_internal,
	&conf.charset_filename
    };

    gint    num = sizeof (strings) / sizeof (gchar **);

    for (i = 0; i < num; i++)
    {
	gchar **src, **dest, **member;
	gulong  offset;

	if (i < num)
	    member = strings[i];
	else
	    break;

	offset = (gchar *) member - (gchar *) & conf;
	src = G_STRUCT_MEMBER_P (config_prechanged, offset);
	dest = G_STRUCT_MEMBER_P (config_changed, offset);

	if (*src == *dest)
	    continue;

	if (value_changed)
	{
	    g_free (*src);
	}
	else
	{
	    g_free (*dest);
	}
    }
}

static void
prefs_win_create_page (PrefsWinPagePrivate * priv)
{
    const gchar *title = NULL;
    GtkWidget *vbox, *label;

    if (!priv || !priv->page)
	return;
    if (priv->widget)
	return;

    /*
     * translate page title 
     */
    if (priv->page->path)
	title = g_basename (_(priv->page->path));

    /*
     * create page 
     */
    if (priv->page->create_page_fn)
    {
	vbox = priv->page->create_page_fn ();
	label = gtk_label_new (title);
	gtk_notebook_append_page (GTK_NOTEBOOK (prefs_win.notebook),
				  vbox, label);
	priv->widget = vbox;
    }
}

void
prefs_win_set_page (const gchar * path)
{
    PrefsWinPagePrivate *priv = NULL;
    GList  *node;
    gint    num;

    if (!path || !*path)
    {
	if (!priv_page_list)
	    return;
	priv = priv_page_list->data;
    }
    else
    {
	for (node = priv_page_list; node; node = g_list_next (node))
	{
	    priv = node->data;
	    if (priv->page && !strcmp (path, priv->page->path))
		break;
	    priv = NULL;
	}
    }

    if (!priv)
    {
	if (!priv_page_list)
	    return;
	priv = priv_page_list->data;
    }

    if (!priv->widget)
    {
	prefs_win_create_page (priv);
    }
    if (priv->widget)
	gtk_widget_show (priv->widget);
    else
	return;

    num = gtk_notebook_page_num (GTK_NOTEBOOK (prefs_win.notebook),
				 priv->widget);
    if (num >= 0)
	gtk_notebook_set_page (GTK_NOTEBOOK (prefs_win.notebook), num);

#ifdef ENABLE_TREEVIEW
#else /* ENABLE_TREEVIEW */

/*  gtk_ctree_select (GTK_CTREE (prefs_win.tree), priv->node);  */

#endif /* ENABLE_TREEVIEW */
}

/*
 *-------------------------------------------------------------------
 * functions for creating navigate tree
 *-------------------------------------------------------------------
 */

#ifdef ENABLE_TREEVIEW

typedef enum
{
    COLUMN_TERMINATOR = -1,
    COLUMN_ICON_OPEN,
    COLUMN_ICON_OPEN_MASK,
    COLUMN_ICON_CLOSE,
    COLUMN_ICON_CLOSE_MASK,
    COLUMN_NAME,
    COLUMN_PRIV_DATA,
    N_COLUMN
}
TreeStoreColumn;

struct FindNode
{
    gchar  *path;
    PrefsWinPagePrivate *priv;
    GtkTreeIter iter;
};

static  gboolean
find_node_by_path (GtkTreeModel * model,
		   GtkTreePath * path, GtkTreeIter * iter, gpointer data)
{
    struct FindNode *node = data;
    PrefsWinPagePrivate *priv;

    gtk_tree_model_get (model, iter,
			COLUMN_PRIV_DATA, &priv, COLUMN_TERMINATOR);
    if (priv && !strcmp (priv->page->path, node->path))
    {
	node->priv = priv;
	node->iter = *iter;
	return TRUE;
    }

    return FALSE;
}

static  gboolean
prefs_win_navtree_get_parent (PrefsWinPagePrivate * priv, GtkTreeIter * iter)
{
    GtkTreeModel *model;
    struct FindNode node;

    g_return_val_if_fail (priv, FALSE);
    g_return_val_if_fail (priv->page, FALSE);
    g_return_val_if_fail (priv->page->path, FALSE);

    node.path = g_dirname (priv->page->path);
    node.priv = NULL;

    model = gtk_tree_view_get_model (GTK_TREE_VIEW (prefs_win.tree));
    gtk_tree_model_foreach (model, find_node_by_path, &node);

    g_free (node.path);

    if (node.priv)
    {
	*iter = node.iter;
	return TRUE;
    }

    return FALSE;
}

static void
cb_tree_cursor_changed (GtkTreeView * treeview, gpointer data)
{
    GtkTreeSelection *selection;
    GtkTreeModel *model;
    GtkTreeIter iter;
    PrefsWinPagePrivate *priv;

    g_return_if_fail (treeview);

    selection = gtk_tree_view_get_selection (treeview);
    gtk_tree_selection_get_selected (selection, &model, &iter);
    gtk_tree_model_get (model, &iter,
			COLUMN_PRIV_DATA, &priv, COLUMN_TERMINATOR);

    g_return_if_fail (priv);
    g_return_if_fail (priv->page);

    prefs_win_set_page (priv->page->path);
}

static GtkWidget *
prefs_win_create_navtree (void)
{
    GtkTreeStore *store;
    GtkTreeViewColumn *col;
    GtkCellRenderer *render;
    GtkWidget *tree;
    gint    i;

    store = gtk_tree_store_new (N_COLUMN,
				GDK_TYPE_PIXMAP,
				GDK_TYPE_PIXMAP,
				GDK_TYPE_PIXMAP,
				GDK_TYPE_PIXMAP,
				G_TYPE_STRING, G_TYPE_POINTER);
    tree = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
    prefs_win.tree = tree;
    gtk_tree_view_set_rules_hint (GTK_TREE_VIEW (tree), TRUE);
    gtk_tree_view_set_headers_visible (GTK_TREE_VIEW (tree), FALSE);

    g_signal_connect (G_OBJECT (tree), "cursor_changed",
		      G_CALLBACK (cb_tree_cursor_changed), NULL);

    /*
     * name column 
     */
    col = gtk_tree_view_column_new ();

    render = gtk_cell_renderer_pixmap_new ();
    gtk_tree_view_column_pack_start (col, render, FALSE);
    gtk_tree_view_column_add_attribute (col, render, "pixmap",
					COLUMN_ICON_CLOSE);
    gtk_tree_view_column_add_attribute (col, render, "mask",
					COLUMN_ICON_CLOSE_MASK);
    gtk_tree_view_column_add_attribute (col, render, "pixmap_expander_open",
					COLUMN_ICON_OPEN);
    gtk_tree_view_column_add_attribute (col, render, "mask_expander_open",
					COLUMN_ICON_OPEN_MASK);
    gtk_tree_view_column_add_attribute (col, render, "pixmap_expander_closed",
					COLUMN_ICON_CLOSE);
    gtk_tree_view_column_add_attribute (col, render, "mask_expander_closed",
					COLUMN_ICON_CLOSE_MASK);

    render = gtk_cell_renderer_text_new ();
    gtk_tree_view_column_pack_start (col, render, TRUE);
    gtk_tree_view_column_add_attribute (col, render, "text", COLUMN_NAME);

    gtk_tree_view_append_column (GTK_TREE_VIEW (tree), col);
    gtk_tree_view_set_expander_column (GTK_TREE_VIEW (tree), col);

    /*
     * create pages 
     */
    for (i = 0; i < prefs_pages_num; i++)
    {
	const gchar *title;
	GdkPixmap *pixmap = NULL, *opixmap = NULL;
	GdkBitmap *mask = NULL, *omask = NULL;
	PrefsWinPagePrivate *priv;
	GtkTreeIter iter, parent_iter;

	if (!prefs_pages[i].path)
	    continue;

	/*
	 * translate page title 
	 */
	title = g_basename (_(prefs_pages[i].path));

	/*
	 * set private data 
	 */
	priv = g_new0 (PrefsWinPagePrivate, 1);
	priv->page = &prefs_pages[i];
	priv->widget = NULL;

	priv_page_list = g_list_append (priv_page_list, priv);

	if (prefs_win_navtree_get_parent (priv, &parent_iter))
	{
	    gtk_tree_store_append (store, &iter, &parent_iter);
	}
	else
	{
	    gtk_tree_store_append (store, &iter, NULL);
	}
	gtk_tree_store_set (store, &iter,
			    COLUMN_ICON_CLOSE, pixmap,
			    COLUMN_ICON_CLOSE_MASK, mask,
			    COLUMN_ICON_OPEN, opixmap,
			    COLUMN_ICON_OPEN_MASK, omask,
			    COLUMN_NAME, title,
			    COLUMN_PRIV_DATA, priv, COLUMN_TERMINATOR);
    }

    return tree;
}

#else /* ENABLE_TREEVIEW */

static void
cb_ctree_select_row (GtkWidget * widget, gint row, gint column,
		     GdkEventButton * event, gpointer data)
{
    GtkCTreeNode *node;
    PrefsWinPagePrivate *priv;

    node = gtk_ctree_node_nth (GTK_CTREE (widget), row);
    if (!node)
	return;

    priv = gtk_ctree_node_get_row_data (GTK_CTREE (widget), node);
    if (priv->page)
	prefs_win_set_page (priv->page->path);
}

static GtkCTreeNode *
prefs_win_navtree_get_parent (PrefsWinPage * page)
{
    GList  *node = NULL;
    gchar  *parent;

    g_return_val_if_fail (page, NULL);
    g_return_val_if_fail (page->path, NULL);

    if (!page->path)
	return NULL;
    parent = g_dirname (page->path);

    for (node = priv_page_list; node; node = g_list_next (node))
    {
	PrefsWinPagePrivate *priv = node->data;
	if (priv && priv->page && !strcmp (parent, priv->page->path))
	{
	    g_free (parent);
	    return priv->node;
	}
    }

    g_free (parent);

    return NULL;
}

static GtkWidget *
prefs_win_create_navtree (void)
{
    GtkWidget *ctree;
    GtkCTreeNode *node, *parent = NULL;
    gint    i;
    GtkCTreeNode *sel_node = NULL;

    /*
     * create tree 
     */
    ctree = prefs_win.tree = gtk_ctree_new (1, 0);
    prefs_win.tree = ctree;
    gtk_clist_set_column_auto_resize (GTK_CLIST (ctree), 0, TRUE);
    gtk_clist_set_selection_mode (GTK_CLIST (ctree), GTK_SELECTION_BROWSE);
    gtk_ctree_set_line_style (GTK_CTREE (ctree), conf.dirtree_line_style);
    gtk_ctree_set_expander_style (GTK_CTREE (ctree),
				  conf.dirtree_expander_style);
    gtk_clist_set_row_height (GTK_CLIST (ctree), 18);

    gtk_signal_connect (GTK_OBJECT (ctree), "select-row",
			GTK_SIGNAL_FUNC (cb_ctree_select_row), NULL);

    /*
     * create pages 
     */
    for (i = 0; i < prefs_pages_num; i++)
    {
	const gchar *title;
	GdkPixmap *pixmap = NULL, *opixmap = NULL;
	GdkBitmap *mask = NULL, *omask = NULL;
	PrefsWinPagePrivate *priv;

	if (!prefs_pages[i].path)
	    continue;

	/*
	 * translate page title 
	 */
	title = g_basename (_(prefs_pages[i].path));

	/*
	 * insert node 
	 */
	parent = prefs_win_navtree_get_parent (&prefs_pages[i]);
	node = gtk_ctree_insert_node (GTK_CTREE (ctree), parent, NULL,
				      (gchar **) & title, 4,
				      pixmap, mask,
				      opixmap, omask, FALSE, FALSE);

	if (parent)
	    gtk_ctree_expand (GTK_CTREE (ctree), parent);

	/*
	 * set private data 
	 */
	priv = g_new0 (PrefsWinPagePrivate, 1);
	priv->page = &prefs_pages[i];
	priv->widget = NULL;
	priv->node = node;

	priv_page_list = g_list_append (priv_page_list, priv);

	gtk_ctree_node_set_row_data (GTK_CTREE (ctree), node, priv);

	if (i == 0)
	    sel_node = node;
    }

    gtk_ctree_select (GTK_CTREE (ctree), sel_node);

    return ctree;
}

#endif /* ENABLE_TREEVIEW */

/*
 *-------------------------------------------------------------------
 * callback functions for preference window
 *-------------------------------------------------------------------
 */

static void
cb_prefs_win_destroy ()
{
    if (prefs_win.ok_pressed)
    {
	prefs_win_free_strings (TRUE);
	conf = *config_changed;
	prefs_win_apply_config (config_prechanged, &conf, PREFS_WIN_OK);
    }
    else
    {
	prefs_win_free_strings (FALSE);
	conf = *config_prechanged;
	prefs_win_apply_config (config_changed, &conf, PREFS_WIN_CANCEL);
    }

    g_free (config_changed);
    g_free (config_prechanged);
    config_changed = NULL;
    config_prechanged = NULL;

    prefs_window = NULL;
    prefs_win.notebook = NULL;
    prefs_win.tree = NULL;
    prefs_win.ok_pressed = FALSE;

    g_list_foreach (priv_page_list, (GFunc) g_free, NULL);
    g_list_free (priv_page_list);
    priv_page_list = NULL;
}


#ifdef USE_GTK2

static void
cb_dialog_response (GtkDialog * dialog, gint arg, gpointer data)
{
    switch (arg)
    {
      case GTK_RESPONSE_ACCEPT:
	  prefs_win.ok_pressed = TRUE;
	  gtk_widget_destroy (prefs_window);
	  break;
      case GTK_RESPONSE_APPLY:
	  memcpy (&conf, config_changed, sizeof (Config));
	  prefs_win_apply_config (config_prechanged, &conf, PREFS_WIN_APPLY);
	  break;
      case GTK_RESPONSE_REJECT:
	  gtk_widget_destroy (prefs_window);
	  break;
      default:
	  break;
    }
}

#else

static void
cb_prefs_ok_button ()
{
    prefs_win.ok_pressed = TRUE;
    gtk_widget_destroy (prefs_window);
}


static void
cb_prefs_apply_button ()
{
    memcpy (&conf, config_changed, sizeof (Config));
    prefs_win_apply_config (config_prechanged, &conf, PREFS_WIN_APPLY);
}


static void
cb_prefs_cancel_button ()
{
    gtk_widget_destroy (prefs_window);
}

#endif

/*******************************************************************************
 *
 *   prefs_open_window:
 *      @ Create preference window. If already opened, raise it and return.
 *
 *   GtkWidget *prefs_window (global variable):
 *      Pointer to preference window.
 *
 *   Static PrefsWin prefs_win (local variable):
 *      store pointer to eache widget;
 *
 *******************************************************************************/
void
prefs_win_open (const gchar * path)
{
    GtkWidget *notebook, *pane, *scrolledwin, *navtree;

    /*
     * if preference window is alredy opend, raise it and return 
     */
    if (prefs_window)
    {
	gdk_window_raise (prefs_window->window);
	return;
    }

    prefs_win.ok_pressed = FALSE;

    /*
     * allocate buffer for new config 
     */
    config_prechanged = g_memdup (&conf, sizeof (Config));
    config_changed = g_memdup (&conf, sizeof (Config));

    /*
     * create config window 
     */
    prefs_window = gtk_dialog_new ();
    gtk_window_set_position (GTK_WINDOW (prefs_window), GTK_WIN_POS_CENTER);
    gtk_window_set_wmclass (GTK_WINDOW (prefs_window), "prefs", "PornView");
    gtk_window_set_default_size (GTK_WINDOW (prefs_window), 760, 450);
    gtk_window_set_title (GTK_WINDOW (prefs_window), _("Preference"));
    gtk_window_set_transient_for (GTK_WINDOW (prefs_window),
				  GTK_WINDOW (BROWSER_WINDOW));

    gtk_signal_connect (GTK_OBJECT (prefs_window), "destroy",
			GTK_SIGNAL_FUNC (cb_prefs_win_destroy), NULL);

    /*
     * pane 
     */
    pane = gedo_hpaned_new ();
    gtk_container_set_border_width (GTK_CONTAINER (pane), 5);
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (prefs_window)->vbox),
			pane, TRUE, TRUE, 0);
    gtk_widget_show (pane);

    notebook = prefs_win.notebook = gtk_notebook_new ();
    gtk_notebook_set_tab_pos (GTK_NOTEBOOK (notebook), GTK_POS_TOP);
    gtk_notebook_set_scrollable (GTK_NOTEBOOK (notebook), TRUE);
    /*
     * gtk_notebook_popup_enable (GTK_NOTEBOOK (notebook)); 
     */
    gtk_notebook_set_show_tabs (GTK_NOTEBOOK (notebook), FALSE);
    gtk_widget_show (notebook);

    /*
     * scrolled window 
     */
    scrolledwin = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwin),
				    GTK_POLICY_AUTOMATIC,
				    GTK_POLICY_AUTOMATIC);
    gtk_widget_set_usize (scrolledwin, 170, -1);
    gtk_widget_show (scrolledwin);

    /*
     * navigation tree 
     */
    navtree = prefs_win_create_navtree ();
    gtk_container_add (GTK_CONTAINER (scrolledwin), navtree);
    gtk_widget_show (navtree);

    gedo_paned_add1 (GEDO_PANED (pane), scrolledwin);
    gedo_paned_add2 (GEDO_PANED (pane), notebook);

    /*
     * button 
     */
#ifdef USE_GTK2
    gtk_dialog_add_buttons (GTK_DIALOG (prefs_window),
			    GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
			    GTK_STOCK_APPLY, GTK_RESPONSE_APPLY,
			    GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, NULL);
    gtk_signal_connect_object (GTK_OBJECT (prefs_window), "response",
			       GTK_SIGNAL_FUNC (cb_dialog_response), NULL);
#else
    {
	GtkWidget *button;

	/*
	 * dialog buttons 
	 */
	button = gtk_button_new_with_label (_("OK"));
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (prefs_window)->action_area),
			    button, TRUE, TRUE, 0);
	gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				   GTK_SIGNAL_FUNC (cb_prefs_ok_button),
				   GTK_OBJECT (prefs_window));
	GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
	gtk_widget_show (button);

	gtk_widget_grab_focus (button);

	button = gtk_button_new_with_label (_("Apply"));
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (prefs_window)->action_area),
			    button, FALSE, TRUE, 0);
	gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				   GTK_SIGNAL_FUNC (cb_prefs_apply_button),
				   GTK_OBJECT (prefs_window));
	GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
	gtk_widget_show (button);

	button = gtk_button_new_with_label (_("Cancel"));
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (prefs_window)->action_area),
			    button, FALSE, TRUE, 0);
	gtk_signal_connect_object (GTK_OBJECT (button), "clicked",
				   GTK_SIGNAL_FUNC (cb_prefs_cancel_button),
				   GTK_OBJECT (prefs_window));
	GTK_WIDGET_SET_FLAGS (button, GTK_CAN_DEFAULT);
	gtk_widget_show (button);
    }
#endif

    gtk_widget_show (prefs_window);

    prefs_win_set_page ("/General");
}

static  gboolean
idle_prefs_win_open (gpointer data)
{
    gchar  *path = data;
    prefs_win_open (path);
    g_free (path);
    return FALSE;
}

void
prefs_win_open_idle (const gchar * path)
{
    gchar  *str = NULL;

    if (path)
	str = g_strdup (path);

    gtk_idle_add (idle_prefs_win_open, str);
}

gboolean
prefs_win_is_opened (void)
{
    if (prefs_window)
	return TRUE;
    else
	return FALSE;
}
