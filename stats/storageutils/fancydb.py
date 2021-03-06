#!/usr/bin/env python

from __future__ import with_statement
import simpledb
import time
import threading
import numpy as N

# rrdtoolisms
RULE_AVERAGE=1
RULE_MIN=2
RULE_MAX=3
RULE_LAST=4
RULE_TOTAL=5
RULE_RATE=6
RULE_STDDEV=7
RULE_VARIANCE=8

STEP_COUNT=1
STEP_TIME=2

OP_GET=1
OP_SET=2
OP_DELETE=3

class DuplicateRuleTarget(Exception):
  pass

def StringToType(string):
  return {
    "average": RULE_AVERAGE,
    "min": RULE_MIN,
    "max": RULE_MAX,
    "last": RULE_LAST,
    "total": RULE_TOTAL,
    "rate": RULE_RATE,
    "stddev": RULE_STDDEV,
    "variance": RULE_VARIANCE,
    "time": STEP_TIME,
    "count": STEP_COUNT,
  }[string]

class RuleEvaluator(threading.Thread):
  def __init__(self):
    threading.Thread.__init__(self)
    self._notifications = dict()
    self._lock = threading.Condition(threading.RLock())
    self._done = threading.Event()
    self._done.clear()

  def run(self):
    done = False
    while not done:
      # Wait for data.
      with self._lock:
        #print "evaluator lock: %s/%s" % (self._lock, threading.currentThread())
        while len(self._notifications) == 0 and not self._done.isSet():
          #print "evaluator wait: %s" % threading.currentThread()
          #print len(self._notifications) == 0, not self._done.isSet()
          #print "Lock: %s" % self._lock
          self._lock.wait()
        #print "evaluator (wait finished, lock is mine)"
        done = (len(self._notifications) == 0 and self._done.isSet())
        notifications = self._notifications
        self._notifications = dict()
      #print "evaluator released: %s" % threading.currentThread()
      self.Process(notifications)
      self._done.wait(1)

  def EvaluatorNotify(self, notification):
    #print "notify lock attempt by %s" % threading.currentThread()
    with self._lock:
      self._notifications.setdefault(notification, 0)
      self._notifications[notification] = 1
    #print "notify release: %s" % threading.currentThread()

  def Process(self, notifications):
    #print "Processing %d notifications" % (len(notifications))
    for n in notifications:
      #print "proc loop enter : %s" % threading.currentThread()
      rule = n._rule
      rule.Evaluate(n._start)
      #print "proc loop bottom : %s" % threading.currentThread()

  def Finish(self):
    self._done.set()
    #print "evaluator finish Lock: %s" % self._lock
    #print "evaluator finish RuleEvaluator self: %s" % self
    with self._lock:
      self._lock.notifyAll()

class RuleNotification(object):
  def __init__(self, rule, start, end):
    self._rule = rule
    self._start = start
    self._end = end

  def __hash__(self):
    return hash(self._rule) + hash(self._start) + hash(self._end)

  def __cmp__(self, other):
    return cmp(self._rule, other._rule) \
           or cmp(self._start, other._start) \
           or cmp(self._end, other._end)

class Rule(object):
  init_attrs = ("_source", "_target", "_ruletype", "_xff", "_steps", "_step_type")
  restore_attrs = ("_hits",)

  def __init__(self, source, target, ruletype, xff, steps, step_type):
    self._source = source
    self._target = target
    self._ruletype = ruletype
    self._xff = int(xff)
    self._steps = int(steps)
    self._step_type = step_type

    # Rule Evaluator
    self._evaluator = None
    self._db = None

    if isinstance(self._ruletype, str):
      self._ruletype = StringToType(self._ruletype)
    if isinstance(self._step_type, str):
      self._step_type = StringToType(self._step_type)

    if self._step_type == STEP_TIME:
      self._next_timestamp = 0

    self._dispatch = {
      RULE_AVERAGE: self.ComputeAverage,
      RULE_MIN: self.ComputeMin,
      RULE_MAX: self.ComputeMax,
      RULE_LAST: self.ComputeLast,
      RULE_TOTAL: self.ComputeTotal,
      RULE_RATE: self.ComputeRate,
      RULE_STDDEV: self.ComputeStandardDeviation,
      RULE_VARIANCE: self.ComputeVariance,
    }

    self._hits = 0

  def ComputeAverage(self, values, timestamps):
    return N.mean(values)

  def ComputeMin(self, values, timestamps):
    return N.min(values)

  def ComputeMax(self, values, timestamps):
    return N.max(values)

  def ComputeTotal(self, values, timestamps):
    return N.sum(values)

  def ComputeLast(self, values, timestamps):
    return values[0]

  def ComputeRate(self, values, timestamps):
    """ This function assumes the value is a counter that never decreases.
    If it does decrease, it considers the counter to have reset.
    """
    # Check if the counter has reset
    if len(values) < 2:
      return 0
    if values[0] < values[-1]:
      print "RESET: %s" % values
      values[-1] = 0
    delta = float(values[0] - values[-1])
    duration = float(timestamps[0] - timestamps[-1]) / 1000000
    #print "(%d) %d / %d = %f" % (len(values), delta, duration, delta / duration)
    return delta / duration

  def ComputeStandardDeviation(self, values, timestamps):
    return N.std(values)

  def ComputeVariance(self, values, timestamps):
    return N.var(values)

  def SetDB(self, db):
    self._db = db

  def __hash__(self):
    return hash(self._target)

  def SetEvaluator(self, evaluator):
    self._evaluator = evaluator

  def ToDict(self):
    attrs = self.init_attrs + self.restore_attrs
    data = {}
    for i in attrs:
      data[i] = getattr(self, i)
    return data

  @classmethod
  def CreateFromDict(self, data):
    """ Factory-type method to create a Rule instance from a dictionary.

    For a Rule instance 'obj', 
      Rule.CreateFromDict(obj.ToDict()) == obj
    """
    args = {}
    for i in self.init_attrs:
      args[i[1:]] = data[i]
    obj = Rule(**args)
    for i in self.restore_attrs:
      setattr(obj, i, data[i])
    return obj

  def RuleNotify(self, unused_args, db, timestamp):
    if self._step_type == STEP_COUNT:
      raise NotImplemented("COUNT not implemented for rules")
    if not self._evaluator:
      raise NotImplemented("No evaluator for this rule. Cannot notify.")
    (start, end) = self.GetTimeRangeForTimestamp(timestamp)
    notification = RuleNotification(self, start, end)
    self._evaluator.EvaluatorNotify(notification)

  def GetTimeRangeForTimestamp(self, timestamp):
    period = self._steps * 1000000
    start = timestamp - (timestamp % period)
    end = start + period
    return (start, end)

  def Evaluate(self, timestamp, *args):
    #print "Evaluate called"
    if self._step_type == STEP_TIME:
      self.EvaluateTime(timestamp, *args)
    elif self._step_type == STEP_COUNT:
      self.EvaluateCount(timestamp, *args)

  def EvaluateCount(self, timestamp, *args):
    assert(False)
    self._hits += 1
    if self._hits < self._steps:
      return
    self._hits = 0
    count = 0
    values = []
    last_timestamp = None
    for entry in self._db.ItemIteratorByRows([self._source]):
      assert entry.row == self._source
      count += 1
      values.append(entry.value)
      last_timestamp = entry.timestamp
      if count == self._steps:
        break;

  def EvaluateTime(self, timestamp, *args):
    #print "time eval"
    #for i in self._db.ItemIterator():
      #print "%s: %s" % (timestamp, i)
    (start, end) = self.GetTimeRangeForTimestamp(timestamp)
    #print "range: (%s) (%s, %s)" % (timestamp/1000000, end/1000000, start/1000000)
    values = []
    timestamps = []
    last_timestamp = None
    count = 0
    #print "Eval on row %s" % self._source
    for entry in self._db.ItemIteratorByRows([self._source], start_timestamp=start, end_timestamp=end):
      count += 1
      if entry.timestamp > end:
        print "WTF cont"
        continue
      if entry.timestamp < start:
        print "WTF break"
        break
      #print "found: %s" % (entry.timestamp / 1000000)
      values.append(entry.value)
      timestamps.append(entry.timestamp)
      last_timestamp = entry.timestamp

    if last_timestamp is not None:
      result = self._dispatch[self._ruletype](values, timestamps)
      #print "Set: %s@%s => %s" % (self._target, end, result)
      self._db.Set(self._target, result, end)

class FancyDB(simpledb.SimpleDB):
  def __init__(self, db_path, encode_keys=False):
    simpledb.SimpleDB.__init__(self, db_path, encode_keys=False)
    self._row_listeners = {}
    self._ruledb = simpledb.SimpleDB(db_path, db_name="rules", encode_keys=False)
    self._rules = {}
    self._evaluator = RuleEvaluator()
    self._evaluator.start()

  def Open(self, create_if_necessary=True):
    simpledb.SimpleDB.Open(self, create_if_necessary)
    self._ruledb.Open(create_if_necessary=True)
    self.LoadRules()

  def Close(self):
    self.debug("Closing fancydb %s" % self._db_name)
    while self._evaluator.isAlive():
      self._evaluator.Finish()
      self.debug("Waiting for evaluator thread to finish")
      self._evaluator.join(1)
    self.debug("Closing simpledb")
    simpledb.SimpleDB.Close(self)
    self.debug("Closing ruledb")
    self._ruledb.Close()
    self.debug("Done closing")

  def PurgeDatabase(self):
    simpledb.SimpleDB.PurgeDatabase(self)
    self._ruledb.PurgeDatabase()

  def LoadRules(self):
    for entry in self._ruledb.ItemIterator():
      self.AddRule(Rule.CreateFromDict(entry.value), save_rule=False)

  def AddRule(self, rule, save_rule=True):
    if rule._target in self._rules:
      raise DuplicateRuleTarget("Attempt to add rule creating rows '%s'"
                                "aborted. A rule already exists to "
                                "generate this row." % rule._target)
    #print "Add rule: %s => %s" % (rule._source, rule._target)
    rule.SetEvaluator(self._evaluator)
    rule.SetDB(self)
    self.AddRowListener(rule._source, rule.RuleNotify)
    self._rules[rule._target] = rule
    if save_rule:
      self._ruledb.Set("rule", rule.ToDict())

  def AddRowListener(self, row, callback, *args):
    self._row_listeners.setdefault(row, [])
    self._row_listeners[row].append((callback, args))
    self.debug("Added row listener for row '%s': %s" % (row, callback.__name__))

  def Set(self, row, value, timestamp=None):
    #print "%s @ %s" % (row, timestamp)
    simpledb.SimpleDB.Set(self, row, value, timestamp)

    #if row in self._row_listeners:
      #for (listener, args) in self._row_listeners[row]:
        #listener(args, self, timestamp)

    #print row, self._row_listeners.get(row, [])
    #print self._row_listeners
    for (listener, args) in self._row_listeners.get(row, []):
      listener(args, self, timestamp)

def test():
  import random
  import time
  import tempfile
  f = tempfile.mkdtemp()
  db = FancyDB(f, encode_keys=False)
  db.Open(create_if_necessary=True)
  start = int(time.time())

  r_hourly = Rule("hits.minute", "hits.mean.1hour", RULE_TOTAL, 1, 60*60, STEP_TIME)
  #r_daily = Rule("hits.mean.1hour", "hits.mean.1day", RULE_TOTAL, 1, 60*60*24, STEP_TIME)
  r_daily = Rule("hits.minute", "hits.mean.1day", RULE_TOTAL, 1, 60*60*24, STEP_TIME)
  db.AddRule(r_hourly)
  db.AddRule(r_daily)

  # Insert data once a minute
  count = 0
  s = time.time()
  #for i in range(24 * 60 * 7):
  for i in range(100000):
    if count % 10000 == 0:
      print count
    count += 1
    #db.Set("hits.minute", random.randint(1000,2000), (start + (60 * i)) * 1000000)
    db.Set("hits.minute", count, (start + (60 * i)) * 1000000)
  print count / (time.time() - s)

  #for i in db.ItemIteratorByRows(["hits.mean.1hour"]):
    #print i
  #for i in db.ItemIteratorByRows(["hits.mean.1day"]):
    #print i

  db.Close()

#test()
