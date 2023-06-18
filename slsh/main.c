#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// Builtin Decleration Commands

int slsh_cd(char **args);
int slsh_help(char **args);
int slsh_exit(char **args);

// BuiltIn Shell Commands 

char *builtin_str[] = {
  "cd", 
  "help", 
  "exit"
}; 

int (*builtin_func[])(char **)={
  &slsh_cd, 
  &slsh_help, 
  &slsh_exit, 
};

int slsh_cd(char **args) {
  if (args[1] == NULL){
    fprintf(stderr, "slsh: expected argument to \"cd\"\n");
  } else {
    if (chdir(args[1]) != 0) {
      perror("slsh");
    }
  }
  return 1;
}

int slsh_help(char **args){
  int i;
  printf("Saihaj Law's SLSH\n");
  printf("Type programs names and arguments, and hit enter.\n");
  printf("The following are built in:\n");

  for (i=0; i < slsh_num_builtins();i++){
    printf(" %s\n", builtin_str[i]);
  }

  printf("Use the man command for information on other posisble programs\n");
  return 1;
}

int slsh_launch(char **args) {
  pid_t pid;
  int status;

  pid = fork();
  if (pid ==0){
    if (execvp(args[0], args) == -1){
      perror("slsh");
    }
    exit(EXIT_FAILURE);
  } else if (pid < 0){
    perror("slsh");
  } else {

    do {
      waitpid(pid, &status, WUNTRACED);
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));
  }

  return 1;
}

int slsh_execute(char **args){
  int i;

  if (args[0] == NULL){
    return 1;
  }

  for (i=0; i < slsh_num_builtins();i++){
    if (strcmp(args[0], builtin_str[i]) ==0){
      return(*builtin_func[i])(args);
    }
  }

  return slsh_launch(args);
}

char *slsh_read_line(void){

  #ifdef SLSH__USE_STD_GETLINE

  char *line = NULL;
  ssize_t bufsize = 0;
  if (getline(&line, &bufsize, stdin) == -1){
    if (feof(stdin)) {
      exit(EXIT_SUCCESS);
    } else {
      perror("lsh: getline\n");
      exit(EXIT_FAILURE):
    }
  }

  return line;

  #else
  #define SLSH_RL_BUFSIZE 1024
  int bufsize = SLSH_RL_BUFSIZE;
  int position = 0;
  char *buffer = malloc(sizeof(char)*bufsize);
  int c;

  if (!buffer){
    fprintf(stderr, "slsh: allocation error\n");
    exit(EXIT_FAILURE);
  }

  while (1){
    c = getchar();

    if (c == EOF){
      exit(EXIT_SUCCESS);
    } else if (c == '\n'){
      buffer[position] = '\0';
      return buffer;
    } else {
      buffer[position] = c;
    }

    position++;

    if (position >= bufsize){
      bufsize += SLSH_RL_BUFSIZE;
      buffer = realloc(buffer, bufsize);
      if (!buffer){
        fprintf(stderr, "slsh: allocation error\n");
        exit(EXIT_FAILURE);
      }
    }
  }
  #endif
}
#define SLSH_TOK_BUFSIZE 64
#define SLSH_TOK_DELIM " \t\r\n\a" 

char **slsh_split_line(char *line){
  int bufsize = SLSH_TOK_BUFSIZE, position = 0;
  char **tokens = malloc(bufsize * sizeof(char*));
  char *token, **tokens_backup;

  if (!tokens){
    fprintf(stderr, "slsh: allocation error\n");
    exit(EXIT_FAILURE);
  }

  token = strtok(line, SLSH_TOK_DELIM);
  while (token != NULL){
    tokens[position] = token;
    position++;

    if (position >= bufsize){
      bufsize += SLSH_TOK_BUFSIZE;
      tokens_backup = tokens;
      tokens = realloc(tokens, bufsize * sizeof(char*));

      if (!tokens){
        free(tokens_backup);
        fprintf(stderr, "slsh: allocation error\n");
        exit(EXIT_FAILURE);
      }
    }
    token = strtok(NULL, SLSH_TOK_DELIM);
  }
  tokens[position] = NULL;
  return tokens;
}

void slsh_loop(void){
  char *line;
  char **args;
  int status;

  do {
    printf("> ");
    line = slsh_read_line();
    args = slsh_split_line(line);
    status = slsh_execute(args);

    free(line);
    free(args);
  } while (status);
}

int main(int argc, char **argv){

  slsh_loop();

  return EXIT_SUCCESS;
}

