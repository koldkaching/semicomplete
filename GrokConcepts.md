# Overview #

Grok exists to help you do fancier pattern matching with less effort. Grok lets you build (or use existing) sets of named regular expressions and then helps you use them to match strings.

The goal is to bring more semantics to regular expressions and allow you to express ideas rather than syntax. Further, it lets you bring "Don't Repeat Yourself" philosophy to pattern matching, log analysis, etc, by leveraging existing patterns against new data.

As an example, look at this sample log. What do you see?
```
Nov  1 21:14:23 scorn kernel: pid 84558 (expect), uid 30206: exited on signal 3
```

In order, your brain reads a timestamp, a hostname, a process or other
identifying name, a number, a program name, a uid, and an exit message. You might represent this in words as:

```
TIMESTAMP HOST PROGRAM: pid NUMBER (PROGRAM), uid NUMBER: exited on signal NUMBER
```

All of these can be represented by regular expressions. Grok comes with a bunch of pre-defined patterns to make getting started easier, including syslog patterns that help with the above. In grok, this pattern looks like:

```
%{SYSLOGBASE} pid %{NUMBER:pid} \(%{WORD:program}\), uid %{NUMBER:uid}: exited on signal %{NUMBER:signal}
```

All of the base grok patterns are in uppercase for style consistency. Each thing in %{} is evaluated and replaced with the regular expression it represents.

Given that you may have multiple patterns with the same name, like NUMBER above, you can tag individual patterns with an additional name (like 'pid' or 'program' above) to make later inspection easier.

_You will never again have to count parentheses to figure out what capture group number you need._

You can also nest patterns. SYSLOGBASE is a good example. SYSLOGBASE is actually:
```
%{SYSLOGDATE} (?:%{SYSLOGFACILITY} )?%{IPORHOST:logsource} %{SYSLOGPROG}:
```

SYSLOGDATE and SYSLOGPROG also have nested patterns:
  * SYSLOGPROG = %{PROG}(?:\[%{PID}\])?
  * SYSLOGDATE = %{MONTH} +%{MONTHDAY} %{TIME}
  * MONTH = \b(?:Jan(?:uary)?|Feb(?:ruary)?|Mar(?:ch)?|Apr(?:il)?|May|Jun(?:e)?|Jul(?:y)?|Aug(?:ust)?|Sep(?:tember)?|Oct(?:ober)?|Nov(?:ember)?|Dec(?:ember)?)\b
  * etc ...

This means anything matching %{SYSLOGDATE} will have all of the nested pattern names accessible as captures. For example, if you wanted to grab the MONTH value from the pattern match, that's trivial.

Want to know more? The grok program tutorial will give you a good overview of grok's features:
