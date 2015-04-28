#summary A tutorial covering the features of the grok program

# Hello World #

helloworld.grok:
```
program {
  # Tell grok to run this command
  exec "echo Hello World"

  match {
    # match anything
    pattern: ".*"

    # the reaction is what to emit on a match
    reaction: "Got: %{@LINE}"
  }
}
```

Output:
```
% grok -f helloworld.grok
Got: Hello World
```

# Specifying patterns, or %{what?} #

In grok patterns, you specify a named pattern with %{name}, where 'name' is the name of your pattern. Grok comes with a handful of patterns already defined in the [grok/patterns/base file](http://code.google.com/p/semicomplete/source/browse/grok/patterns/base#).

For example, a grok match that should capture the IP addresses from the output of ifconfig:
```
program {
  load-patterns: "patterns/base"
  exec "ifconfig"

  match {
    pattern: "%{IP}"
    reaction: "Found: %{IP}"
  }
}
```

The output looks like this:
```
% grok -f samples/ifconfig.grok
Found: 192.168.0.97
Found: 127.0.0.1
Found: 192.168.250.1
Found: 192.168.251.1
```

The same syntax is used for specifying patterns in a match as for fetching the captured result in a reaction.

See also GrokProgramReactions.

# Predicates #

You can apply predicates (additional checks) to a pattern.

As a trivial example, let's take the output of seq(1) (or jot, for freebsd) and apply a numerical filter that restricts the output to numbers greater than 20.

```
program {
  load-patterns: "patterns/base"
  exec "seq 25"

  match {
    pattern: "%{NUMBER > 20}"
    reaction: "Found: %{NUMBER}"
  }
}
```

Output:
```
% grok -f samples/numberpredicate.grok
Found: 21
Found: 22
Found: 23
Found: 24
Found: 25
```

See GrokPredicates for information on supported predicates, which include the regexp =~ match operator, too!