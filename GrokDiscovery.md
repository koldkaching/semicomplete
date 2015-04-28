# Introduction #

Grok makes regular expressions easier to manage, but what if you didn't have to write them at all? Grok can help you with that from libgrok or from ruby (jls-grok)

Given an input, grok can try to come up with a pattern that best matches your input.

**NOTE**: Grok ships with a tool called 'discogrok' that you can use just like the ruby example below:

`discogrok --patterns ./path/to/patterns/dir`

reads from stdin, outputs discovered patterns.

# Examples (from ruby) #

Code (grokdisco.rb):
```
require "rubygems"
require "grok"

g = Grok.new

# Load our patterns
Dir["/home/jls/projects/logstash/patterns/*"].each { |p| g.add_patterns_from_file(p) }
#g.logmask = (1<<31)-1

$stdin.each do |line|
  line.chomp!
  puts "Line: #{line}" 
  pattern = g.discover(line)
  puts "Pattern: #{pattern}"
  g.compile(pattern)
  puts g.match(line).captures.inspect
end
```


Running it with some examples:

```
% echo "1.2.3.4" | ruby grokdisco.rb
Line: 1.2.3.4
Pattern: \Q\E%{HOST}\Q\E
{"HOST"=>["1.2.3.4"], "HOSTNAME"=>["1.2.3.4"]}

% echo "'hello world'" | ruby grokdisco.rb     
Line: 'hello world'
Pattern: \Q\E%{QUOTEDSTRING}\Q\E
{"QUOTEDSTRING"=>["'hello world'"]}

% echo "http://www.google.com:80/search?q=testing&foo=bar" | ruby grokdisco.rb
Line: http://www.google.com:80/search?q=testing&foo=bar
Pattern: \Q\E%{URI}\Q\E
{"URI"=>["http://www.google.com:80/search?q=testing&foo=bar"], "URIHOST"=>["www.google.com:80"], "USERNAME"=>[""], "USER"=>[""], "URIPARAM"=>["?q=testing&foo=bar"], "POSINT"=>["80"], "IP"=>[""], "URIPATH"=>["/search"], "URIPROTO"=>["http"], "IPORHOST"=>["www.google.com"], "URIPATHPARAM"=>["/search?q=testing&foo=bar"], "HOSTNAME"=>["www.google.com"]}

 % echo "Nov 13 00:30:48 snack nagios3: Auto-save of retention data completed successfully." | ruby grokdisco.rb
Line: Nov 13 00:30:48 snack nagios3: Auto-save of retention data completed successfully.
Pattern: \Q\E%{SYSLOGLINE}\Q\E
{"MINUTE"=>["30", "", ""], "GREEDYDATA:message"=>["Auto-save of retention data completed successfully."], "TIMESTAMP_ISO8601:timestamp8601"=>[""], "SECOND"=>["48", ""], "POSINT:pid"=>[""], "POSINT:priority"=>[""], "PROG:program"=>["nagios3"], "POSINT:facility"=>[""], "HOUR"=>["00", "", ""], "TIME"=>["00:30:48"], "MONTH"=>["Nov"], "SYSLOGLINE"=>["Nov 13 00:30:48 snack nagios3: Auto-save of retention data completed successfully."], "SYSLOGHOST:logsource"=>["snack"], "SYSLOGFACILITY"=>[""], "IP"=>[""], "SYSLOGPROG"=>["nagios3"], "MONTHNUM"=>[""], "MONTHDAY"=>["13", ""], "SYSLOGBASE2"=>["Nov 13 00:30:48 snack nagios3:"], "IPORHOST"=>["snack"], "ISO8601_TIMEZONE"=>[""], "YEAR"=>[""], "SYSLOGTIMESTAMP:timestamp"=>["Nov 13 00:30:48"], "HOSTNAME"=>["snack"]}
```