#define _POSIX_SOURCE
#define _GNU_SOURCE

#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/user.h>
#include <sys/ptrace.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>

#include "child.h"
#include "constants.h"
#include "run.h"
#include "utils.h"

/**
 * run the specific process
 */
void child_process(struct Config *config)
{
  int input_fd = -1;
  int output_fd = -1;
  int err_fd = -1;
  int null_fd = open("/dev/null", O_WRONLY);

  if (config->cpu_time_limit != RESOURCE_UNLIMITED)
  {
    // CPU time limit in seconds.
    log_debug("set cpu_time_limit");
    struct rlimit max_time_rl;
    max_time_rl.rlim_cur = max_time_rl.rlim_max = (rlim_t)((config->cpu_time_limit + 1000) / 1000);
    if (setrlimit(RLIMIT_CPU, &max_time_rl))
      CHILD_ERROR_EXIT("set RLIMIT_CPU failure");
  }

  // 注意，设置 memory_limit 会导致有些程序 crash，比如 python, node
  if (config->memory_limit != RESOURCE_UNLIMITED)
  {
    if (config->memory_check_only == 0)
    {
      // The maximum size of the process's virtual memory (address space) in bytes.
      log_debug("set memory_limit");

      struct rlimit max_memory_rl;
      // 为了避免代码是正确的，但是因为超过内存 oom 而被判定为 re。
      // 如果程序占用低于两倍，最后再重新检查内存占用和配置的关系，就可以判定为超内存而不是 re，如果超过两倍，那就真的 re 了（可能会被 kill）。
      max_memory_rl.rlim_max = max_memory_rl.rlim_cur = config->memory_limit * 1024 * 2;
      if (setrlimit(RLIMIT_AS, &max_memory_rl))
        CHILD_ERROR_EXIT("set RLIMIT_AS failure");
    }
  }

  // log_debug("set max_output_size");
  // // set max output size limit
  // struct rlimit max_output_size;
  // max_output_size.rlim_cur = max_output_size.rlim_max = MAX_OUTPUT;
  // if (setrlimit(RLIMIT_FSIZE, &max_output_size) != 0)
  // {
  //   CHILD_ERROR_EXIT("set RLIMIT_FSIZE failure");
  // }

  // 重定向 标准输出IO 到相应的文件中
  if (config->in_file)
  {
    input_fd = open(config->in_file, O_RDONLY | O_CREAT, 0700);
    if (input_fd != -1)
    {
      log_debug("open in_file");
      if (dup2(input_fd, STDIN_FILENO) == -1)
      {
        CHILD_ERROR_EXIT("input_fd dup error");
      }
    }
    else
    {
      CHILD_ERROR_EXIT("error open in_file");
    }
  }
  else
  {
    log_info("in_file is not set");
  }

  if (config->stdout_file)
  {
    output_fd = open(config->stdout_file, O_WRONLY | O_CREAT | O_TRUNC, 0700);
    if (output_fd != -1)
    {
      log_debug("open stdout_file");
      if (dup2(output_fd, STDOUT_FILENO) == -1)
      {
        CHILD_ERROR_EXIT("output_fd dup error");
      }
    }
    else
    {
      CHILD_ERROR_EXIT("error open stdout_file");
    }
  }
  else
  {
    log_info("stdout_file is not set, default redirect to /dev/null");
    dup2(null_fd, STDOUT_FILENO);
  }

  if (config->stdout_file)
  {
    err_fd = output_fd;
    if (err_fd != -1)
    {

      if (dup2(err_fd, STDERR_FILENO) == -1)
      {
        CHILD_ERROR_EXIT("err_fd");
      }
    }
    else
    {
      CHILD_ERROR_EXIT("error open err_fd");
    }
  }
  else
  {
    log_info("err_out_file is not set, default redirect to /dev/null");
    dup2(null_fd, STDERR_FILENO);
  }

  log_debug("exec %s", config->cmd[0]);

  char *envp[] = {NULL};
  execvpe(config->cmd[0], config->cmd, envp);
  CHILD_ERROR_EXIT("exec cmd error");
}