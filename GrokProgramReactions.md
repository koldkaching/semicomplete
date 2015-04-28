#summary details about the 'reaction' values in grok configs.

# Reactions #

A reaction can be any string. Any %{value} values are replaced with the appropriated capture. You can also apply filter functions.

# What's in a %{name}? #

## Any named pattern in your capture ##

If your pattern was '%{FOO}', you can access the captured value as '%{FOO}' in your reaction.

If there are multiple %{FOO} patterns in your match, the first %{FOO} is given in the reaction.

## Any subname ##

If your pattern was '%{FOO:bar}' you can access the value with this shorthand '%{bar}' - or you can use the full name '%{FOO:bar}'

## Special names ##

  * @LINE - the full line of text from the match
  * @MATCH - the matched text
  * @START - the starting position of the match
  * @END - the ending position of the match
  * @LENGTH - the length of the match

## JSON ##

  * @JSON - hash of arrays, in json. The keys are the pattern names and the values are an array containing each pattern capture.
  * @JSON\_COMPLEX - complex json object with all data about the match.

JSON example:
```
  pattern: "%{SYSLOGBASE} .*"
  reaction: "%{@JSON}"

output:
{ "@LINE": "Nov  7 13:07:35 snack jls: hello", "@MATCH": "Nov  7 13:07:35 snack jls: hello",
  "SYSLOGBASE": "Nov  7 13:07:35 snack jls:", "SYSLOGDATE": "Nov  7 13:07:35", "MONTH": "Nov",
  "MONTHDAY": "7", "TIME": "13:07:35", "SYSLOGFACILITY": "", "INT:facility": "", "INT:priority": "",
  "IPORHOST:logsource": "snack", "HOSTNAME": "snack", "IP": "", "SYSLOGPROG": "jls", "PROG": "jls",
  "PID": "", "INT": "" }
```

JSON\_COMPLEX example:
```
  pattern: "%{SYSLOGBASE} .*"
  reaction: "%{@JSON_COMPLEX}"

output:
{ "grok": [ { "@LINE": { "start": 0, "end": 32, "value": "Nov  7 13:07:35 snack jls: hello" } }, 
            { "@MATCH": { "start": 0, "end": 32, "value": "Nov  7 13:07:35 snack jls: hello" } }, 
            { "SYSLOGBASE": { "start": 0, "end": 26, "value": "Nov  7 13:07:35 snack jls:" } }, 
            { "SYSLOGDATE": { "start": 0, "end": 15, "value": "Nov  7 13:07:35" } }, 
            { "MONTH": { "start": 0, "end": 3, "value": "Nov" } }, 
            { "MONTHDAY": { "start": 5, "end": 6, "value": "7" } }, 
            ...
            { "INT": { "start": -1, "end": -1, "value": "" } } 
          ] }
```

# Reaction Filters #

Filters are used for translating a capture value before sending to the reaction shell.

Below is an example that will use the 'shellescape' filter to escape all characters in a value to as to be safe in a shell context.
```

program {
  load-patterns: "patterns/base"
  exec "echo '$something \"testing\"'"

  match {
    pattern: ".*"
    reaction: "Shell escaped: %{@LINE|shellescape}"
  }

  match {
    pattern: ".*"
    reaction: "json: { myvalue: \"%{@LINE|jsonencode}\" }"
  } 

  match {
    pattern: ".*"
    reaction: "echo %{@LINE|shellescape}"
    shell: "/bin/sh"
    flush: yes
  }
}
```

## Available Filters ##

  * shellescape - escapes all characters necessary to make the string safe in non-quoted a shell argument
  * shelldqescape - escapes characters necessary to be safe within doublequotes in a shell.
  * jsonencode - makes the string safe to represent in a json string (escapes according to json.org recommendations)