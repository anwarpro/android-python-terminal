/*
 * Copyright (C) 2012 Steven Luo
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "process.h"

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <dirent.h>
#include <cerrno>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <signal.h>
#include <string.h>
#include <Python.h>
#include <locale.h>

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
        {NULL, NULL, 0, NULL}
};


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
}

typedef unsigned short char16_tt;

class String8 {
public:
    String8() {
        mString = 0;
    }

    ~String8() {
        if (mString) {
            free(mString);
        }
    }

    void set(const char16_tt *o, size_t numChars) {
        if (mString) {
            free(mString);
        }
        mString = (char *) malloc(numChars + 1);
        if (!mString) {
            return;
        }
        for (size_t i = 0; i < numChars; i++) {
            mString[i] = (char) o[i];
        }
        mString[numChars] = '\0';
    }

    const char *string() {
        return mString;
    }

private:
    char *mString;
};

static int throwOutOfMemoryError(JNIEnv *env, const char *message) {
    jclass exClass;
    const char *className = "java/lang/OutOfMemoryError";

    exClass = env->FindClass(className);
    return env->ThrowNew(exClass, message);
}

static int throwIOException(JNIEnv *env, int errnum, const char *message) {
    __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "%s errno %s(%d)",
                        message, strerror(errno), errno);

    if (errnum != 0) {
        const char *s = strerror(errnum);
        if (strcmp(s, "Unknown error") != 0)
            message = s;
    }

    jclass exClass;
    const char *className = "java/io/IOException";

    exClass = env->FindClass(className);
    return env->ThrowNew(exClass, message);
}

static void closeNonstandardFileDescriptors() {
    // Android uses shared memory to communicate between processes. The file descriptor is passed
    // to child processes using the environment variable ANDROID_PROPERTY_WORKSPACE, which is of
    // the form "properties_fd,sizeOfSharedMemory"
    int properties_fd = -1;
    char *properties_fd_string = getenv("ANDROID_PROPERTY_WORKSPACE");
    if (properties_fd_string != NULL) {
        properties_fd = atoi(properties_fd_string);
    }
    DIR *dir = opendir("/proc/self/fd");
    if (dir != NULL) {
        int dir_fd = dirfd(dir);

        while (true) {
            struct dirent *entry = readdir(dir);
            if (entry == NULL) {
                break;
            }

            int fd = atoi(entry->d_name);
            if (fd > STDERR_FILENO && fd != dir_fd && fd != properties_fd) {
                close(fd);
            }
        }

        closedir(dir);
    }
}

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

    /* our logging module for android */
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

    /* ensure threads will work.
     */
    LOGP("AND: Init threads");
    PyEval_InitThreads();

    PyRun_SimpleString("import androidembed\nandroidembed.log('testing python "
                       "print redirection')");

    /* inject our bootstrap code to redirect python stdin/stdout
     * replace sys.path with our path
     */
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
        /* "sys.path.append(join(dirname(realpath(__file__)), 'site-packages'))") */
        PyRun_SimpleString("sys.path = ['.'] + sys.path");
    }

    LOGP("AND: Ran string");

    /* run it !
     */
    LOGP("Run user program, change dir and execute entrypoint");

    /* Get the entrypoint, search the .pyo then .py
     */
    char *dot = strrchr(env_entrypoint, '.');
    char *ext = ".pyc";

    if (strlen(env_entrypoint) > ENTRYPOINT_MAXLEN - 2) {
        LOGP("Entrypoint path is too long, try increasing ENTRYPOINT_MAXLEN.");
        return -1;
    }
    if (!strcmp(dot, ext)) {
        if (!file_exists(env_entrypoint)) {
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
        strcpy(entrypoint, env_entrypoint);
        entrypoint[strlen(env_entrypoint) + 1] = '\0';
        entrypoint[strlen(env_entrypoint)] = 'c';
        if (!file_exists(entrypoint)) {
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


    auto **argv_copy = (wchar_t **) PyMem_RawMalloc(sizeof(wchar_t *) * (argc + 1));
    auto **argv_copy2 = (wchar_t **) PyMem_RawMalloc(sizeof(wchar_t *) * (argc + 1));

    char *oldloc = strdup(setlocale(LC_ALL, 0));
    setlocale(LC_ALL, "");
    for (int i = 0; i < argc; ++i) {
        argv_copy[i] = Py_DecodeLocale(argv[i], nullptr);
        if (argv_copy[i] == 0) {
            free(oldloc);
        }
        argv_copy2[i] = argv_copy[i];
    }
    argv_copy2[argc] = argv_copy[argc] = 0;

    ret = Py_Main(argc, argv_copy);

    if (PyErr_Occurred() != NULL) {
        ret = 1;
        PyErr_Print(); /* This exits with the right code if SystemExit. */
        PyObject *f = PySys_GetObject("stdout");
        if (PyFile_WriteString("\n", f))
            PyErr_Clear();
    }

    return ret;
}

static int create_subprocess(JNIEnv *env, const char *cmd, char *const argv[], char *const envp[],
                             int masterFd) {
    // same size as Android 1.6 libc/unistd/ptsname_r.c
    char devname[64];
    pid_t pid;

    fcntl(masterFd, F_SETFD, FD_CLOEXEC);

    // grantpt is unnecessary, because we already assume devpts by using /dev/ptmx
    if (unlockpt(masterFd)) {
        throwIOException(env, errno, "trouble with /dev/ptmx");
        return -1;
    }
    memset(devname, 0, sizeof(devname));
    // Early (Android 1.6) bionic versions of ptsname_r had a bug where they returned the buffer
    // instead of 0 on success.  A compatible way of telling whether ptsname_r
    // succeeded is to zero out errno and check it after the call
    errno = 0;
    int ptsResult = ptsname_r(masterFd, devname, sizeof(devname));
    if (ptsResult && errno) {
        throwIOException(env, errno, "ptsname_r returned error");
        return -1;
    }

    pid = fork();
    if (pid < 0) {
        throwIOException(env, errno, "fork failed");
        return -1;
    }

    if (pid == 0) {
        int pts;

        setsid();

        pts = open(devname, O_RDWR);
        if (pts < 0) exit(-1);

        ioctl(pts, TIOCSCTTY, 0);

        dup2(pts, 0);
        dup2(pts, 1);
        dup2(pts, 2);

        closeNonstandardFileDescriptors();

        if (envp) {
            for (; *envp; ++envp) {
                putenv(*envp);
            }
        }

//        execv(cmd, argv);

        char *argv[2];
        argv[0] = "Python_app";
        argv[1] = NULL;

        main(1, argv);

        exit(-1);
    } else {
        return (int) pid;
    }
}

extern "C" {

JNIEXPORT void JNICALL Java_jackpal_androidterm_TermExec_sendSignal(JNIEnv *env, jobject clazz,
                                                                    jint procId, jint signal) {
    kill(procId, signal);
}

JNIEXPORT jint JNICALL Java_jackpal_androidterm_TermExec_waitFor(JNIEnv *env, jclass clazz,
                                                                 jint procId) {
    int status;
    waitpid(procId, &status, 0);
    int result = 0;
    if (WIFEXITED(status)) {
        result = WEXITSTATUS(status);
    }
    return result;
}

JNIEXPORT jint JNICALL Java_jackpal_androidterm_TermExec_createSubprocessInternal(JNIEnv *env,
                                                                                  jclass clazz,
                                                                                  jstring cmd,
                                                                                  jobjectArray args,
                                                                                  jobjectArray envVars,
                                                                                  jint masterFd) {
    const jchar *str = cmd ? env->GetStringCritical(cmd, 0) : 0;
    String8 cmd_8;
    if (str) {
        cmd_8.set(str, env->GetStringLength(cmd));
        env->ReleaseStringCritical(cmd, str);
    }

    jsize size = args ? env->GetArrayLength(args) : 0;
    char **argv = NULL;
    String8 tmp_8;
    if (size > 0) {
        argv = (char **) malloc((size + 1) * sizeof(char *));
        if (!argv) {
            throwOutOfMemoryError(env, "Couldn't allocate argv array");
            return 0;
        }
        for (int i = 0; i < size; ++i) {
            jstring arg = reinterpret_cast<jstring>(env->GetObjectArrayElement(args, i));
            str = env->GetStringCritical(arg, 0);
            if (!str) {
                throwOutOfMemoryError(env, "Couldn't get argument from array");
                return 0;
            }
            tmp_8.set(str, env->GetStringLength(arg));
            env->ReleaseStringCritical(arg, str);
            argv[i] = strdup(tmp_8.string());
        }
        argv[size] = NULL;
    }

    size = envVars ? env->GetArrayLength(envVars) : 0;
    char **envp = NULL;
    if (size > 0) {
        envp = (char **) malloc((size + 1) * sizeof(char *));
        if (!envp) {
            throwOutOfMemoryError(env, "Couldn't allocate envp array");
            return 0;
        }
        for (int i = 0; i < size; ++i) {
            jstring var = reinterpret_cast<jstring>(env->GetObjectArrayElement(envVars, i));
            str = env->GetStringCritical(var, 0);
            if (!str) {
                throwOutOfMemoryError(env, "Couldn't get env var from array");
                return 0;
            }
            tmp_8.set(str, env->GetStringLength(var));
            env->ReleaseStringCritical(var, str);
            envp[i] = strdup(tmp_8.string());
        }
        envp[size] = NULL;
    }

    int ptm = create_subprocess(env, cmd_8.string(), argv, envp, masterFd);

    if (argv) {
        for (char **tmp = argv; *tmp; ++tmp) {
            free(*tmp);
        }
        free(argv);
    }
    if (envp) {
        for (char **tmp = envp; *tmp; ++tmp) {
            free(*tmp);
        }
        free(envp);
    }

    return ptm;
}

}