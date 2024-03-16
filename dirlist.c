#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#include "m_pd.h"

static t_class *dirlist_class;

// a list of filenames
typedef struct {
    size_t count;
    char **filenames;
    int next_numbered_name;  // the highest numbered file (e.g. 124.wav) +1, used to name next file
} Filenames;

typedef struct _dirlist {
    t_object  x_obj;
    t_outlet *outlet;
    Filenames filenames;
} t_dirlist;

// helpers //

// add a filename to the list
static void append(Filenames *p, const char *path) {
    p->filenames = realloc(p->filenames, sizeof(char*) * (p->count + 1));
    p->filenames[p->count] = strdup(path);
    p->count++;
}

// check if file is .wav or .WAV
static int ends_with_wav(const char *str) {
    if(str == NULL) return 0;

    size_t len = strlen(str);
    if(len < 4) return 0;

    const char *extension = str + len - 4;
    return strcmp(extension, ".wav") == 0 || strcmp(extension, ".WAV") == 0;
}

// for sorting 
static int compare(const void *a, const void *b) {
//    return strverscmp(*(const char **)a, *(const char **)b);
    return strcmp(*(const char **)a, *(const char **)b);
}

// clear the filename list
static void clear_filenames(t_dirlist *x) {
    for (size_t i = 0; i < x->filenames.count; i++) {
        free(x->filenames.filenames[i]);
    }
    free(x->filenames.filenames);
    x->filenames.count = 0;
    x->filenames.filenames = NULL;
    x->filenames.next_numbered_name = 0;
}

// add all wav files at directory path to the list
static int scan_for_wavs(t_dirlist *x, const char *path) {
    DIR *d;
    struct dirent *dir;
    struct stat path_stat;
    char fullpath[1024];

    clear_filenames(x);

    d = opendir(path);

    if(d) {
        // loop over directory
        while ((dir = readdir(d)) != NULL && x->filenames.count < 10000) {
            
            // ignore anything starting with '.'
            if (dir->d_name[0] == '.') continue;
            
            // check that it is a file 
            snprintf(fullpath, sizeof(fullpath), "%s/%s", path, dir->d_name);
            stat(fullpath, &path_stat);
            if(!S_ISREG(path_stat.st_mode)) continue;
            
            // check that it ends in '.wav' or '.WAV'
            if(!ends_with_wav(dir->d_name)) continue;

            // good to go
            append(&(x->filenames), dir->d_name);

            // if the filename is a number, we want to know the biggest one
            char name_no_ext[strlen(dir->d_name) - 4 + 1];  // +1 for the null-terminator
            memcpy(name_no_ext, dir->d_name, strlen(dir->d_name) - 4);
            name_no_ext[strlen(dir->d_name) - 4] = '\0';
            char *end;
            long num = strtol(name_no_ext, &end, 10);
            // if it is a number and bigger than the current max
            if (end != name_no_ext && *end == '\0' && num <= 9999 && num >= 0) {
                if (num > x->filenames.next_numbered_name) {
                    x->filenames.next_numbered_name = num;
                }
            }
        }
        closedir(d);
    }

    qsort(x->filenames.filenames, x->filenames.count, sizeof(char *), compare);
    x->filenames.next_numbered_name += 1;

    /*
    for (size_t i = 0; i < x->filenames.count; i++) {
        post("%s", x->filenames.filenames[i]);
    }
    post("max: %d", x->filenames.next_numbered_name);
    post("number files: %d", x->filenames.count);
    */

    return 0;
}

// pd class //

// make list of wav files in the specified folder
static void dirlist_scan(t_dirlist *x, t_symbol *s) {
    t_atom atom[1];

    scan_for_wavs(x, s->s_name);

    SETFLOAT(atom, x->filenames.count);
    outlet_anything(x->outlet, gensym("total_files"), 1, atom);
    
    SETFLOAT(atom, x->filenames.next_numbered_name);
    outlet_anything(x->outlet, gensym("next_name"), 1, atom);
}

// index the list
static void dirlist_float(t_dirlist *x, t_floatarg f) {
    t_atom atom[1];
   
    if (x->filenames.count <= 0) {post("no files");return;}
    unsigned int i = (int)f;
    if (i < 0) i = 0;
    if (i >= x->filenames.count) i = x->filenames.count - 1;

    t_symbol* mySymbol = gensym(x->filenames.filenames[i]);
    
    SETSYMBOL(atom, mySymbol);
    outlet_anything(x->outlet, gensym("filename"), 1, atom);
}

// add file to the index
static void dirlist_add(t_dirlist *x, t_symbol *s) {
    append(&(x->filenames), s->s_name);

    x->filenames.next_numbered_name += 1;
 
    t_atom atom[1];

    SETFLOAT(atom, x->filenames.count);
    outlet_anything(x->outlet, gensym("total_files"), 1, atom);
    
    SETFLOAT(atom, x->filenames.next_numbered_name);
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

    x->filenames.count = 0;
    x->filenames.filenames = NULL;
    x->filenames.next_numbered_name = 0;

    return (void *)x;
}

static void dirlist_free(t_dirlist *x) {
    //post("freeing...");
 
    for (size_t i = 0; i < x->filenames.count; i++) {
        free(x->filenames.filenames[i]);
    }
    free(x->filenames.filenames);

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

