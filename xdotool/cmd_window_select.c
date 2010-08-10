#include "xdo_cmd.h"

int cmd_window_select(context_t *context) {
  Window window = 0;
  int ret;
  char *cmd = context->argv[0];

  int c;
  static struct option longopts[] = {
    { "help", no_argument, NULL, 'h' },
    { 0, 0, 0, 0 },
  };
  static const char *usage = "Usage: %s\n";
  int option_index;

  while ((c = getopt_long_only(context->argc, context->argv, "+h",
                               longopts, &option_index)) != -1) {
    switch (c) {
      case 'h':
        printf(usage, cmd);
        consume_args(context, context->argc);
        return EXIT_SUCCESS;
        break;
      default:
        fprintf(stderr, usage, cmd);
        return EXIT_FAILURE;
    }
  }

  consume_args(context, optind);

  ret = xdo_window_select_with_click(context->xdo, &window);

  if (ret) {
    fprintf(stderr, "xdo_window_select_with_click reported an error\n");
  } else {
    /* only print if we're the last command */
    if (context->argc == 0) {
      window_print(window);
    }
    window_save(context, window);
  }

  return ret;
}

