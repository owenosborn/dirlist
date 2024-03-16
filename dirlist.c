#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#include "m_pd.h"

static t_class *dirlist_class;

// a list of dirnames
typedef struct {
    size_t count;
    char **dirnames;
    int next_numbered_name;  // the highest numbered dir (e.g. 124) +1, used to name next dir
} dirnames;

typedef struct _dirlist {
    t_object  x_obj;
    t_outlet *outlet;
    dirnames dirnames;
} t_dirlist;

// helpers //

// add a dirname to the list
static void append(dirnames *p, const char *path) {
    p->dirnames = realloc(p->dirnames, sizeof(char*) * (p->count + 1));
    p->dirnames[p->count] = strdup(path);
    p->count++;
}

// for sorting 
static int compare(const void *a, const void *b) {
//    return strverscmp(*(const char **)a, *(const char **)b);
    return strcmp(*(const char **)a, *(const char **)b);
}

// clear the dirname list
static void clear_dirnames(t_dirlist *x) {
    for (size_t i = 0; i < x->dirnames.count; i++) {
        free(x->dirnames.dirnames[i]);
    }
    free(x->dirnames.dirnames);
    x->dirnames.count = 0;
    x->dirnames.dirnames = NULL;
    x->dirnames.next_numbered_name = 0;
}

// add all dirs at directory path to the list
static int scan_for_dirs(t_dirlist *x, const char *path) {
    DIR *d;
    struct dirent *dir;
    struct stat path_stat;
    char fullpath[1024];

    clear_dirnames(x);

    d = opendir(path);

    if(d) {
        // loop over directory
        while ((dir = readdir(d)) != NULL && x->dirnames.count < 10000) {
            
            // ignore anything starting with '.'
            if (dir->d_name[0] == '.') continue;
            
            // check that it is a directory 
            snprintf(fullpath, sizeof(fullpath), "%s/%s", path, dir->d_name);
            stat(fullpath, &path_stat);
            if(!S_ISDIR(path_stat.st_mode)) continue;
            
            // good to go
            append(&(x->dirnames), dir->d_name);

            // if the dirname is a number, we want to know the biggest one
            char *end;
            long num = strtol(dir->d_name, &end, 10);
            // if it is a number and bigger than the current max
            if (end != dir->d_name && *end == '\0' && num <= 9999 && num >= 0) {
                if (num > x->dirnames.next_numbered_name) {
                    x->dirnames.next_numbered_name = num;
                }
            }
        }
        closedir(d);
    }

    qsort(x->dirnames.dirnames, x->dirnames.count, sizeof(char *), compare);
    x->dirnames.next_numbered_name += 1;

    /*
    for (size_t i = 0; i < x->dirnames.count; i++) {
        post("%s", x->dirnames.dirnames[i]);
    }
    post("max: %d", x->dirnames.next_numbered_name);
    post("number files: %d", x->dirnames.count);
    */

    return 0;
}

// pd class //

// make list of dirs in the specified dir
static void dirlist_scan(t_dirlist *x, t_symbol *s) {
    t_atom atom[1];

    scan_for_dirs(x, s->s_name);

    SETFLOAT(atom, x->dirnames.count);
    outlet_anything(x->outlet, gensym("total_dirs"), 1, atom);
    
    SETFLOAT(atom, x->dirnames.next_numbered_name);
    outlet_anything(x->outlet, gensym("next_name"), 1, atom);
}

// index the list
static void dirlist_float(t_dirlist *x, t_floatarg f) {
    t_atom atom[1];
   
    if (x->dirnames.count <= 0) {post("no dirs");return;}
    unsigned int i = (int)f;
    if (i < 0) i = 0;
    if (i >= x->dirnames.count) i = x->dirnames.count - 1;

    t_symbol* mySymbol = gensym(x->dirnames.dirnames[i]);
    
    SETSYMBOL(atom, mySymbol);
    outlet_anything(x->outlet, gensym("dirname"), 1, atom);
}

// add file to the index
static void dirlist_add(t_dirlist *x, t_symbol *s) {
    append(&(x->dirnames), s->s_name);

    x->dirnames.next_numbered_name += 1;
 
    t_atom atom[1];

    SETFLOAT(atom, x->dirnames.count);
    outlet_anything(x->outlet, gensym("total_dirs"), 1, atom);
    
    SETFLOAT(atom, x->dirnames.next_numbered_name);
    outlet_anything(x->outlet, gensym("next_name"), 1, atom);
}

/*
static void dirlist_next(t_dirlist *x){}

static void dirlist_previous(t_dirlist *x){}

static void dirlist_clear(t_dirlist *x){}
*/

static void *dirlist_new(){
    t_dirlist *x = (t_dirlist *)pd_new(dirlist_class);
    x->outlet = outlet_new(&x->x_obj, &s_list);

    x->dirnames.count = 0;
    x->dirnames.dirnames = NULL;
    x->dirnames.next_numbered_name = 0;

    return (void *)x;
}

static void dirlist_free(t_dirlist *x) {
    //post("freeing...");
 
    for (size_t i = 0; i < x->dirnames.count; i++) {
        free(x->dirnames.dirnames[i]);
    }
    free(x->dirnames.dirnames);

    outlet_free(x->outlet);
}

void dirlist_setup(void) {
    dirlist_class = class_new(gensym("dirlist"), (t_newmethod)dirlist_new, (t_method)dirlist_free, sizeof(t_dirlist), 0, 0);
    class_addmethod(dirlist_class, (t_method)dirlist_scan, gensym("scan"), A_SYMBOL, 0);
    class_addmethod(dirlist_class, (t_method)dirlist_float, gensym("float"), A_FLOAT, 0);
    class_addmethod(dirlist_class, (t_method)dirlist_add, gensym("add"), A_SYMBOL, 0);
    //class_addmethod(dirlist_class, (t_method)dirlist_next, gensym("next"), 0);
    //class_addmethod(dirlist_class, (t_method)dirlist_previous, gensym("previous"), 0);
    //class_addmethod(dirlist_class, (t_method)dirlist_clear, gensym("clear"), 0);
}

