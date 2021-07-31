/*#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <jni.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include "../../../../../../Android/Sdk/ndk/21.4.7075529/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include/string.h"

#include "android/log.h"
#include "../../../../../../.local/share/python-for-android/build/other_builds/python3/arm64-v8a__ndk_target_21/python3/Include/Python.h"

#define ENTRYPOINT_MAXLEN 128
#define LOG(n, x) __android_log_write(ANDROID_LOG_INFO, (n), (x))
#define LOGP(x) LOG("python", (x))

static PyObject *androidembed_log(PyObject *self, PyObject *args) {
    char *logstr = NULL;
    if (!PyArg_ParseTuple(args, "s", &logstr)) {
        return NULL;
    }
    LOG(getenv("PYTHON_NAME"), logstr);
    Py_RETURN_NONE;
}

static PyMethodDef AndroidEmbedMethods[] = {
        {"log", androidembed_log, METH_VARARGS, "Log on android platform"},
        {NULL, NULL, 0, NULL}};


static struct PyModuleDef androidembed = {PyModuleDef_HEAD_INIT, "androidembed",
                                          "", -1, AndroidEmbedMethods};

PyMODINIT_FUNC initandroidembed(void) {
    return PyModule_Create(&androidembed);
}

int dir_exists(char *filename) {
    struct stat st;
    if (stat(filename, &st) == 0) {
        if (S_ISDIR(st.st_mode))
            return 1;
    }
    return 0;
}

int file_exists(const char *filename) {
    FILE *file;
    if ((file = fopen(filename, "r"))) {
        fclose(file);
        return 1;
    }
    return 0;
}*/
/*
*//* int main(int argc, char **argv) { *//*
int main(int argc, char *argv[]) {

    char *env_argument = NULL;
    char *env_entrypoint = NULL;
    char *env_logname = NULL;
    char entrypoint[ENTRYPOINT_MAXLEN];
    int ret = 0;
    FILE *fd;

    LOGP("Initializing Python for Android");

    // Set a couple of built-in environment vars:
    setenv("P4A_BOOTSTRAP", "SDL2", 1);  // env var to identify p4a to applications
    env_argument = getenv("ANDROID_ARGUMENT");
    setenv("ANDROID_APP_PATH", env_argument, 1);
    env_entrypoint = getenv("ANDROID_ENTRYPOINT");
    env_logname = getenv("PYTHON_NAME");
    if (!getenv("ANDROID_UNPACK")) {
        *//* ANDROID_UNPACK currently isn't set in services *//*
        setenv("ANDROID_UNPACK", env_argument, 1);
    }
    if (env_logname == NULL) {
        env_logname = "python";
        setenv("PYTHON_NAME", "python", 1);
    }

    // Set additional file-provided environment vars:
    LOGP("Setting additional env vars from p4a_env_vars.txt");
    char env_file_path[256];
    snprintf(env_file_path, sizeof(env_file_path),
             "%s/p4a_env_vars.txt", getenv("ANDROID_UNPACK"));
    FILE *env_file_fd = fopen(env_file_path, "r");
    if (env_file_fd) {
        char *line = NULL;
        size_t len = 0;
        while (getline(&line, &len, env_file_fd) != -1) {
            if (strlen(line) > 0) {
                char *eqsubstr = strstr(line, "=");
                if (eqsubstr) {
                    size_t eq_pos = eqsubstr - line;

                    // Extract name:
                    char env_name[256];
                    strncpy(env_name, line, sizeof(env_name));
                    env_name[eq_pos] = '\0';

                    // Extract value (with line break removed:
                    char env_value[256];
                    strncpy(env_value, (char *) (line + eq_pos + 1), sizeof(env_value));
                    if (strlen(env_value) > 0 &&
                        env_value[strlen(env_value) - 1] == '\n') {
                        env_value[strlen(env_value) - 1] = '\0';
                        if (strlen(env_value) > 0 &&
                            env_value[strlen(env_value) - 1] == '\r') {
                            // Also remove windows line breaks (\r\n)
                            env_value[strlen(env_value) - 1] = '\0';
                        }
                    }

                    // Set value:
                    setenv(env_name, env_value, 1);
                }
            }
        }
        fclose(env_file_fd);
    } else {
        LOGP("Warning: no p4a_env_vars.txt found / failed to open!");
    }

    LOGP("Changing directory to the one provided by ANDROID_ARGUMENT");
    LOGP(env_argument);
    chdir(env_argument);

    Py_SetProgramName(L"android_python");

    *//* our logging module for android
     *//*
    PyImport_AppendInittab("androidembed", initandroidembed);

    LOGP("Preparing to initialize python");

    // Set up the python path
    char paths[256];

    char python_bundle_dir[256];
    snprintf(python_bundle_dir, 256,
             "%s/_python_bundle", getenv("ANDROID_UNPACK"));
    if (dir_exists(python_bundle_dir)) {
        LOGP("_python_bundle dir exists");
        snprintf(paths, 256,
                 "%s/stdlib.zip:%s/modules",
                 python_bundle_dir, python_bundle_dir);

        LOGP("calculated paths to be...");
        LOGP(paths);

        wchar_t *wchar_paths = Py_DecodeLocale(paths, NULL);
        Py_SetPath(wchar_paths);

        LOGP("set wchar paths...");
    } else {
        LOGP("_python_bundle does not exist...this not looks good, all python"
             " recipes should have this folder, should we expect a crash soon?");
    }

    Py_Initialize();
    LOGP("Initialized python");

    *//* ensure threads will work.
     *//*
    LOGP("AND: Init threads");
    PyEval_InitThreads();

    PyRun_SimpleString("import androidembed\nandroidembed.log('testing python "
                       "print redirection')");

    *//* inject our bootstrap code to redirect python stdin/stdout
     * replace sys.path with our path
     *//*
    PyRun_SimpleString("import sys, posix\n");

    char add_site_packages_dir[256];

    if (dir_exists(python_bundle_dir)) {
        snprintf(add_site_packages_dir, 256,
                 "sys.path.append('%s/site-packages')",
                 python_bundle_dir);

        PyRun_SimpleString("import sys\n"
                           "sys.argv = ['notaninterpreterreally']\n"
                           "from os.path import realpath, join, dirname");
        PyRun_SimpleString(add_site_packages_dir);
        *//* "sys.path.append(join(dirname(realpath(__file__)), 'site-packages'))") *//*
        PyRun_SimpleString("sys.path = ['.'] + sys.path");
    }

*//*    PyRun_SimpleString(
            "class LogFile(object):\n"
            "    def __init__(self):\n"
            "        self.__buffer = ''\n"
            "    def write(self, s):\n"
            "        s = self.__buffer + s\n"
            "        lines = s.split('\\n')\n"
            "        for l in lines[:-1]:\n"
            "            androidembed.log(l.replace('\\x00', ''))\n"
            "        self.__buffer = lines[-1]\n"
            "    def flush(self):\n"
            "        return\n"
            "sys.stdout = sys.stderr = LogFile()\n"
            "print('Android path', sys.path)\n"
            "import os\n"
            "print('os.environ is', os.environ)\n"
            "print('Android kivy bootstrap done. __name__ is', __name__)");*//*

    LOGP("AND: Ran string");

    *//* run it !
     *//*
    LOGP("Run user program, change dir and execute entrypoint");

    *//* Get the entrypoint, search the .pyo then .py
     *//*
    char *dot = strrchr(env_entrypoint, '.');
    char *ext = ".pyc";
    if (dot <= 0) {
        LOGP("Invalid entrypoint, abort.");
        return -1;
    }
    if (strlen(env_entrypoint) > ENTRYPOINT_MAXLEN - 2) {
        LOGP("Entrypoint path is too long, try increasing ENTRYPOINT_MAXLEN.");
        return -1;
    }
    if (!strcmp(dot, ext)) {
        if (!file_exists(env_entrypoint)) {
            *//* fallback on .py *//*
            strcpy(entrypoint, env_entrypoint);
            entrypoint[strlen(env_entrypoint) - 1] = '\0';
            LOGP(entrypoint);
            if (!file_exists(entrypoint)) {
                LOGP("Entrypoint not found (.pyc, fallback on .py), abort");
                return -1;
            }
        } else {
            strcpy(entrypoint, env_entrypoint);
        }
    } else if (!strcmp(dot, ".py")) {
        *//* if .py is passed, check the pyo version first *//*
        strcpy(entrypoint, env_entrypoint);
        entrypoint[strlen(env_entrypoint) + 1] = '\0';
        entrypoint[strlen(env_entrypoint)] = 'c';
        if (!file_exists(entrypoint)) {
            *//* fallback on pure python version *//*
            if (!file_exists(env_entrypoint)) {
                LOGP("Entrypoint not found (.py), abort.");
                return -1;
            }
            strcpy(entrypoint, env_entrypoint);
        }
    } else {
        LOGP("Entrypoint have an invalid extension (must be .py or .pyc), abort.");
        return -1;
    }
    // LOGP("Entrypoint is:");
    // LOGP(entrypoint);
    fd = fopen(entrypoint, "r");
    if (fd == NULL) {
        LOGP("Open the entrypoint failed");
        LOGP(entrypoint);
        return -1;
    }

    *//* run python !
     *//*
    ret = PyRun_SimpleFile(fd, entrypoint);
    fclose(fd);

    if (PyErr_Occurred() != NULL) {
        ret = 1;
        PyErr_Print(); *//* This exits with the right code if SystemExit. *//*
        PyObject *f = PySys_GetObject("stdout");
        if (PyFile_WriteString("\n", f))
            PyErr_Clear();
    }

    LOGP("Python for android ended.");

    *//* Shut down: since regular shutdown causes issues sometimes
       (seems to be an incomplete shutdown breaking next launch)
       we'll use sys.exit(ret) to shutdown, since that one works.

       Reference discussion:

       https://github.com/kivy/kivy/pull/6107#issue-246120816
     *//*
    char terminatecmd[256];
    snprintf(
            terminatecmd, sizeof(terminatecmd),
            "import sys; sys.exit(%d)\n", ret
    );
    PyRun_SimpleString(terminatecmd);

    *//* This should never actually be reached, but we'll leave the clean-up
     * here just to be safe.
     *//*
    if (Py_FinalizeEx() != 0)  // properly check success on Python 3
        LOGP("Unexpectedly reached Py_FinalizeEx(), and got error!");
    else
        LOGP("Unexpectedly reached Py_FinalizeEx(), but was successful.");

    return ret;
}*/

#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <dlfcn.h>
#include <wchar.h>
#include <locale.h>
#include <stdlib.h>

#define PYTHON3_STDLIB_REL_PATH "stdlib.zip"
#define PYTHON3_MODULES_REL_PATH "modules"
#define PYTHON3_DLL_REL_PATH "libpython3.8.so"

#define SYS_PATH_BUFFER_SIZE (2*(PATH_MAX + 1))
#define ENTRYPOINT_MAXLEN 128

static char NULL_PTR_STR[] = "NULL";

static void GetExecutablePath(char *path) {
    int size = readlink("/proc/self/exe", path, PATH_MAX);
    if (size < 0)
        size = 0;
    path[size] = 0;
}

static void GetRelativePathFormat(char *base, char *fmt) {
    unsigned idx;
    char *p, *end;
    end = strrchr(base, '/');
    for (idx = 0, p = base; *p; ++p, ++idx) {
        fmt[idx] = *p;
        if (p == end)
            break;
    }
    fmt[++idx] = '%';
    fmt[++idx] = 's';
    fmt[++idx] = 0;
}

typedef void (*Py_SetProgramNamePtr)(wchar_t *);

typedef void (*Py_SetPathPtr)(const wchar_t *);

typedef int (*Py_MainPtr)(int, wchar_t **);

typedef void *(*PyMem_RawMallocPtr)(size_t);

typedef void (*PyMem_RawFreePtr)(void *);

typedef wchar_t *(*Py_DecodeLocalePtr)(const char *, size_t *);


int main(int argc, char **argv) {
    char executable[PATH_MAX + 1] = {0};
    char pthfmt[PATH_MAX + 1] = {0};
    char corepath[PATH_MAX + 1] = {0};
    char stdlibpath[PATH_MAX + 1] = {0};
    char modpath[PATH_MAX + 1] = {0};
    char syspath[SYS_PATH_BUFFER_SIZE] = {0};
    void *core = 0;
    int retcode = 126;
    int i;

    char *env_argument = NULL;
    char *env_entrypoint = NULL;
    char *env_logname = NULL;
    char entrypoint[ENTRYPOINT_MAXLEN];
    int ret = 0;

//    LOGP("Initializing Python for Android");

    // Set a couple of built-in environment vars:
    setenv("P4A_BOOTSTRAP", "SDL2", 1);  // env var to identify p4a to applications
    env_argument = getenv("ANDROID_ARGUMENT");
    setenv("ANDROID_APP_PATH", env_argument, 1);
    env_entrypoint = getenv("ANDROID_ENTRYPOINT");
    env_logname = getenv("PYTHON_NAME");
    if (!getenv("ANDROID_UNPACK")) {
        /* ANDROID_UNPACK currently isn't set in services */
        setenv("ANDROID_UNPACK", env_argument, 1);
    }
    if (env_logname == NULL) {
        env_logname = "python";
        setenv("PYTHON_NAME", "python", 1);
    }

    // Set additional file-provided environment vars:
//    LOGP("Setting additional env vars from p4a_env_vars.txt");
    char env_file_path[256];
    snprintf(env_file_path, sizeof(env_file_path),
             "%s/p4a_env_vars.txt", getenv("ANDROID_UNPACK"));

    Py_SetProgramNamePtr Py_SetProgramName = 0;
    Py_SetPathPtr Py_SetPath = 0;
    Py_MainPtr Py_Main = 0;
    PyMem_RawMallocPtr PyMem_RawMalloc = 0;
    PyMem_RawFreePtr PyMem_RawFree = 0;
    Py_DecodeLocalePtr Py_DecodeLocale = 0;

    GetExecutablePath(executable);
    GetRelativePathFormat(executable, pthfmt);

    snprintf(corepath, PATH_MAX, pthfmt, PYTHON3_DLL_REL_PATH);
    snprintf(stdlibpath, PATH_MAX, pthfmt, PYTHON3_STDLIB_REL_PATH);
    snprintf(modpath, PATH_MAX, pthfmt, PYTHON3_MODULES_REL_PATH);
    snprintf(syspath, SYS_PATH_BUFFER_SIZE - 1, "%s:%s", stdlibpath, modpath);

    // Set up the python path
    char paths[256];

    char python_bundle_dir[256];
    snprintf(python_bundle_dir, 256,
             "%s/_python_bundle", getenv("ANDROID_UNPACK"));

    snprintf(paths, 256,
             "%s/stdlib.zip:%s/modules",
             python_bundle_dir, python_bundle_dir);

    core = dlopen(corepath, RTLD_LAZY);
    if (core == 0) {
        const char *lasterr = dlerror();
        if (lasterr == 0)
            lasterr = NULL_PTR_STR;
        fprintf(stderr, "Fatal Python error: cannot load library: '%s', dlerror: %s\n", corepath,
                lasterr);
        goto exit;
    }

    Py_SetProgramName = (Py_SetProgramNamePtr) dlsym(core, "Py_SetProgramName");
    if (Py_SetProgramName == 0) {
        const char *lasterr = dlerror();
        if (lasterr == 0)
            lasterr = NULL_PTR_STR;
        fprintf(stderr,
                "Fatal Python error: cannot load symbol: '%s' from library '%s', dlerror: %s\n",
                "Py_SetProgramName", corepath, lasterr);
        goto exit;
    }

    Py_SetPath = (Py_SetPathPtr) dlsym(core, "Py_SetPath");
    if (Py_SetPath == 0) {
        const char *lasterr = dlerror();
        if (lasterr == 0)
            lasterr = NULL_PTR_STR;
        fprintf(stderr,
                "Fatal Python error: cannot load symbol: '%s' from library '%s', dlerror: %s\n",
                "Py_SetPath", corepath, lasterr);
        goto exit;
    }

    Py_Main = (Py_MainPtr) dlsym(core, "Py_Main");
    if (Py_Main == 0) {
        const char *lasterr = dlerror();
        if (lasterr == 0)
            lasterr = NULL_PTR_STR;
        fprintf(stderr,
                "Fatal Python error: cannot load symbol: '%s' from library '%s', dlerror: %s\n",
                "Py_Main", corepath, lasterr);
        goto exit;
    }

    PyMem_RawMalloc = (PyMem_RawMallocPtr) dlsym(core, "PyMem_RawMalloc");
    if (PyMem_RawMalloc == 0) {
        const char *lasterr = dlerror();
        if (lasterr == 0)
            lasterr = NULL_PTR_STR;
        fprintf(stderr,
                "Fatal Python error: cannot load symbol: '%s' from library '%s', dlerror: %s\n",
                "PyMem_RawMalloc", corepath, lasterr);
        goto exit;
    }

    PyMem_RawFree = (PyMem_RawFreePtr) dlsym(core, "PyMem_RawFree");
    if (PyMem_RawFree == 0) {
        const char *lasterr = dlerror();
        if (lasterr == 0)
            lasterr = NULL_PTR_STR;
        fprintf(stderr,
                "Fatal Python error: cannot load symbol: '%s' from library '%s', dlerror: %s\n",
                "PyMem_RawFree", corepath, lasterr);
        goto exit;
    }

    Py_DecodeLocale = (Py_DecodeLocalePtr) dlsym(core, "Py_DecodeLocale");
    if (Py_DecodeLocale == 0) {
        const char *lasterr = dlerror();
        if (lasterr == 0)
            lasterr = NULL_PTR_STR;
        fprintf(stderr,
                "Fatal Python error: cannot load symbol: '%s' from library '%s', dlerror: %s\n",
                "Py_DecodeLocale", corepath, lasterr);
        goto exit;
    }

    wchar_t *executable_w = Py_DecodeLocale(executable, 0);
    if (executable_w == 0) {
        fprintf(stderr, "Fatal Python error: unable to decode executable path: '%s'\n", executable);
        goto exit;
    }

    wchar_t *syspath_w = Py_DecodeLocale(paths, 0);
    if (syspath_w == 0) {
        fprintf(stderr, "Fatal Python error: unable to decode syspath: '%s'\n", paths);
        goto exit;
    }

    wchar_t **argv_copy = (wchar_t **) PyMem_RawMalloc(sizeof(wchar_t *) * (argc + 1));
    wchar_t **argv_copy2 = (wchar_t **) PyMem_RawMalloc(sizeof(wchar_t *) * (argc + 1));

    char *oldloc = strdup(setlocale(LC_ALL, 0));
    setlocale(LC_ALL, "");
    for (i = 0; i < argc; ++i) {
        argv_copy[i] = Py_DecodeLocale(argv[i], 0);
        if (argv_copy[i] == 0) {
            free(oldloc);
            fprintf(stderr, "Fatal Python error: unable to decode the command line argument #%i\n",
                    i + 1);
            goto exit;
        }
        argv_copy2[i] = argv_copy[i];
    }
    argv_copy2[argc] = argv_copy[argc] = 0;
    setlocale(LC_ALL, oldloc);
    free(oldloc);

    Py_SetProgramName(executable_w);
    Py_SetPath(syspath_w);
    retcode = Py_Main(argc, argv_copy);

    PyMem_RawFree(executable_w);
    PyMem_RawFree(syspath_w);
    for (i = 0; i < argc; i++) {
        PyMem_RawFree(argv_copy2[i]);
    }
    PyMem_RawFree(argv_copy);
    PyMem_RawFree(argv_copy2);

    exit:
    if (core != 0)
        dlclose(core);

    return retcode;
}