#define _GNU_SOURCE

#include "../include/busto/script_runtime.h"

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

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

struct loaded_script {
    void *handle;
};

static struct loaded_script current_script = {0};

void busto_script_unload(void)
{
    if (current_script.handle) {
        dlclose(current_script.handle);
        current_script.handle = NULL;
    }
}

int busto_script_compile_and_run(
    const char *source,
    struct busto_api *api
)
{
    if (!source || !api) {
        fprintf(stderr, "[busto-script] missing source or api\n");
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

    if (close(so_fd) != 0) {
        perror("[busto-script] close shared object temp");
        unlink(c_path);
        unlink(so_path);
        return -1;
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror("[busto-script] fork compiler");
        unlink(c_path);
        unlink(so_path);
        return -1;
    }

    if (pid == 0) {
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

    if (waitpid(pid, &status, 0) < 0) {
        perror("[busto-script] wait compiler");
        unlink(c_path);
        unlink(so_path);
        return -1;
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "[busto-script] compilation failed\n");
        unlink(c_path);
        unlink(so_path);
        return -1;
    }

    unlink(c_path);

    void *handle = dlopen(so_path, RTLD_NOW | RTLD_LOCAL);

    if (!handle) {
        fprintf(
            stderr,
            "[busto-script] dlopen: %s\n",
            dlerror()
        );

        unlink(so_path);
        return -1;
    }

    unlink(so_path);

    dlerror();

    busto_script_main_fn main_fn =
        (busto_script_main_fn)dlsym(
            handle,
            "busto_main"
        );

    const char *error = dlerror();

    if (error) {
        fprintf(
            stderr,
            "[busto-script] dlsym: %s\n",
            error
        );

        dlclose(handle);

        return -1;
    }

    busto_script_unload();
    current_script.handle = handle;

    main_fn(api);

    return 0;
}
