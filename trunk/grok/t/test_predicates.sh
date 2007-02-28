#!/bin/sh

. ./test.subr

rungrok() {
  perl ../grok -m "%NUMBER<10%" < input/predicates.gt10
  perl ../grok -m "%NUMBER>10%" < input/predicates.gt10
  perl ../grok -m "%GREEDYDATA~/test line/%" < input/predicates.regex
}

try_self $0
