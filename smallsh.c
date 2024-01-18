// Name: Kevin Riemer
// Smallsh

#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <signal.h>

#ifndef MAX_WORDS
#define MAX_WORDS 512
#endif

char *words[MAX_WORDS];
size_t wordsplit(char const *line);
char * expand(char const *word);

/* Set Global Booleans */
int multipleCommands = 0;
int multiCommandComplete = 0;

int questionStatus;
int bgInt;
pid_t bgPIDforExpand;

/* Signal Handler */
void sigint_handler(int sig) {}

/* Begin smallsh */
int main(int argc, char *argv[])
{
  FILE *input = stdin;
  char *input_fn = "(stdin)";
  if (argc == 2) {
    input_fn = argv[1];
    input = fopen(input_fn, "re");
    if (!input) err(1, "%s", input_fn);
  } else if (argc > 2) {
    errx(1, "too many arguments");
  }

  char *line = NULL;
  size_t n = 0;
  
  /* Define Signal Structures */
  struct sigaction SIGINT_action = {0}, ignore_action = {0}, old_action = {0};

  SIGINT_action.sa_handler = sigint_handler;
  sigfillset(&SIGINT_action.sa_mask);
  SIGINT_action.sa_flags = 0;

  ignore_action.sa_handler = SIG_IGN;
  sigfillset(&ignore_action.sa_mask);
  ignore_action.sa_flags = 0;

  old_action.sa_handler = SIG_DFL;
  sigfillset(&old_action.sa_mask);
  old_action.sa_flags = 0;


  for (;;) {
    
    /* Register Ignore Signal Handler  */
    sigaction(SIGINT, &ignore_action, &old_action);

    /* Manage Background Processes */
    pid_t bgpid;
    for (;;) {
      bgpid = waitpid(0, &bgInt, WNOHANG | WUNTRACED); /* Check for any processes */
      if (bgpid == 0) break;
      if (bgpid == -1) {
        clearerr(stderr);
        break;
      }
      /* Deal with them accordingly if there are any that finished */
      else if (WIFEXITED(bgInt)) {
        fprintf(stderr, "Child process %jd done. Exit status %d.\n", (intmax_t) bgpid, WEXITSTATUS(bgInt));
        bgPIDforExpand = bgpid;
      }
      else if (WIFSIGNALED(bgInt)) {
        fprintf(stderr, "Child process %jd done. Signaled %d.\n", (intmax_t) bgpid, WTERMSIG(bgInt));
        bgPIDforExpand = bgpid;
      }
      else {
        kill(bgpid, SIGCONT);
        fprintf(stderr, "Child process %jd stopped. Continuing.\n", (intmax_t) bgpid);
        bgPIDforExpand = bgpid;
      }
    
    }
    
    /* Prompt */
    if (input == stdin) {
    /* Interactive Mode */
      
      /* Get and print PS1 */
      char *ps1 = getenv("PS1");
      fprintf(stderr, "%s", ps1);
      
      /* Register SIGINT Signal Handler */
      sigaction(SIGINT, &SIGINT_action, NULL);
    }

    ssize_t line_len = getline(&line, &n, input);
    
    /* Check for EINTR and clearerr accordingly */
    if (errno == EINTR) {
      clearerr(stdin);
      fprintf(stderr,"\n");
      continue;
    }

    /* Register Signal Handlers and set back to ignore */
    sigaction(SIGINT, NULL, &SIGINT_action);
    sigaction(SIGINT, &ignore_action, NULL);
    
    /* Check for EOF, other errors, and newline */
    if (line_len < 0 && (feof(stdin) || feof(input))) exit(0);
    if (line_len < 0) err(1, "%s", input_fn);
    if (strcmp(line, "\n") == 0) {
      continue; 
    }
    
    /* Reset booleans */
    multipleCommands = 0;
    multiCommandComplete = 0;

    size_t nwords = wordsplit(line);

    for (size_t i = 0; i < nwords; ++i) {
      /* Expand */
      char *exp_word = expand(words[i]);
      free(words[i]);
      words[i] = exp_word;
    }

    /* Check for exit built-in */
    if (words[0] != NULL && strcmp(words[0], "exit") == 0) {
      if (words[1] == NULL) { /* If there is no follow up exit int */
        char *exitExpand = expand("$?");  /* Expand "$?" if no exit int provided */
        int exitExpandStatus = atoi(exitExpand);
        exit(exitExpandStatus); /* Exit with appropriate status */
      }
      else {
        if (strcmp(words[1], "0") != 0 && atoi(words[1]) == 0) errx(1, "Argument is not an integer\n"); /* Check for bad arguments */
        if (words[3] != NULL) errx(1, "Too many arguments\n"); /* Check for too many arguments */
        int exitStatus = atoi(words[1]);
        exit(exitStatus); /* Exit with appropriate status */
      } 
    }
      
    /* Check for CD built-in */
    else if (words[0] != NULL && strcmp(words[0], "cd") == 0) {
      if (words[1] == NULL) { /* If path is not provided */
        char *homePath;
        homePath = expand("${HOME}"); /* Expand home path */
        chdir(homePath); /* Change directory */ 
        
        /* Clear words array */
        for (int i = 0; i < nwords; ++i) {
          words[i] = 0;
        }
      }
      
      else {
        if (words[2] != NULL) errx(1, "Too many arguments\n"); /* Check for too many arguments */
        char *newPath;
        newPath = words[1];
        chdir(newPath); /* Change directory */
        
        /* Clear words array */
        for (int i = 0; i < nwords; ++i) {
          words[i] = 0;
        }  
      }
    }
      
      else { /* Fork */
        /* Declare fork variables */
        pid_t spawnpid = -5;
        int childStatus;
        int openFile;
        int wasRedirected = 0;
        int isBackground = 0;
        int j = 0;
        char *childWords[nwords + 1]; /* Define child words array */
        if (childWords[0] == NULL) {
          /* Clear child words array */
          for (int i = 0; i < nwords; ++i) {
            childWords[i] = 0;
          } 
        }

        if (strcmp(words[nwords - 1], "&") == 0) {
          isBackground = 1; 
        }

        spawnpid = fork();
        switch (spawnpid) {
          case -1:
            perror("fork() failed!");
            exit(1);
            break;
          case 0:
            /* Revert Signal Handler */
            sigaction(SIGINT, &old_action, &ignore_action);   
            
            /* File Redirection */
            for (int i = 0; i < nwords; ++i) {
              if (strcmp(words[i], "<") == 0) {
                openFile = open(words[i + 1], O_RDONLY);
                int openSuccess = dup2(openFile, STDIN_FILENO);
                if (openSuccess < 0) {
                  perror("open() failed!");
                  exit(1);
                }
                i++;
                wasRedirected = 1;
              }

              else if (strcmp(words[i], ">") == 0) {
                openFile = open(words[i + 1], O_CREAT | O_RDWR | O_TRUNC, 0777);
                int openSuccess = dup2(openFile, STDOUT_FILENO);
                if (openSuccess < 0) {
                  perror("open() failed!");
                  exit(1);
                }
                i++;
                wasRedirected = 1;
              }

              else if (strcmp(words[i], ">>") == 0) {
                openFile = open(words[i + 1], O_CREAT | O_RDWR | O_APPEND, 0777);
                int openSuccess = dup2(openFile, STDOUT_FILENO);
                if (openSuccess < 0) {
                  perror("open() failed!");
                  exit(1);
                }
                i++;
                wasRedirected = 1;
              }
              
              /* Not a redirection operator */
              else {
                childWords[j] = words[i];
                j++;
              }
              
         } 
            
            if (wasRedirected == 1) { /* Check if a redirection operator was present */
              childWords[j] = NULL;
              int execReturn = execvp(words[0], childWords);
              if (execReturn) exit(execReturn);
              else {exit(0);}
              
            }

            else {
              execvp(words[0], words);
            }
          
            break;

          default:
            /* Check if it is a foreground command */
            if (isBackground == 0) {
              spawnpid = waitpid(spawnpid, &childStatus, WUNTRACED);
              questionStatus = childStatus;
              if (spawnpid > 0 && !WIFEXITED(childStatus) && !WIFSIGNALED(childStatus)) { /* Check if child process needs to be killed */
                kill(spawnpid, SIGCONT);
                fprintf(stderr, "Child process %jd stopped. Continuing.\n", (intmax_t) spawnpid);
                bgPIDforExpand = spawnpid; /* Update PID */
              }

            }
            /* Clear words array */
            for (int i = 0; i < nwords; ++i) {
              words[i] = 0;
            }
            isBackground = 0;
            bgPIDforExpand = spawnpid; /* Update PID */

            break;
        }

      }

    }

  }
  

char *words[MAX_WORDS] = {0};


/* Splits a string into words delimited by whitespace. Recognizes
 * comments as '#' at the beginning of a word, and backslash escapes.
 *
 * Returns number of words parsed, and updates the words[] array
 * with pointers to the words, each as an allocated string.
 */
size_t wordsplit(char const *line) {
  size_t wlen = 0;
  size_t wind = 0;

  char const *c = line;
  for (;*c && isspace(*c); ++c); /* discard leading space */

  for (; *c;) {
    if (wind == MAX_WORDS) break;
    /* read a word */
    if (*c == '#') break;
    for (;*c && !isspace(*c); ++c) {
      if (*c == '\\') ++c;
      void *tmp = realloc(words[wind], sizeof **words * (wlen + 2));
      if (!tmp) err(1, "realloc");
      words[wind] = tmp;
      words[wind][wlen++] = *c; 
      words[wind][wlen] = '\0';
    }
    ++wind;
    wlen = 0;
    for (;*c && isspace(*c); ++c);
  }
  return wind;
}


/* Find next instance of a parameter within a word. Sets
 * start and end pointers to the start and end of the parameter
 * token.
 */
char
param_scan(char const *word, char const **start, char const **end)
{
  static char const *prev;
  if (!word) word = prev;
  
  char ret = 0;
  *start = 0;
  *end = 0;
  for (char const *s = word; *s && !ret; ++s) {
    s = strchr(s, '$');
    if (!s) break;
    switch (s[1]) {
    case '$':
    case '!':
    case '?':
      ret = s[1];
      *start = s;
      *end = s + 2;
      break;
    case '{':;
      char *e = strchr(s + 2, '}');
      if (e) {
        ret = s[1];
        *start = s;
        *end = e + 1;
      }
      break;
    }
  }
  prev = *end;
  return ret;
}


/* Simple string-builder function. Builds up a base
 * string by appending supplied strings/character ranges
 * to it.
 */
char *
build_str(char const *start, char const *end)
{
  static size_t base_len = 0;
  static char *base = 0;

  if (!start) {
    /* Reset; new base string, return old one */
    char *ret = base;
    base = NULL;
    base_len = 0;
    return ret;
  }
  /* Append [start, end) to base string 
   * If end is NULL, append whole start string to base string.
   * Returns a newly allocated string that the caller must free.
   */
  size_t n = end ? end - start : strlen(start);
  size_t newsize = sizeof *base *(base_len + n + 1);
  void *tmp = realloc(base, newsize);
  if (!tmp) err(1, "realloc");
  base = tmp;
  memcpy(base + base_len, start, n);
  base_len += n;
  base[base_len] = '\0';

  return base;
}

/* Expands all instances of $! $$ $? and ${param} in a string 
 * Returns a newly allocated string that the caller must free
 */
char *
expand(char const *word)
{
  char const *pos = word;
  char const *start, *end;
  char c = param_scan(pos, &start, &end);
  build_str(NULL, NULL);
  build_str(pos, start);
 
  while (c) {
    if (c == '!') {
      char bgIntText[10]; /* Allocate big enough string */ 
      if (!bgPIDforExpand) build_str("", NULL);
      else {
        sprintf(bgIntText, "%jd", (intmax_t) bgPIDforExpand); /* Convert PID to string */
        build_str(bgIntText, NULL);
      }
    } 
    else if (c == '$') {
      char PID_text[10]; /* Allocate big enough string */
      sprintf(PID_text, "%d", getpid()); /* Get PID and convert to string */
      build_str(PID_text, NULL);
    }
    else if (c == '?') {
      char questionStatustext[10]; /* Allocate big enough string */
      if (!questionStatus) build_str("0", NULL);
      else if (WIFEXITED(questionStatus)) sprintf(questionStatustext, "%d", WEXITSTATUS(questionStatus)); /* If it exited, print exit status */
      else if (WIFSIGNALED(questionStatus)) sprintf(questionStatustext, "%d", WTERMSIG(questionStatus) + 128); /* If it terminated, print termination status plus 128 */
      build_str(questionStatustext, NULL);
    }

    else if (c == '{') {
      int endIndex = 0; /* Initialize variable to track end index */
      char firstWord[strlen(pos)];
      
      if (pos[0] != '$') { /* Case of parameter inside of words. Build initial word and find its ending index */
        for (int i = 0; i < strlen(pos); ++i) {
          if (pos[i] == '$') {
            endIndex = i;
            break;
          }
          else {
            firstWord[i] = pos[i];
          }
        }

        char *envNameNest = build_str(start + 2, end - 1); /* Build nested expansion */
        char *envNameInit = envNameNest + endIndex; /* Isolate getenv name */
        char *envName = getenv(envNameInit); /* Get the env variable */
        
        build_str(NULL, NULL);  /* Clear */
        build_str(firstWord, NULL); /* Add initial word to expnasion */
        if (envName == NULL) build_str("", NULL); /* If env variable is unset */
        else {build_str(envName, NULL);} /* Add env variable to expansion */
      }

      else { /* Case of leading "$" */
        char *envName = getenv(build_str(start + 2, end - 1)); /* Get env variable */
        if (multipleCommands > 0) { /* Case of multiple commands */
          if (envName == NULL) {
            char *extractedEnvName = NULL;
            extractedEnvName = malloc(strlen(start) - 3);
            for (int i = 2; i < strlen(start); ++i) {
              if (strcmp(&start[i], "}") == 0) break;
              extractedEnvName[i - 2] = start[i];
            }
            
            /* Build env name */
            envName = getenv(extractedEnvName); 
            free(extractedEnvName);
            build_str(NULL, NULL);
            build_str(envName, NULL);
            multiCommandComplete++;
          }
         
        }
        if (multiCommandComplete < 1) build_str(NULL, NULL); /* Clear */
        if (envName == NULL) build_str("", NULL); /* If env variable is unset */
        else {build_str(envName, NULL);} /* Add env variable to expansion */
      }
    }
    pos = end;
    c = param_scan(pos, &start, &end);
    build_str(pos, start);
  }
  multipleCommands++;
  return build_str(start, NULL);
}
