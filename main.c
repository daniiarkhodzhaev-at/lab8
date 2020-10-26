/* Some code taken from https://developer.gnome.org/goocanvas/stable/goocanvas-simple-canvas.html */

#include <stdlib.h>
#include <goocanvas.h>
#include <math.h>
#include <time.h>

/* global constants */
char *NAME = "";
static gint FPS = 100;
static gint WIDTH = 640;
static gint HEIGHT = 600;
static gint MAX_CANNON_CHARGE = 100;
static gdouble MAX_CANNON_SPEED = 10.0;
static gdouble K = 0.999;
static gdouble AY = 0.1;
static gdouble CHARGE_SPEED = 0.02;

static gint X0 = 50,
            Y0 = 50,
            D  = 5,
            L0 = 25,
            LMAX = 100;

static char *COLORS[] = {"#ff0000", "#0000ff", "#ffff00",
                        "#00ff00", "#ff00ff", "#00ffff"};


struct object_list {
    struct object_list *next;
    GooCanvasItem *obj;
    gdouble x;
    gdouble y;
    gdouble r;
    gdouble vx;
    gdouble vy;
    gint score;
    gint skip;
};

/* containers */
static GtkWidget *window, *canvas;
static GooCanvasItem *root, *cannon;
static gint score = 0;
static gchar *username = "";
static gint cannon_charge = 0;
static gdouble cannon_rot = 0;
static gboolean pressed = FALSE;
static struct timespec *start_time = NULL;
static struct object_list *bullets = NULL;
static struct object_list *targets = NULL;

static inline gint max (gint x, gint y) {
    return (x > y) ? (x) : (y);
}

static inline gint min (gint x, gint y) {
    return (x < y) ? (x) : (y);
}

static gboolean on_button_press      (GooCanvasItem  *view,
                                      GooCanvasItem  *target,
                                      GdkEventButton *event,
                                      gpointer        data);

static gboolean on_canvas_press      (GooCanvasItem  *view,
                                      GooCanvasItem  *target,
                                      GdkEventButton *event,
                                      gpointer        data);

static gboolean on_canvas_release    (GooCanvasItem  *view,
                                      GooCanvasItem  *target,
                                      GdkEventButton *event,
                                      gpointer        data);

static gboolean on_delete_event      (GtkWidget *window,
                                      GdkEvent  *event,
                                      gpointer   unused_data);

static gboolean on_motion_event      (GtkWidget *widget,
                                      GdkEvent  *event,
                                      gpointer   unused_data);

static gboolean on_next_frame        (gpointer unused_data);

static gboolean on_username_entered  (GtkWidget *confirm_button,
                                      GdkEvent  *event,
                                      gpointer   pop_window);

static gint     randint              (gint a,
                                      gint b);

static gint     cannon_get_len       (gint charge);

static gdouble  cannon_get_speed     (gint charge);

static gchar *  cannon_get_color     (gint charge);

/**
 * This function sets up params.
 * @param name (char *) name of app
 * @param width (gint) width of the window
 * @param height (gint) height of the window
 *
 * @return (gint) Exit code
 */
int setup (char *name, gint width, gint height) {
    WIDTH = width;
    HEIGHT = height;
    Y0 = height / 2;
    NAME = malloc(strlen(name) + 1);
    memcpy (NAME, name, strlen(name) + 1);
    start_time = malloc (sizeof (struct timespec));
    return 0;
}

/**
 * This is init function. It initialize GTK+. Uses values set by setup function.
 * Also it queries username via pop-up window.
 * @return (gint) Exit code
 */
int init () {
    int argc = 1;
    char **argv = &NAME;

    GtkWidget *hbox, *c_hbox, *vbox, *halign, *stopBtn, *showLeaderbordBtn,
              *pop_window, *entry_text_label, *entry_text, *confirm_button,
              *pop_vbox, *pop_hbox_entry, *pop_hbox_confirm,
              *pop_vbox_spacer_top, *pop_vbox_spacer_mid, *pop_vbox_spacer_bot,
              *pop_hbox_entry_spacer_lft, *pop_hbox_entry_spacer_rt,
              *pop_hbox_confirm_spacer_lft, *pop_hbox_confirm_spacer_rt;
    GtkEntryBuffer *username_buf;

    /* Initialize GTK+. */
    gtk_set_locale ();
    gtk_init (&argc, &argv);

    /* Create GTK+ window and bind closing callback. */
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW (window), HEIGHT, WIDTH);
    gtk_window_set_resizable(GTK_WINDOW (window), FALSE);
    
    g_signal_connect (window, "delete_event", (GtkSignalFunc) on_delete_event,
                      NULL);

    vbox = gtk_vbox_new (FALSE, 5);
    hbox = gtk_hbox_new (TRUE, 5);
    c_hbox = gtk_hbox_new(FALSE, 0);
    halign = gtk_alignment_new (1, 0, 0, 0);

    /* Create new canvas, set it size and get root element for future usage.*/
    canvas = goo_canvas_new ();
    goo_canvas_set_bounds (GOO_CANVAS (canvas), 0, 0, WIDTH, HEIGHT);
    gtk_widget_set_size_request (canvas, WIDTH, HEIGHT);
    root = goo_canvas_get_root_item (GOO_CANVAS (canvas));
    cannon = goo_canvas_rect_new (root, X0, Y0, cannon_get_len(0), D,
            "line-width", 0.0, "fill-color", cannon_get_color(0), NULL);

    /* Add buttons and layout. */
    stopBtn = gtk_button_new_with_label ("Stop");
    /* showLeaderbordBtn = gtk_button_new_with_label ("Show leaderboard"); */
    showLeaderbordBtn = gtk_label_new ("");
    gtk_widget_set_size_request (stopBtn, 200, 40);

    g_signal_connect (stopBtn, "button_press_event",
                      (GtkSignalFunc) on_delete_event, NULL);
    g_signal_connect (canvas, "motion_notify_event",
                      (GtkSignalFunc) on_motion_event, NULL);
    g_signal_connect (canvas, "button_press_event",
                      (GtkSignalFunc) on_canvas_press, NULL);
    g_signal_connect (canvas, "button_release_event",
                      (GtkSignalFunc) on_canvas_release, NULL);
    gtk_widget_add_events (canvas, GDK_POINTER_MOTION_MASK);

    gtk_container_add (GTK_CONTAINER (hbox), stopBtn);
    gtk_container_add (GTK_CONTAINER (hbox), showLeaderbordBtn);
    gtk_container_add (GTK_CONTAINER (halign), hbox);
    gtk_box_pack_start (GTK_BOX(vbox), halign, FALSE, FALSE, 0);
    gtk_container_add (GTK_CONTAINER (c_hbox), canvas);
    gtk_container_add (GTK_CONTAINER (vbox), c_hbox);
    gtk_container_add (GTK_CONTAINER (window), vbox);

    /* gtk_widget_show_all (window); */


    /* Add dialog for acquiring username */
    pop_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_deletable (GTK_WINDOW (pop_window), FALSE);
    gtk_window_set_resizable (GTK_WINDOW (pop_window), FALSE);

    pop_vbox = gtk_vbox_new (TRUE, 0);
    pop_hbox_entry = gtk_hbox_new (TRUE, 0);
    pop_hbox_confirm = gtk_hbox_new (TRUE, 0);

    pop_vbox_spacer_top = gtk_label_new ("");
    pop_vbox_spacer_mid = gtk_label_new ("");
    pop_vbox_spacer_bot = gtk_label_new ("");

    pop_hbox_entry_spacer_lft = gtk_label_new ("");
    pop_hbox_entry_spacer_rt = gtk_label_new ("");

    pop_hbox_confirm_spacer_lft = gtk_label_new ("");
    pop_hbox_confirm_spacer_rt = gtk_label_new ("");

    entry_text_label = gtk_label_new ("Enter your name:");
    username_buf = gtk_entry_buffer_new (NULL, -1);
    entry_text = gtk_entry_new_with_buffer (username_buf);
    gtk_editable_set_editable (GTK_EDITABLE (entry_text), TRUE);
    
    confirm_button = gtk_button_new_with_label ("Start!");

    gtk_container_add (GTK_CONTAINER (pop_hbox_entry), pop_hbox_entry_spacer_lft);
    gtk_container_add (GTK_CONTAINER (pop_hbox_entry), entry_text);
    gtk_container_add (GTK_CONTAINER (pop_hbox_entry), pop_hbox_entry_spacer_rt);

    gtk_container_add (GTK_CONTAINER (pop_hbox_confirm), pop_hbox_confirm_spacer_lft);
    gtk_container_add (GTK_CONTAINER (pop_hbox_confirm), confirm_button);
    gtk_container_add (GTK_CONTAINER (pop_hbox_confirm), pop_hbox_confirm_spacer_rt);

    gtk_container_add (GTK_CONTAINER (pop_vbox), pop_vbox_spacer_top);
    gtk_container_add (GTK_CONTAINER (pop_vbox), entry_text_label);
    gtk_container_add (GTK_CONTAINER (pop_vbox), pop_hbox_entry);
    gtk_container_add (GTK_CONTAINER (pop_vbox), pop_vbox_spacer_mid);
    gtk_container_add (GTK_CONTAINER (pop_vbox), pop_hbox_confirm);
    gtk_container_add (GTK_CONTAINER (pop_vbox), pop_vbox_spacer_bot);

    gtk_container_add (GTK_CONTAINER (pop_window), pop_vbox);

    g_signal_connect (confirm_button, "button_press_event",
                      (GtkSignalFunc) on_username_entered, pop_window);

    gtk_widget_show_all (pop_window);

    return 0;
}

/**
 * This function adds circle with given params to canvas and sets onclick callback.
 * @param list (void **) list to add object
 * @param x (gdouble) x coord
 * @param y (gdouble) y coord
 * @param r (gdouble) radius
 * @param vx (gdouble) x velocity
 * @param vy (gdouble) y velocity
 * @param color (char *) color in string format
 * @param score (gint) score increment, used once
 *
 * @return (gint) Exit code
 */
int add_circ (void **_list, gdouble x, gdouble y, gdouble r, gdouble vx, gdouble vy, char *color, gint score) {
    struct object_list **list = (struct object_list **)_list;
    struct object_list *new_obj = malloc(sizeof(struct object_list));
    if (!new_obj) {
        g_print("OOM: dropping\n");
        abort();
    }
    new_obj->next = *list;
    new_obj->score = score;
    new_obj->skip = 0;
    new_obj->x = x;
    new_obj->y = y;
    new_obj->r = r;
    new_obj->vx = vx;
    new_obj->vy = vy;
    new_obj->obj = goo_canvas_ellipse_new(root, 0, 0, r, r, "line-width", 0.0, "fill-color", color, NULL);

    cairo_matrix_t *bul_transform = malloc (sizeof (cairo_matrix_t));
    cairo_matrix_init_identity (bul_transform);
    cairo_matrix_translate (bul_transform, new_obj->x, new_obj->y);
    goo_canvas_item_set_transform (new_obj->obj, bul_transform);
    free (bul_transform);

    /*g_signal_connect (new_obj->obj, "button_press_event",
                      (GtkSignalFunc) on_button_press, new_obj);*/
    *list = new_obj;
    return 0;
}

/**
 * This function adds rectangle with given params to canvas and sets onclick callback.
 * @param list (void **) list to add object
 * @param x (gdouble) x coord
 * @param y (gdouble) y coord
 * @param r (gdouble) radius
 * @param vx (gdouble) x velocity
 * @param vy (gdouble) y velocity
 * @param color (char *) color in string format
 * @param score (gint) score increment, used once
 *
 * @return (gint) Exit code
 */
int add_rect (void **list, gdouble x, gdouble y, gdouble r, gdouble vx, gdouble vy, char *color, gint score) {
    struct object_list *new_obj = malloc(sizeof(struct object_list));
    if (!new_obj) {
        g_print("OOM: dropping\n");
        abort();
    }
    new_obj->next = *list;
    new_obj->score = score;
    new_obj->skip = 0;
    new_obj->x = x;
    new_obj->y = y;
    new_obj->r = r;
    new_obj->vx = vx;
    new_obj->vy = vy;
    new_obj->obj = goo_canvas_rect_new(root, 0, 0, r, r, "line-width", 0.0, "fill-color", color, NULL);

    cairo_matrix_t *bul_transform = malloc (sizeof (cairo_matrix_t));
    cairo_matrix_init_identity (bul_transform);
    cairo_matrix_translate (bul_transform, new_obj->x, new_obj->y);
    goo_canvas_item_set_transform (new_obj->obj, bul_transform);
    free (bul_transform);

    /*g_signal_connect (new_obj->obj, "button_press_event",
                      (GtkSignalFunc) on_button_press, new_obj);*/
    *list = new_obj;
    return 0;
}

/**
 * This function removes the last added element from canvas.
 * @return (gint) Exit code
 */
int pop_object () {
    goo_canvas_item_remove (targets->obj);
    gpointer tmp = targets;
    targets = targets->next;
    free (tmp);
    return 0;
}

/**
 * This is mainllop function. It sets next frame callback
 * and passes control to GTK+ main event loop.
 * @return (gint) Exit code
 */
int mainloop () {
    /* Set next frame callback */
    g_timeout_add (1000.0 / FPS, &on_next_frame, NULL);

    /* Pass control to the GTK+ main event loop. */
    gtk_main ();
    return 0;
}

/**
 * This is test function to demonstrate functionality.
 * @return (gint) Exit code
 */
int
main (int argc, char *argv[])
{
    setup ("Test", WIDTH, HEIGHT);

    init ();

    for (gint i = 0; i < 5; ++i) {
        gint x, y, r;
        gdouble vx, vy;
        r = randint (5, 20);
        x = randint (r, WIDTH - r);
        y = randint (r, HEIGHT - r);
        vx = 0.1 * randint (-100, 100);
        vy = 0.1 * randint (-100, 100);
        char *color = COLORS [randint (0, 6)];

        add_rect ((void **) &targets, x, y, r, vx, vy, color, 1);
    }

    mainloop ();

    g_print ("Your score is %i.\n", score);

    return 0;
}


/* This handles button presses in item views. */
static gboolean
on_button_press (GooCanvasItem    *item,
                      GooCanvasItem    *target,
                      GdkEventButton   *event,
                      gpointer          data)
{
    struct object_list *cur = (struct object_list *)data;
    score += cur->score;
    cur->score = 0;
    cur->skip = 1;
    goo_canvas_item_remove(cur->obj);
    return TRUE;
}

static gboolean
on_canvas_press (GooCanvasItem  *item,
                 GooCanvasItem  *target,
                 GdkEventButton *event,
                 gpointer        data)
{
    if (!pressed) {
        pressed = TRUE;
        clock_gettime (CLOCK_REALTIME, start_time);
    }
    return TRUE;
}

static gboolean
on_canvas_release (GooCanvasItem  *item,
                   GooCanvasItem  *target,
                   GdkEventButton *event,
                   gpointer        data)
{
    if (pressed) {
        pressed = FALSE;
        gdouble x, y, r, vx, vy;
        r = randint (5, 20);
        x = X0;
        y = Y0;
        vx = cannon_get_speed (cannon_charge) * cos (cannon_rot);
        vy = cannon_get_speed (cannon_charge) * sin (cannon_rot);
        char *color = COLORS [randint (0, 6)];

        add_circ ((void **) &bullets, x, y, r, vx, vy, color, 0);
        cannon_charge = 0;
    }
}

/* This is our handler for the "delete-event" signal of the window, which
     is emitted when the 'x' close button is clicked. We return controll for
     mainloop initiator here. */
static gboolean
on_delete_event (GtkWidget *window,
                 GdkEvent  *event,
                 gpointer   unused_data)
{
    gtk_main_quit();
    return FALSE;
}

/**
 * Handler for username entered event. Hides pop-window and
 * shows main window
 */
static gboolean
on_username_entered (GtkWidget *confirm_button,
                     GdkEvent *event,
                     gpointer  pop_window)
{
    gtk_widget_destroy (GTK_WIDGET (pop_window));
    gtk_widget_show_all (window);

    return TRUE;
}

static gboolean
on_motion_event (GtkWidget *widget,
                 GdkEvent *event,
                 gpointer unused_data)
{
    GdkEventMotion* e = (GdkEventMotion*) event;
    cannon_rot = atan2 ((gint) e->y - Y0, (gint) e->x - X0);

    return TRUE;
}

/* This is handler for next frame. All magic happens here. It uses global static vars. */
static gboolean
on_next_frame (gpointer unused_data) {
    struct object_list *cur = bullets;
    gint target_cnt = 0;

    while (cur) {
        if (cur->skip) {
            cur = cur->next;
            continue;
        }
        cur->x += cur->vx;
        cur->y += cur->vy;
        cur->vx *= K;
        cur->vy *= K;
        cur->vy += AY;
        
        cairo_matrix_t *bul_transform = malloc (sizeof (cairo_matrix_t));
        cairo_matrix_init_identity (bul_transform);
        cairo_matrix_translate (bul_transform, cur->x, cur->y);
        goo_canvas_item_set_transform (cur->obj, bul_transform);
        free (bul_transform);

        if (cur->x < cur->r || cur->x > WIDTH - cur->r) {
            cur->vx *= -1;
        }
        if (cur->y < cur->r || cur->y > HEIGHT - cur->r) {
            cur->vy *= -1;
        }

        /* collisions */
        struct object_list *tar = targets;
        while (tar) {
            if (tar->skip) {
                tar = tar->next;
                continue;
            }
            if ((tar->x - cur->x) * (tar->x - cur->x) +
                    (tar->y - cur->y) * (tar->y - cur->y) <
                    (cur->r + tar->r) * (cur->r + tar->r)) {
                tar->skip = TRUE;
                cur->skip = TRUE;
                goo_canvas_item_remove(cur->obj);
                goo_canvas_item_remove(tar->obj);
                score += tar->score;
                break;
            }
            tar = tar->next;
        }
        cur = cur->next;
    }

    cur = targets;
    while (cur) {
        if (cur->skip) {
            cur = cur->next;
            continue;
        }
        target_cnt++;
        cur->x += cur->vx;
        cur->y += cur->vy;

        cairo_matrix_t *bul_transform = malloc (sizeof (cairo_matrix_t));
        cairo_matrix_init_identity (bul_transform);
        cairo_matrix_translate (bul_transform, cur->x, cur->y);
        goo_canvas_item_set_transform (cur->obj, bul_transform);
        free (bul_transform);

        if (cur->x < cur->r || cur->x > WIDTH - cur->r) {
            cur->vx *= -1;
        }
        if (cur->y < cur->r || cur->y > HEIGHT - cur->r) {
            cur->vy *= -1;
        }
        cur = cur->next;
    }
    if (target_cnt == 0) {
        gtk_main_quit ();
    }

    if (pressed) {
        struct timespec *cur_t = malloc (sizeof (struct timespec));
        clock_gettime (CLOCK_REALTIME, cur_t);
        cannon_charge = CHARGE_SPEED * ((cur_t->tv_sec - start_time->tv_sec) * 1000 + (cur_t->tv_nsec - start_time->tv_nsec) / 1000000);
        cannon_charge = min (cannon_charge, MAX_CANNON_CHARGE);
        free (cur_t);
    }

    cairo_matrix_t *gun_transform = malloc (sizeof (cairo_matrix_t));
    cairo_matrix_init_identity (gun_transform);
    cairo_matrix_translate (gun_transform, X0 + 0.0f * cannon_get_len (cannon_charge), Y0 + 0.5f * D);
    cairo_matrix_rotate (gun_transform, cannon_rot);
    cairo_matrix_scale (gun_transform, 1.0f * cannon_get_len (cannon_charge) / L0, 1.0f);
    cairo_matrix_translate (gun_transform, -0.0f * cannon_get_len (cannon_charge), -0.5f * D);
    cairo_matrix_translate (gun_transform, -X0, -Y0);

    goo_canvas_item_set_transform (cannon, gun_transform);

    free (gun_transform);

    return TRUE;
}

/* This returns random integer from [a, b) */
static gint
randint (gint a, gint b) {
    return a + (gint)(random() * (glong)(b - a) / RAND_MAX);
}

/* This returns length of cannon for given charge */
static gint
cannon_get_len (gint charge) {
    return L0 + LMAX * (gdouble) charge / (gdouble) MAX_CANNON_CHARGE;
}

/* This returns speed of target for given charge */
static gdouble
cannon_get_speed (gint charge) {
    return MAX_CANNON_SPEED * (gdouble) charge / MAX_CANNON_CHARGE;
}

/* This returns color of cannon for given charge */
static gchar *
cannon_get_color (gint charge) {
    return "#000000";
}
