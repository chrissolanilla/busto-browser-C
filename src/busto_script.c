#define _GNU_SOURCE

#include "../include/busto/script_runtime.h"

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

#ifdef __APPLE__
#define BUSTO_SCRIPT_COMPILER "clang"
#define BUSTO_SCRIPT_OUTPUT_TEMPLATE "/tmp/busto-script-XXXXXX.dylib"
#define BUSTO_SCRIPT_OUTPUT_SUFFIX_LEN 6
#define BUSTO_SCRIPT_SHARED_FLAG "-dynamiclib"
#else
#define BUSTO_SCRIPT_COMPILER "gcc"
#define BUSTO_SCRIPT_OUTPUT_TEMPLATE "/tmp/busto-script-XXXXXX.so"
#define BUSTO_SCRIPT_OUTPUT_SUFFIX_LEN 3
#define BUSTO_SCRIPT_SHARED_FLAG "-shared"
#endif

#define BUSTO_SCRIPT_TEXT_MAX 1024

enum busto_script_to_browser_type {
    BUSTO_SCRIPT_TO_BROWSER_SET_CONTENT,
    BUSTO_SCRIPT_TO_BROWSER_SET_TITLE,
    BUSTO_SCRIPT_TO_BROWSER_NAVIGATE,
    BUSTO_SCRIPT_TO_BROWSER_REQUEST_REDRAW,
    BUSTO_SCRIPT_TO_BROWSER_GRAPHICS_CLEAR,
    BUSTO_SCRIPT_TO_BROWSER_GRAPHICS_SET_FILL,
    BUSTO_SCRIPT_TO_BROWSER_GRAPHICS_FILL_RECT
};

enum busto_browser_to_script_type {
    BUSTO_BROWSER_TO_SCRIPT_UPDATE,
    BUSTO_BROWSER_TO_SCRIPT_KEY_PRESS,
    BUSTO_BROWSER_TO_SCRIPT_STOP
};

struct busto_script_to_browser_msg {
    int type;
    char text[BUSTO_SCRIPT_TEXT_MAX];
    double a;
    double b;
    double c;
    double d;
};

struct busto_browser_to_script_msg {
    int type;
    char key[64];
    double deltaT;
    int width;
    int height;
};

struct loaded_script {
    /* void *handle; */
    pid_t pid;
    int to_script_fd;
    int from_script_fd;
};

static struct loaded_script current_script = {
    /* 0 */
    .pid = -1,
    .to_script_fd = -1,
    .from_script_fd = -1
};

static int g_child_to_browser_fd = -1;
static int g_cached_width = 0;
static int g_cached_height = 0;

static void child_send_msg(struct busto_script_to_browser_msg *msg) {
    if(g_child_to_browser_fd < 0) {
        return;
    }
    write(g_child_to_browser_fd, msg, sizeof(*msg));
}

static void child_set_content(const char *text) {
    struct busto_script_to_browser_msg msg = {0};
    msg.type = BUSTO_SCRIPT_TO_BROWSER_SET_CONTENT;
    if(text) {
        snprintf(msg.text, sizeof(msg.text), "%s", text);
    }
    child_send_msg(&msg);
}

static void child_set_title(const char *text) {
    struct busto_script_to_browser_msg msg = {0};
    msg.type = BUSTO_SCRIPT_TO_BROWSER_SET_TITLE;
    if(text) {
        snprintf(msg.text, sizeof(msg.text), "%s", text);
    }
    child_send_msg(&msg);
}

static void child_navigate(const char *url) {
    struct busto_script_to_browser_msg msg = {0};
    msg.type = BUSTO_SCRIPT_TO_BROWSER_NAVIGATE;
    if(url) {
        snprintf(msg.text, sizeof(msg.text), "%s", url);
    }
    child_send_msg(&msg);
}

static void child_request_redraw(void) {
    struct busto_script_to_browser_msg msg = {0};
    msg.type = BUSTO_SCRIPT_TO_BROWSER_REQUEST_REDRAW;
    child_send_msg(&msg);
}

static int child_get_width(void) {
    return g_cached_width;
}

static int child_get_height(void) {
    return g_cached_height;
}

static void child_graphics_clear(void) {
    struct busto_script_to_browser_msg msg = {0};
    msg.type = BUSTO_SCRIPT_TO_BROWSER_GRAPHICS_CLEAR;
    child_send_msg(&msg);
}

static void child_graphics_set_fill(double r, double g, double b, double a) {
    struct busto_script_to_browser_msg msg = {0};
    msg.type = BUSTO_SCRIPT_TO_BROWSER_GRAPHICS_SET_FILL;
    msg.a = r;
    msg.b = g;
    msg.c = b;
    msg.d = a;
    child_send_msg(&msg);
}

static void child_graphics_fill_rect(double x, double y, double w, double h) {
    struct busto_script_to_browser_msg msg = {0};
    msg.type = BUSTO_SCRIPT_TO_BROWSER_GRAPHICS_FILL_RECT;
    msg.a = x;
    msg.b = y;
    msg.c = w;
    msg.d = h;
    child_send_msg(&msg);
}

static struct busto_api child_api = {
    .set_content = child_set_content,
    .set_title = child_set_title,
    .navigate = child_navigate,
    .request_redraw = child_request_redraw,
    .get_width = child_get_width,
    .get_height = child_get_height,
    .graphics = {
        .clear = child_graphics_clear,
        .set_fill = child_graphics_set_fill,
        .fill_rect = child_graphics_fill_rect
    }
};

static void send_browser_to_script(struct busto_browser_to_script_msg *msg) {
    if (current_script.to_script_fd < 0) {
        return;
    }

    write(current_script.to_script_fd, msg, sizeof(*msg));
}

void busto_script_update(double dt, int width, int height) {
    struct busto_browser_to_script_msg msg = {0};
    msg.type = BUSTO_BROWSER_TO_SCRIPT_UPDATE;
    msg.deltaT = dt;
    msg.width = width;
    msg.height = height;

    send_browser_to_script(&msg);
}


void busto_script_key_press(const char *key) {
    struct busto_browser_to_script_msg msg = {0};
    msg.type = BUSTO_BROWSER_TO_SCRIPT_KEY_PRESS;

    if (key) {
        snprintf(msg.key, sizeof(msg.key), "%s", key);
    }

    send_browser_to_script(&msg);
}

static void run_script_child(const char *so_path, int from_browser_fd, int to_browser_fd) {
    g_child_to_browser_fd = to_browser_fd;
    void *handle = dlopen(so_path, RTLD_NOW | RTLD_LOCAL);
    if(!handle) {
        fprintf(stderr, "[busto-script] child lost his parents and became an orphan: %s\n", dlerror());
        _exit(1);
    }

    busto_script_start_fn start_fn = (busto_script_start_fn)dlsym(handle, "busto_start");
    busto_script_update_fn update_fn = (busto_script_update_fn)dlsym(handle, "busto_update");
    busto_script_on_key_press_fn key_press_fn = (busto_script_on_key_press_fn)dlsym(handle, "busto_on_key_press");
    busto_script_stop_fn stop_fn = (busto_script_stop_fn)dlsym(handle, "busto_stop");
    busto_script_main_fn main_fn = (busto_script_main_fn)dlsym(handle, "busto_main");

    if(start_fn) {
        start_fn(&child_api);
    }
    else if(main_fn) {
        main_fn(&child_api);
    }

    for(;;) {
        struct busto_browser_to_script_msg msg;
        ssize_t n = read(from_browser_fd, &msg, sizeof(msg));

        if(n == 0) {
            break;
        }
        if(n != sizeof(msg)) {
            continue;
        }

        g_cached_width = msg.width;
        g_cached_height = msg.height;

        switch (msg.type) {
            case BUSTO_BROWSER_TO_SCRIPT_UPDATE:
                if(update_fn) {
                    update_fn(&child_api, msg.deltaT);
                }
                break;

            case BUSTO_BROWSER_TO_SCRIPT_KEY_PRESS:
                if(key_press_fn) {
                    key_press_fn(&child_api, msg.key);
                }
                break;

            case BUSTO_BROWSER_TO_SCRIPT_STOP:
                if(stop_fn) {
                    stop_fn(&child_api);
                }
                _exit(0);
        }
    }

    if(stop_fn) {
        stop_fn(&child_api);
    }
    _exit(0);
}

void busto_script_pump_browser_messages(struct busto_api *browser_api) {
    if (current_script.from_script_fd < 0 || !browser_api) {
        return;
    }

    for (;;) {
        struct busto_script_to_browser_msg msg;
        ssize_t n = read(current_script.from_script_fd, &msg, sizeof(msg));
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }

            perror("[busto-script] read script message");
            break;
        }

        if (n == 0) {
            break;
        }

        if (n != sizeof(msg)) {
            continue;
        }

        switch (msg.type) {
            case BUSTO_SCRIPT_TO_BROWSER_SET_CONTENT:
                browser_api->set_content(msg.text);
                break;

            case BUSTO_SCRIPT_TO_BROWSER_SET_TITLE:
                browser_api->set_title(msg.text);
                break;

            case BUSTO_SCRIPT_TO_BROWSER_NAVIGATE:
                browser_api->navigate(msg.text);
                break;

            case BUSTO_SCRIPT_TO_BROWSER_REQUEST_REDRAW:
                browser_api->request_redraw();
                break;

            case BUSTO_SCRIPT_TO_BROWSER_GRAPHICS_CLEAR:
                browser_api->graphics.clear();
                break;

            case BUSTO_SCRIPT_TO_BROWSER_GRAPHICS_SET_FILL:
                browser_api->graphics.set_fill(msg.a, msg.b, msg.c, msg.d);
                break;

            case BUSTO_SCRIPT_TO_BROWSER_GRAPHICS_FILL_RECT:
                browser_api->graphics.fill_rect(msg.a, msg.b, msg.c, msg.d);
                break;
        }
    }
}


void busto_script_unload(void) {
    if (current_script.to_script_fd >= 0) {
        struct busto_browser_to_script_msg msg = {0};
        msg.type = BUSTO_BROWSER_TO_SCRIPT_STOP;
        write(current_script.to_script_fd, &msg, sizeof(msg));
    }

    if (current_script.pid > 0) {
        int status = 0;
        if (waitpid(current_script.pid, &status, WNOHANG) == 0) {
            kill(current_script.pid, SIGTERM);
            usleep(20 * 1000);

            if (waitpid(current_script.pid, &status, WNOHANG) == 0) {
                kill(current_script.pid, SIGKILL);
                waitpid(current_script.pid, &status, 0);
            }
        }
    }

    if (current_script.to_script_fd >= 0) {
        close(current_script.to_script_fd);
    }

    if (current_script.from_script_fd >= 0) {
        close(current_script.from_script_fd);
    }

    current_script.pid = -1;
    current_script.to_script_fd = -1;
    current_script.from_script_fd = -1;
}

/* int busto_script_compile_and_run( */
/*     const char *source, */
/*     struct busto_api *api */
/* ) */
/* { */
/*     if (!source || !api) { */
/*         fprintf(stderr, "[busto-script] missing source or api\n"); */
/*         return -1; */
/*     } */
/**/
/*     char c_path[] = "/tmp/busto-script-XXXXXX.c"; */
/*     int fd = mkstemps(c_path, 2); */
/**/
/*     if (fd < 0) { */
/*         perror("[busto-script] mkstemps source"); */
/*         return -1; */
/*     } */
/**/
/*     FILE *file = fdopen(fd, "w"); */
/**/
/*     if (!file) { */
/*         perror("[busto-script] fdopen source"); */
/*         close(fd); */
/*         unlink(c_path); */
/*         return -1; */
/*     } */
/**/
/*     if (fputs(source, file) == EOF) { */
/*         perror("[busto-script] write source"); */
/*         fclose(file); */
/*         unlink(c_path); */
/*         return -1; */
/*     } */
/**/
/*     if (fclose(file) != 0) { */
/*         perror("[busto-script] close source"); */
/*         unlink(c_path); */
/*         return -1; */
/*     } */
/**/
/*     char so_path[] = BUSTO_SCRIPT_OUTPUT_TEMPLATE; */
/*     int so_fd = mkstemps(so_path, BUSTO_SCRIPT_OUTPUT_SUFFIX_LEN); */
/**/
/*     if (so_fd < 0) { */
/*         perror("[busto-script] mkstemps shared object"); */
/*         unlink(c_path); */
/*         return -1; */
/*     } */
/**/
/*     if (close(so_fd) != 0) { */
/*         perror("[busto-script] close shared object temp"); */
/*         unlink(c_path); */
/*         unlink(so_path); */
/*         return -1; */
/*     } */
/**/
/*     pid_t pid = fork(); */
/**/
/*     if (pid < 0) { */
/*         perror("[busto-script] fork compiler"); */
/*         unlink(c_path); */
/*         unlink(so_path); */
/*         return -1; */
/*     } */
/**/
/*     if (pid == 0) { */
/*         execlp( */
/*             BUSTO_SCRIPT_COMPILER, */
/*             BUSTO_SCRIPT_COMPILER, */
/*             BUSTO_SCRIPT_SHARED_FLAG, */
/*             "-fPIC", */
/*             "-I./include", */
/*             c_path, */
/*             "-o", */
/*             so_path, */
/*             (char *)NULL */
/*         ); */
/*         perror("[busto-script] exec compiler"); */
/*         _exit(127); */
/*     } */
/**/
/*     int status = 0; */
/**/
/*     if (waitpid(pid, &status, 0) < 0) { */
/*         perror("[busto-script] wait compiler"); */
/*         unlink(c_path); */
/*         unlink(so_path); */
/*         return -1; */
/*     } */
/**/
/*     if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) { */
/*         fprintf(stderr, "[busto-script] compilation failed\n"); */
/*         unlink(c_path); */
/*         unlink(so_path); */
/*         return -1; */
/*     } */
/**/
/*     unlink(c_path); */
/**/
/*     void *handle = dlopen(so_path, RTLD_NOW | RTLD_LOCAL); */
/**/
/*     if (!handle) { */
/*         fprintf( */
/*             stderr, */
/*             "[busto-script] dlopen: %s\n", */
/*             dlerror() */
/*         ); */
/**/
/*         unlink(so_path); */
/*         return -1; */
/*     } */
/**/
/*     unlink(so_path); */
/**/
/*     dlerror(); */
/**/
/*     busto_script_main_fn main_fn = */
/*         (busto_script_main_fn)dlsym( */
/*             handle, */
/*             "busto_main" */
/*         ); */
/**/
/*     const char *error = dlerror(); */
/**/
/*     if (error) { */
/*         fprintf( */
/*             stderr, */
/*             "[busto-script] dlsym: %s\n", */
/*             error */
/*         ); */
/**/
/*         dlclose(handle); */
/**/
/*         return -1; */
/*     } */
/**/
/*     busto_script_unload(); */
/*     current_script.handle = handle; */
/**/
/*     main_fn(api); */
/**/
/*     return 0; */
/* } */


int busto_script_compile_and_start(const char *source)
{
    if (!source) {
        fprintf(stderr, "[busto-script] missing source\n");
        return -1;
    }

    char c_path[] = "/tmp/busto-script-XXXXXX.c";
    int fd = mkstemps(c_path, 2);

    if (fd < 0) {
        perror("[busto-script] mkstemps source");
        return -1;
    }

    FILE *file = fdopen(fd, "w");
    if (!file) {
        perror("[busto-script] fdopen source");
        close(fd);
        unlink(c_path);
        return -1;
    }

    if (fputs(source, file) == EOF) {
        perror("[busto-script] write source");
        fclose(file);
        unlink(c_path);
        return -1;
    }

    if (fclose(file) != 0) {
        perror("[busto-script] close source");
        unlink(c_path);
        return -1;
    }

    char so_path[] = BUSTO_SCRIPT_OUTPUT_TEMPLATE;
    int so_fd = mkstemps(so_path, BUSTO_SCRIPT_OUTPUT_SUFFIX_LEN);

    if (so_fd < 0) {
        perror("[busto-script] mkstemps shared object");
        unlink(c_path);
        return -1;
    }

    close(so_fd);

    pid_t compiler_pid = fork();

    if (compiler_pid < 0) {
        perror("[busto-script] fork compiler");
        unlink(c_path);
        unlink(so_path);
        return -1;
    }

    if (compiler_pid == 0) {
        execlp(
            BUSTO_SCRIPT_COMPILER,
            BUSTO_SCRIPT_COMPILER,
            BUSTO_SCRIPT_SHARED_FLAG,
            "-fPIC",
            "-I./include",
            c_path,
            "-o",
            so_path,
            (char *)NULL
        );

        perror("[busto-script] exec compiler");
        _exit(127);
    }

    int status = 0;

    if (waitpid(compiler_pid, &status, 0) < 0) {
        perror("[busto-script] wait compiler");
        unlink(c_path);
        unlink(so_path);
        return -1;
    }

    unlink(c_path);

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "[busto-script] compilation failed\n");
        unlink(so_path);
        return -1;
    }

    int to_script[2];
    int from_script[2];

    if (pipe(to_script) != 0) {
        perror("[busto-script] pipe to_script");
        unlink(so_path);
        return -1;
    }

    if (pipe(from_script) != 0) {
        perror("[busto-script] pipe from_script");
        close(to_script[0]);
        close(to_script[1]);
        unlink(so_path);
        return -1;
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror("[busto-script] fork script");
        close(to_script[0]);
        close(to_script[1]);
        close(from_script[0]);
        close(from_script[1]);
        unlink(so_path);
        return -1;
    }

    if (pid == 0) {
        close(to_script[1]);
        close(from_script[0]);

        run_script_child(so_path, to_script[0], from_script[1]);

        _exit(1);
    }

    close(to_script[0]);
    close(from_script[1]);

    busto_script_unload();

    current_script.pid = pid;
    current_script.to_script_fd = to_script[1];
    current_script.from_script_fd = from_script[0];

    fcntl(current_script.from_script_fd, F_SETFL, O_NONBLOCK);

    /*
       Do not unlink so_path here yet unless you have a child-ready handshake.
       Otherwise parent can delete it before child dlopen() runs.
    */

    return 0;
}
/* int busto_script_compile_and_start(const char *source) { */
/*     int to_script[2]; */
/*     int from_script[2]; */
/*     char so_path[] = BUSTO_SCRIPT_OUTPUT_TEMPLATE; */


/*     if (pipe(to_script) != 0) { */
/*         perror("[busto-script] pipe to_script"); */
/*         return -1; */
/*     } */

/*     if (pipe(from_script) != 0) { */
/*         perror("[busto-script] pipe from_script"); */
/*         close(to_script[0]); */
/*         close(to_script[1]); */
/*         return -1; */
/*     } */

/*     pid_t pid = fork(); */

/*     if (pid < 0) { */
/*         perror("[busto-script] fork script"); */
/*         close(to_script[0]); */
/*         close(to_script[1]); */
/*         close(from_script[0]); */
/*         close(from_script[1]); */
/*         return -1; */
/*     } */

/*     if (pid == 0) { */
/*         close(to_script[1]); */
/*         close(from_script[0]); */

/*         run_script_child(so_path, to_script[0], from_script[1]); */

/*         _exit(1); */
/*     } */

/*     close(to_script[0]); */
/*     close(from_script[1]); */

/*     busto_script_unload(); */

/*     current_script.pid = pid; */
/*     current_script.to_script_fd = to_script[1]; */
/*     current_script.from_script_fd = from_script[0]; */

/*     fcntl(current_script.from_script_fd, F_SETFL, O_NONBLOCK); */

/*     unlink(so_path); */

/*     return 0; */
/* } */
