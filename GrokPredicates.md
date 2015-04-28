# Predicates #

Predicates let you add additional requirements to your matches. They allow you to further restrict a match without having to rewrite a pattern.

Predicates behave in a very important way: they directly affect the matching while the expression is being executed. For example, to find in a string of numbers, a number that is greater than 20, we easily represent this in grok as: %{NUMBER > 20}.

```
program {
  load-patterns: "patterns/base"
  exec "echo 3 5 19 33 10"

  match {
    pattern: "%{NUMBER > 20}"
    reaction: "Got number: %{NUMBER}"
  }
}
```

Output:
```
Got number: 33
```

Regular expression engines don't generally support what we have done above. If you wanted the above in Python or Ruby regex, you would have to write a custom expression that matches a number > 20, which isn't exactly trivial to write. Alternately, you try a number (\d+) pattern and loop over all matches until you find one greater than 20, which sucks and doesn't really work at all in a more complex expression.

## > < <= >= == != ##

These are numerical comparison predicates.

## $> $< $<= $>= $== $!= ##

These are string comparison predicates.

```
program {
  load-patterns: "patterns/base"
  exec "echo hello; echo world"

  match {
    pattern: "%{WORD $> test}"
    reaction: "Found: %{WORD}"
  } 
} 
```

Output:
```
% grok samples/strpredicate.grok 
Found: world
```



## =~ and !~ (regular expression predicates) ##

The =~ and !~ operators work the same way they do in Perl and Ruby with respect to matching strings against regular expressions.

The exact syntax is %{name =~ /regexp/}.

If you want to match an IP that starts with 192.168, that's easy:
```
program {
  load-patterns: "patterns/base"
  exec "ifconfig"

  match {
    pattern: "%{IP =~ /^192.168/}"
    reaction: "Found: %{IP}"
  }
}
```

Output:
```
% grok samples/ip-predicate.grok   
Found: 192.168.0.97
```

Without the '=~ /^192.168/' predicate, our output is:
```
Found: 192.168.0.97
Found: 127.0.0.1
```

You can also nest another named pattern: