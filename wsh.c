// Author:  Vojtech Aschenbrenner <asch@cs.wisc.edu>, Fall 2023
// Revised: John Shawger <shawgerj@cs.wisc.edu>, Spring 2024
// Revised: Vojtech Aschenbrenner <asch@cs.wisc.edu>, Fall 2024
// Revised: Leshna Balara <lbalara@cs.wisc.edu>, Spring 2025
// Revised: Pavan Thodima <thodima@cs.wisc.edu>, Spring 2026

#include "parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

// mode of the shell (1 == interactive, 0 == batch)
int mode = -1;

// global variable exit status
int last_exit = 0;

// global variable for environment variables
extern char **environ;

// global variable for pipes
//int **pipe_arr;

// TODO: Parser hook for variable substitution.
char *get_variable(const char *var) {
  char *value = getenv(var);
  if (!value)
    return "";

  return value;
}

// Function to split VAR and VALUE by the delimeter '='
char **split_string(const char *var_val) {
  
  if (var_val == NULL) 
    return NULL;

  const char *eq = strchr(var_val, '=');

  size_t var_len;

  if (eq) {
    var_len = (size_t)(eq - var_val);
  } else {
    var_len = strlen(var_val);
  }
  
  if (var_len == 0) 
    return NULL;

  const char *val_start;

  if (eq) {
    val_start = eq + 1;
  } else {
    val_start = "";
  }

  size_t val_len = 0;

  if (eq)
    val_len = strlen(val_start);

  char **out = malloc(2 * sizeof(char *));
  if (!out) 
    return NULL;

  out[0] = malloc(var_len + 1);
  if (!out[0]) { 
    free(out); 
    return NULL; 
  }

  out[1] = malloc(val_len + 1);
  if (!out[1]) { 
    free(out[0]); 
    free(out); 
    return NULL; 
  }

  memcpy(out[0], var_val, var_len);
  out[0][var_len] = '\0';

  memcpy(out[1], val_start, val_len);
  out[1][val_len] = '\0';

  return out;
}

void execute_absolute_cmd(struct command *cmd) {
  int pid = fork();

  if (pid < 0) {
    //perror("fork");
    exit(1);
  }

  // This is the child
  if (pid == 0) {
    if (execv(cmd->argv[0], cmd->argv) == -1) {
      fprintf(stderr, "%s: Command not found\n", cmd->argv[0]);
      exit(1);
    }
   }

  // This is the parent so wait for child to finish
  int status;
  if (pid > 0) {
    waitpid(pid, &status, 0);
  }

  return;
}

void execute_ext_cmd(struct command *cmd, struct command_line *cl, char *cmd_line) {
  // Check for exit command
  if (cmd->argv && cmd->argv[0] && strcmp(cmd->argv[0], "exit") == 0) {
    free(cmd_line);
    free_command_line(cl);
    exit(0);
  }

  // Check for cd command
  else if (cmd->argv && cmd->argv[0] && strcmp(cmd->argv[0], "cd") == 0) {
    // cd with no argument
    if (cmd->argv[1] == NULL) {
      char *path = getenv("HOME");

      if (path == NULL) {
        fprintf(stderr, "cd: HOME not set\n");
        last_exit = 1;
      }
      else {
        if (chdir(path) != 0) {
          perror("cd");
          last_exit = 1;
        }
      }
    } 
    // cd with argument "cd [dir]"
    else {
      if (chdir(cmd->argv[1]) != 0) {
        perror("cd");
        last_exit = 1;
      }
    }
  }

  // Check for env command
  else if (cmd->argv && cmd->argv[0] && strcmp(cmd->argv[0], "env") == 0) {
    if (cmd->argv[1] == NULL) {
    // Print all environment variables
      int i = 0;
      while (environ[i] != NULL) {
        printf("%s\n", environ[i]);
        i++;
      }
    }
    // Argument given, so set the VAR and value in environment
    else {
    // Get string of argument and set those variables

    // If value not set then just store VAR name with empty VAL
      if (strchr(cmd->argv[1], '=') == NULL) {
        if (setenv(cmd->argv[1], "", 1) == -1) {
          perror("setenv");
          last_exit = 1;
        }
      }
      // Else, set with both VAR and VAL
      else {
        char **var_val = split_string(cmd->argv[1]);
        if (setenv(var_val[0], var_val[1], 1) == -1) {
          perror("setenv");
          last_exit = 1;
        }
        // free memory
        free(var_val[0]);
        free(var_val[1]);
        free(var_val);
      }
    }
  }

  // Check for absolute path commands
  else if (cmd->argv && cmd->argv[0] && cmd->argv[0][0] == '/') {
    execute_absolute_cmd(cmd);
  }
  // Commands that are not built-in (external) and without absolute paths, for example "ls"
  else {
    char *path = getenv("PATH");
    if (path == NULL) {
      path = "/bin";
    }
              
    // Paths may contain ":" so split the different paths by strtok
    //const char delim = ':';
    char *path_copy = strdup(path);
    char *result = strtok(path_copy, ":");
    size_t size;
    char *found_path = NULL;

    while (result != NULL) {
      size = strlen(result) + 1 + strlen(cmd->argv[0]) + 1;
      char *dir_path = malloc(size);

      if (!dir_path) {
        free(dir_path);
        last_exit = 1;
        perror("malloc");
      }

      // Build the path
      strcpy(dir_path, result);
      strcat(dir_path, "/");
      strcat(dir_path, cmd->argv[0]);

      if (access(dir_path, X_OK) == 0) {
        found_path = dir_path;
        break;
      } else {
        free(dir_path);
      }

        result = strtok(NULL, ":");
    }
    free(path_copy);

    if (found_path != NULL) {
      int k = fork();

      if (k < 0) {
        //perror("fork");
        exit(1);
      }

      if (k == 0) {
       if (execv(found_path, cmd->argv) == -1) {
          fprintf(stderr, "%s: Command not found\n", cmd->argv[0]);
          exit(1);
        }
      }

      int status;
      if (k > 0)
        waitpid(k, &status, 0);

      free(found_path);

    } else {
      fprintf(stderr, "%s: Command not found\n", cmd->argv[0]);
      last_exit = 1;
    }
  }
}

int main(int argc, char **argv) {
  //(void)argc;

  FILE *fp = stdin;

  // If argument count > 2
  if (argc > 2) {
    fprintf(stderr, "Usage: %s [file]\n", argv[0]);
    exit(1);
  }

  // Check for interactive mode
  if (argc == 1) {
    mode = isatty(STDIN_FILENO);   
  }

  // Check for batch mode
  if (argc == 2) {
    mode = 0;
    fp = fopen(argv[1], "r");

    if (!fp) {
      perror("fopen");
      exit(1);
    }
  }

  // Arguments for getline
  char *cmd_line = NULL;
  size_t buffsize = 0;
  struct command_line *cl = NULL;
  struct pipeline *pl = NULL;
  struct command *cmd = NULL;

  while (1) {

    if (mode == 1) {
      printf("wsh> ");
      fflush(stdout);
    }

    if (getline(&cmd_line, &buffsize, fp) == -1) {
      break;
    } else {
      cl = parse_input(cmd_line);
      if (cl == NULL) {
        continue;
      } else {
        //printf("%s\n", cl->pipelines[0].commands[0].argv[0]);
        for (int i = 0; i < cl->num_pipelines; i++) {
          last_exit = 0;
          pl = &cl->pipelines[i];

          // Check for single commands like "exit" without pipe
          if (pl->num_commands == 1) {
            cmd = &pl->commands[0];
            execute_ext_cmd(cmd, cl, cmd_line);
          }
          // Handle pipelines
          else if (pl->num_commands > 1) {
            // create N-1 pipes for N commands
            int pipefds[pl->num_commands - 1][2];
            int commands = pl->num_commands;
            int pids[commands];
            for (int i = 0; i < pl->num_commands - 1; i++) {
              // Make a pipe between the command
              if (pipe(pipefds[i]) == -1) {
                perror("pipe");
                exit(1);
              }
            }

            for (int i = 0; i < pl->num_commands; i++) {
              cmd = &pl->commands[i];
              pids[i] = fork();

              if (pids[i] < 0) {
                perror("fork");
                exit(1);
              }

              // Child
              if (pids[i] == 0) {
                if (i > 0) {
                  // If not the first command, then redirect stdin to the read end of the previous pipe
                  if (dup2(pipefds[i - 1][0], STDIN_FILENO) == -1) {
                    perror("dup2");
                    _exit(1);
                  }
                }

                if (i < pl->num_commands - 1) {
                  // If not the last command, then redirect stdout to the write end of the current pipe
                  if (dup2(pipefds[i][1], STDOUT_FILENO) == -1) {
                    perror("dup2");
                    _exit(1);
                  }
                }

                for (int j = 0; j < pl->num_commands - 1; j++) {
                  close(pipefds[j][0]);
                  close(pipefds[j][1]);
                }

                // Execute the command
                if (cmd->argv && cmd->argv[0] && strcmp(cmd->argv[0], "exit") == 0) {
                  free(cmd_line);
                  free_command_line(cl);

                  fflush(stdout);
                  _exit(0);
                }
                else if (cmd->argv && cmd->argv[0] && strcmp(cmd->argv[0], "cd") == 0) {
                  // cd with no argument
                  if (cmd->argv[1] == NULL) {
                    char *path = getenv("HOME");

                    if (path == NULL) {
                      fprintf(stderr, "cd: HOME not set\n");
                      last_exit = 1;
                    }
                    else {
                      if (chdir(path) != 0) {
                        perror("cd");
                        last_exit = 1;
                      }
                    }
                  } 
                  // cd with argument "cd [dir]"
                  else {
                    if (chdir(cmd->argv[1]) != 0) {
                      perror("cd");
                      last_exit = 1;
                    }
                  }
                  fflush(stdout);
                  _exit(last_exit);
                }

                else if (cmd->argv && cmd->argv[0] && strcmp(cmd->argv[0], "env") == 0) {
                  if (cmd->argv[1] == NULL) {
                  // Print all environment variables
                    int i = 0;
                    while (environ[i] != NULL) {
                      printf("%s\n", environ[i]);
                      i++;
                    }
                  }
                  // Argument given, so set the VAR and value in environment
                  else {
                  // Get string of argument and set those variables

                  // If value not set then just store VAR name with empty VAL
                    if (strchr(cmd->argv[1], '=') == NULL) {
                      if (setenv(cmd->argv[1], "", 1) == -1) {
                        perror("setenv");
                        last_exit = 1;
                      }
                    }
                    // Else, set with both VAR and VAL
                    else {
                      char **var_val = split_string(cmd->argv[1]);
                      if (setenv(var_val[0], var_val[1], 1) == -1) {
                        perror("setenv");
                        last_exit = 1;
                      }
                      // free memory
                      free(var_val[0]);
                      free(var_val[1]);
                      free(var_val);
                    }
                  }
                  fflush(stdout);
                  _exit(last_exit);
                }
                // built in commands
                else {
                  char *path = getenv("PATH");
                  if (path == NULL) {
                    path = "/bin";
                  }
                            
                  // Paths may contain ":" so split the different paths by strtok
                  //const char delim = ':';
                  char *path_copy = strdup(path);
                  char *result = strtok(path_copy, ":");
                  size_t size;
                  char *found_path = NULL;

                  while (result != NULL) {
                    size = strlen(result) + 1 + strlen(cmd->argv[0]) + 1;
                    char *dir_path = malloc(size);

                    if (!dir_path) {
                      free(dir_path);
                      last_exit = 1;
                      perror("malloc");
                    }

                    // Build the path
                    strcpy(dir_path, result);
                    strcat(dir_path, "/");
                    strcat(dir_path, cmd->argv[0]);

                    if (access(dir_path, X_OK) == 0) {
                      found_path = dir_path;
                      break;
                    } else {
                      free(dir_path);
                    }

                      result = strtok(NULL, ":");
                  }
                  free(path_copy);

                  if (found_path != NULL) {

                    if (execv(found_path, cmd->argv) == -1) {
                      fprintf(stderr, "%s: Command not found\n", cmd->argv[0]);
                      _exit(1);
                    }
                  }
                  else {
                    last_exit = 1;
                  }

                  fflush(stdout);
                  _exit(last_exit);
                }
                exit(last_exit);
              }
            }

            // Parent
            // Close all piepfds
            for (int i = 0; i < pl->num_commands-1; i++) {
              if (close(pipefds[i][0]) == -1)
                perror("close");
              if (close(pipefds[i][1]) == -1)
                perror("close");
            }

            for (int i = 0; i < pl->num_commands; i++) {
                int status;
                waitpid(pids[i], &status, 0);
            }
          }
        }
        free_command_line(cl);
      }
    }
  }

  if (mode == 1) {
    if (fclose(fp) == EOF) {
        perror("fclose");
        last_exit = 1;
    }
  }

  free(cmd_line);
  return last_exit;
}
