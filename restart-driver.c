#include <linux/limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    int index;
    char *value;
} Arg;

// Returns 1 if interface exists, 0 if timed out
int wait_for_iface(const char *iface, int timeout_sec) {
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "/sys/class/net/%s", iface);

    time_t start = time(NULL);
    while (time(NULL) - start < timeout_sec) {
        if (access(path, F_OK) == 0)
            return 1;
        struct timespec ts = {0, 200000000L}; // 200ms
        nanosleep(&ts, NULL);
    }
    return 0;
}

bool get_arg(int argc, char **argv, Arg *argument) {
    if (argument->value == NULL &&
        (argument->index < argc && argument->index > 0)) {
        argument->value = argv[argument->index];
        return false;
    }

    for (int i = 0; i < argc; i++) {
        if (argv[i] != NULL && strcmp(argv[i], argument->value) == 0) {
            argument->index = i;
            argument->value = argv[i];
            return false;
        }
    }

    return true;
}

char *format_arg(char *value) {
    char prev;
    size_t len = strlen(value);
    // length of original value plus two for a new dash and a null terminator.
    char *new_value = malloc(len + 2);
    if (new_value == NULL) {
        fprintf(stderr, "Buy more RAM!\n");
        exit(1);
    }

    // Add single dash infront of the arg value
    for (size_t i = 0; i <= len; i++) {
        if (i == 0) {
            new_value[i] = '-';
            prev = value[i];
        } else {
            new_value[i] = prev;
            prev = value[i];
        }
    }
    new_value[len + 1] = '\0';

    return new_value;
}

bool argcmp(char *arg, char *value) {
    char *level_one = format_arg(value);

    if (strcmp(arg, level_one) == 0) {
        free(level_one);
        return true;
    }

    char *level_two = format_arg(level_one);

    bool ret = false;

    if (strcmp(arg, level_two) == 0) {
        ret = true;
    }

    free(level_one);
    free(level_two);

    return ret;
}

void help() {
    fprintf(stderr, "Usage: sudo restart-driver <device name>|[-d|-driver] "
                    "<device name>\n\tNote: Needs to run as root!\n"
                    "\nAvailable device: wifi, bluetooth\n"
                    "Example:\n\tsudo restart-driver bluetooth\n"
                    "\tsudo restart-driver -d wifi\n");
    exit(1);
}

char *print_cmd(char **cmd) {
    size_t i = 0, len = 0;

    while (cmd[i] != NULL) {
        len += (strlen(cmd[i]) + 1); // extra +1 for white space
        i++;
    }

    char *ret = malloc(len);
    if (ret == NULL) {
        fprintf(stderr, "Buy more RAM!\n");
        exit(1);
    }
    ret[0] = '\0';

    for (size_t j = 0; j < i; j++) {
        if (cmd[j] == NULL)
            break;

        if (j != 0)
            ret = strcat(ret, " ");
        ret = strcat(ret, cmd[j]);
    }

    return ret;
}

void run_cmd(char **cmd) {
    pid_t pid = fork();
    int ret = 0;

    if (pid < 0) {
        perror("fork failed");
        exit(1);
    }

    char *cmd_str = print_cmd(cmd);

    if (pid == 0) {
        execvp(cmd[0], cmd);
        perror("exec");
        exit(1);
    } else {
        int status;
        printf("[INFO] %s\n", cmd_str);
        waitpid(pid, &status, 0);

        if (WIFEXITED(status)) {
            int exit_code = WEXITSTATUS(status);
            if (exit_code != 0) {
                ret = exit_code;
            }
        } else if (WIFSIGNALED(status)) {
            // int signal_num = WTERMSIG(status);
            ret = 1;
        }

        if (ret != 0) {
            fprintf(stderr, "[ERROR] failed to execute command:\n\t%s\n",
                    cmd_str);
            free(cmd_str);
            exit(ret);
        }
    }
}

int main(int argc, char **argv) {
    if (geteuid() != 0) {
        help();
    }

    Arg driver = {
        .index = 1,
        .value = NULL,
    };

    char *driver_name = NULL;
    bool error = get_arg(argc, argv, &driver);

    if (!error) {
        if (argcmp(driver.value, "driver") == true ||
            argcmp(driver.value, "d") == true) {
            if (driver.index + 1 < argc) {
                driver_name = argv[driver.index + 1];
            }
        } else if (strcmp(driver.value, "wifi") == 0 ||
                   strcmp(driver.value, "bluetooth") == 0) {
            driver_name = driver.value;
        }
    }

    // Handles both cases (error == true, unset driver_name)
    if (driver_name == NULL) {
        help();
    }

    if (strcmp(driver_name, "wifi") == 0) {
        char *s1[] = {"systemctl", "stop", "iwd", NULL};
        run_cmd(s1);

        char *s2[] = {"ip", "link", "set", "wlp1s0f0", "down", NULL};
        run_cmd(s2);

        char *s3[] = {"modprobe", "-r",       "brcmfmac_wcc",
                      "brcmutil", "brcmfmac", NULL};
        run_cmd(s3);

        char *s4[] = {"modprobe", "brcmfmac", NULL};
        run_cmd(s4);

        char *s5[] = {"systemctl", "start", "iwd", NULL};
        run_cmd(s5);

        char *s6[] = {"ip", "link", "set", "wlp1s0f0", "up", NULL};
        if (!wait_for_iface("wlp1s0f0", 10)) {
            fprintf(
                stderr,
                "[ERROR] interface wlp1s0f0 did not appear within timeout\n");
            exit(1);
        }
        run_cmd(s6);

        return 0;
    }

    if (strcmp(driver_name, "bluetooth") == 0) {
        char *s1[] = {"rmmod", "hci_bcm4377", NULL};
        run_cmd(s1);

        char *s2[] = {"modprobe", "hci_bcm4377", NULL};
        run_cmd(s2);

        return 0;
    }

    if (strcmp(driver_name, "test") == 0) {
        char *s1[] = {"notify-send", "Test from restart-driver", NULL};
        run_cmd(s1);

        return 0;
    }

    help();
    return 1;
}
