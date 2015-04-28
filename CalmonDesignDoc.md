#summary Design document for the Calmon monitoring project

# Calmon - a monitoring system #



# Introduction #

This is a work in progress. Many, many things are yet undefined. You can help. Email comments to jls-calmon@semicomplete.com

## Background ##

There are things I like about nagios. There are things I do not like. From my
experiences, it solves about half of my monitoring needs, not including the
need for other tools: munin, collectd, cacti, monit, etc. Many other monitoring
systems (some commercial, some free) are simply forks of Nagios and provide
little on top of what nagios already provides. Those that are not direct
descendants of Nagios fall to other design problems such as only supporting
discovery instead of directly defining nodes or devices (see Source of Truth
below). Few address notifications, acknowledgements, and escalations. Fewer
still support aggregates or estimations easily. This goes way beyond the
problems that any given monitoring tool has a poor dashboard or UI.

Monitoring is hard to get right. Monitoring system metrics is important.
Monitoring business metrics is also important.

A complex computer system will need to monitor things as well as groups of things
and aggregate data from multiple sources to correlate failures with causes.
A good monitoring system allows you to check for health, test assumptions,
correlate data, and automate reactions.

There are many monitoring tools ranging from narrow to wide scopes. Some are
complete, others are incomplete. None I have found have enough of the features I
want in the basic system.

Some monitoring tools help you from check to notification, but don't adequately
handle common alert methods and don't take feedback (acknowledgements, etc)
easily.

## Goals ##

  * Improve the status quo of monitoring.
  * Make easy things easy, hard things possible.
  * Apply things learned from the past decades of monitoring tools.
  * Automatable
  * Pluggable

## Nomenclature ##

  * node, host, device? - a network addressable system that can be tested
  * service, class, tag? - a collection of tests, can be applied to a node
  * test - a single test to run against a host or service.

XXX How do we define a group of hosts? Or a group of host:service pairs?

## Considerations ##

The following are considerations made which impact the design and feature set
of Calmon.

### Source of Truth ###

You need a source of truth to define what hosts have which services, and which
services are important, and which devices are unimportant, etc. In Nagios, for
example, you define truth in the config files by defining hosts, services,
commands, contacts, schedules, etc. In OpenNMS, some truth comes from the network
through discovery (ping sweeps to find hosts, etc). In other systems you might
have a SQL database that has devices, network information, etc.

Truth is important for any automated system to reflect the intentions of its
users. Truth comes from humans. Truth is how you define the system.

There are two main philosophies about where this source of truth comes from:

First philosophy is that the network serves as your source of truth. You know
that a switch's port 34 belongs to 'host foo' because you put that in the port
description. You know that the server foo.example.com is a fileserver because
you noted that in /etc/motd, or because nfsd is active. In this philosophy, any
truth is knowable by simply asking systems about themselves because you
spread your truth across all nodes and devices in your network of datacenters,
systems, etc.

The second philosophy is to centrally store your source of truth. Any truth
queries (What host am I? What packages do I need? What vlans belong on this
switch port?) are answerable from this central authority. As above, server
foo.example.com is a fileserver because you told the central truth source that
it is a fileserver and thus should have nfsd running and configured
appropriately. In practice, centralized truth can range from a simple yaml file
to an database-backed web app.

How to store truth is out of scope for Calmon. Thus, since there are two fairly
different philosophies, it is not realistic for Calmon to provide either method
of truth storage. Instead, Calmon should provide a way to plug your own source
of truth into it, feeding it host, service, test, and other information.

### Push vs Pull ###

In this context, push means that one networked process sends data to another
process; pull means that one networked process requests data of another
process. In a monitoring system, this usually describers how tests or checks
are invoked and how results are moved around.

Fundamentally, the end result is the same: data is transferred. The only
decision is then to choose which node initiates the connection.

Both push and pull are valid methods. In some situations, one may be required
due to firewall rules. In other situations, it make make more sense to use pull
if only to centralize where scheduling decisions are made (nagios does this,
for example).

From this point of view, it seems necessary to support both push and pull for
sending data and requests between monitoring nodes.

## Scaling Perforamance ##

Scale everything. Do it well, or reasonably well.

This includes, but is not necessarily limited to:
  * Should work well with 1, 10, 1000, or 10000 nodes.
  * Executing lots of tests on lots of nodes with reasonable performance and low impact.
  * Preventing pager storms
  * Multiple teams/views for notification and dashboarding
  * Don't bottleneck on the dashboard: Crappy reporting and visualization is not acceptable.
  * Don't skimp on search abilities.
  * Provide good APIs

## Scaling Usability ##

Small/simple infrastructures have different needs than large or complex infrastructures.

In order to support both, we can treat all infrastructures as large and
complex, since small and/or simple is a subset. The way to shield the
complexity in configuration for a simple environment is to provide a healthy
set of sane defaults.

This means we'll need to provide a standard library and apply some default
decisions to some of our objects (hosts/devices, contacts, notifications, etc).

For any non-trivial number of things monitored, it must be easy to search for
things in the system.

Rollup and aggregates must be easy to use. That is, it should be easy to
configure a thing called 'frontends' that encompasses all your frontends.
Further, it should be easy to view this aggregate in the dashboard and query it
from the API.

# Configuration Targets #

TBD

I want to target the most common system infrastructures in use today. I also
want to make it easy/possible to do less common infrastructures.

Anyway, I need more descriptions of infrastructures and services and how you
would ideally monitor them before I can really pick a common set of targets.

Some infrastructure scenarios:
  * Multitenant hosting services (multiple users with restricted views). The biggest feature here would be ACLs for each tenant and possible multiple accounts with different restrictions for a single tenant.
  * Networking only (ie, you have a netops team). Good SNMP support required.
  * Services only (ie, you have an SRE team that doesn't care about hosts or network, only services).
  * Hardware/facility only (ie, you have a dedicated hardware support team). Temperature, SNMP, etc.
  * Hosts, Networking, and Services (ie; ops/sa group monitoring everything in production)
  * Corp/IT services (internal support/helpdesk/other services).
  * Cloud stuff, like EC2. (see below)

The main differences between cloud and noncloud is, in my mind, simply a
difference in your source(s) of truth. On EC2, for example, your list of host and
other data is queryable via the API.

# Architecture #

## Components ##

Monitoring systems generally include a few active components:
  * Agent -  Runs tests on a node, reports data. May act on data.
  * Server - schedules, dispatches, and records test data. Acts on data.
  * Proxy - Forwards requests between agents and servers
  * Dashboard - Provides a view on the monitored data.
  * API - API for querying statuses, adding silences, making notes, etc.

Other components:
  * notification and escalation management

Undecided ownership:
  * Automatic corrective action (such as a service restart, etc). Should this be configured on the agent, or configured on the server which tells the agent which action to take?

### Agent ###

The agent allows you to run tests and do collection on individual nodes. This
is similar to Nagios NRPE in idea.

It also seems reasonable that an agent could be standalone (a la NRPE) or
embedded into any number of applications. Embedded agents would give you
invaluable white-box testing in your live production environment.

Agents can push test reports or have test reports requested.

Pull scenarios make sense for periodic checks orchestrated by a master server.

Push scenarios make sense for a local agent to push report information as soon
as it has it. For example, you could embed agent functionality into your own
software that, upon known-important exceptions or failures, will push reports
up to the calmon server which can result in an immediate notification.

### Server ###

Receives or collects (push vs pull)  data from agents and tests. This data is
logged for historical viewing.

The server should also manage notifications, silences, escalations, etc.

The server can also act like a proxy.

### Dashboard ###

  * Meaningful visuals
  * Monitoring status
  * Managing alerts/notifications/silences/etc.
  * Search interface

### API ###

An API, probably networked, is required for allowing calmon to be easily used
and abused by other tools.

API should also provide search features.

### Proxy ###

Proxies act as store-and-forward systems between agents and servers. They are
not necessarily needed in all situations.

### Escalation Management ###

Very common to have a situation where you have a primary and secondary on-call
person. If the primary does not acknowledge a page within a time threshold, it
should escalate to the secondary person. Other cases may eventually want a
tertiary.

Escalation should continue trying to alert people in the rotation (primary,
secondary, etc) until someone acknowledges.

Additionally, acknowledging a page doesn't mean you acknowledge a problem.

### Notification Management ###

Sometimes problem in a single system  (such as database failures) can cause
problems in multiple applications, causing massive floods of notifications from
monitoring systems. This sucks.

Some options are available:
  * Allow easy, temporary system-wide notification silencing.
  * Detect flooding and attempt to thwart by summarizing or by throttling. A summary could include a message like "SUPERCRITICAL - X services failed in last Y minutes"

## Schema ##

In general, we will be checking various things for health. Each check we will
call a test. From software engineering unit testing, you have a test that is
part of a test suite. Multiple test suites may exist for a given product.

In Calmon:

  * Hosts have Services. (Perhaps we should use 'node' or 'device' instead of 'host' ?)
  * Services have Tests. Services describe a set of expected behavior/functionality.
  * Services have Services?
  * Hosts have Tests.
  * Tests are what are exceuted.
  * Test Results are keyed by (host, service, test)

Test results include test output, exit status, duration, and start time.

A host is a network-addressable device. It can be a switch, router, firewall, storage node, plain server, blade, etc.

? Given virtual machines and blades, would it make sense to allow hosts to have
host children?

? A service, such as 'production master database,' has certain behaviors that
need to be verified. Each behavior could be represented as one or more tests.
Since some behavior testing is reusable, such as "mysql" we should allow for
services to have services.

? Where should we define scheduling (how often to run, what times of day/week to run or not run).

## Addons, Plugins, Extensions ##

Calmon should be easy to extend, hook, and abuse.

  * Event handlers (ruby, python, whatever, scripts) should be possible.
  * What other extensions should we plan for?

# API Features #

HTTP(S) + REST? TBD.

# Dashboard Features #

  * Searchable
  * Support for iphone/android/other browsers would be awesome.
  * Other things? TBD.

# Misc #

## Nomenclature for Nagios users ##

| **Nagios term** | **Calmon term** |
|:----------------|:----------------|
| service | test |
| servicegroup | service |
| host  | host |
| plugin | test command |

Nagios calls 'plugins' what I would call any given program.

For example, in Nagios you may define a service that runs a nagios plugin
'check\_tcp' on port 22. In Calmon, we call that a test that runs a command
'check\_tcp' on port 22.

Services are much more than single tests, so it makes sense to make Service a
collection of tests.
