# Watching logs for errors #

Uses: since, cron, grok.

For this example,you could do this all in grok, but I find it works easier using cron given cron's builtin email capability.

```
program {
  exec "since /var/log/messages"

  # Ignore certain messages
  match {
    pattern: "this is not an error"

    # Silence the output and ensure no further matches are attempted.
    reaction: none        # no output
    break-if-match: yes   # don't continue to the next match
  }

  match {
    pattern: "error"
  }
}
```

Have cron run this periodically. The results will be:
  * output only matched patterns that weren't skipped in the earlier match blocks
  * exits nonzero if there were no unignored matches. (exit zero on any output, like grep)