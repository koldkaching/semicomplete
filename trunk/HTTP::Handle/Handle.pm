=pod

=head1 NAME

HTTP::Handle - HTTP Class designed for streaming

=head1 SYNOPSIS

 use HTTP::Handle;

 my $http = HTTP::Handle->new( uri => "http://www.google.com/" );
 $http->connect();
 
 my $fd = $http->fd();
 
 while (<$fd>) {
 	print "--> $_";
 }

=head1 VERSION

Version: 0.2

$Id$

=head1 DESCRIPTION

The C<HTTP::Handle> module allows you to make HTTP requests and handle
the data yourself. The general ideas is that you use this module to make
a HTTP request and handle non-header data yourself. I needed such a 
feature for my mp3 player to listen to icecast streams.

=cut

package HTTP::Handle;

use strict;
use IO::Handle;
use Socket;
use URI;

my $VERSION = "0.2";

=pod

=over 4

=item HTTP::Handle->new()

Create a new HTTP::Handle object thingy.

Arguments possible:

=over 4

=item url => "http://www.google.com/"

Sets the initial URL to connect to.

=item follow_redirects => [ 0 | 1 ]

Automatically follow HTTP redirects. This defaults to true (1). Set to 0 to disable this.

=item http_request => HASHREF

Any thing put in here will be sent as "key: value" in the http request string.

=back

=cut

sub new {
	my $class = shift;

	my $self = {
		data_timeout         => 5,
		follow_redirects     => 1,
		http_request         => {
			                        "User-Agent"   => "HTTP-Handle/$VERSION",
			                     },
	};

	my %args = @_;

	while (my ($key, $val) = each(%args)) {
		$self->{$key} = $val;
	}

	$self->url($self->{"uri"}) if (defined($self->{"uri"}));
	bless $self, $class;

	return $self;
}

=pod 

=item $http->connect()

Connect, send the http request, and process the response headers. 

This function returns -1 on failure, undef otherwise. The reason for failure will be printed to STDERR.


=cut

sub connect($) {
	my $self = shift;

	my $sock;

CONNECT:

	socket($sock, PF_INET, SOCK_STREAM, getprotobyname('tcp')) 
		or _fatal("Failed creating socket.") and return -1;

	$self->{"socket"} = $sock;

	_debug("Looking up hostname for " . $self->{"host"});
	my $inetnum = inet_aton($self->{"host"});
	_debug("Done!\n");

	if (!defined($inetnum)) {
		_fatal("Unable to resolve hostname: " . $self->{"host"});
		return -1;
	}

	_debug("Connecting to " . $self->{"host"} . ":" . $self->{"port"});

	connect($sock, sockaddr_in($self->{"port"}, $inetnum)) 
		or _fatal("Failed connecting to " . $self->{"host"} . ":" . $self->{"port"}) and return -1;
	_debug("Connected");

	$sock->autoflush(1);
	_debug("Sending HTTP Request");
	print $sock $self->http_request_string();
	_debug($self->http_request_string());

	_debug("Data Sent");
	my $sel = new IO::Select($sock);
	my $count = 0;
	my $data;

	while (1) {
		return -1 if ($count > $self->{"data_timeout"});
		if ($sel->can_read(1)) {
			my $ret = read($sock,$data,1,length($data));
			if (!defined($ret)) {
				_debug("Failed on read: $!");
				_debug("Buffer: $data");
				exit;
			} elsif ($ret == 0) {
				_debug("EOF Hit...");
				_debug("Buffer: $data");
				last;
			}
			while ($data =~ s!(.*)\r\n!!s) {
				chomp(my $f = $1);
				_debug("HEADER- '$f'");
				unless (defined($self->{"code"})) {
					$self->{"code"} = $f;
					next;
				}
				goto DONEHEADERS if ($f =~ m/^$/);
				$f =~ m/(\S+):\s*(.+)\s*/;
				$self->{"http_response"}->{$1} = $2;
			}
			#print STDERR "Waiting on data... $count\n";
		} else {
			$count++;
		}
	}
	DONEHEADERS:


	if ($self->{"follow_redirects"} && ($self->{"code"} =~ m/^\S+\s+302/)) {
		 close($sock);
		 $self->url($self->{"http_response"}->{"Location"});
		 _debug("Redirecting to " . $self->{"http_response"}->{"Location"});
		 goto CONNECT;
	}
}

=pod

=item $http->fd()

Get the file descriptor (socket) we're using to connect.

=cut

sub fd($) {
	my $self = shift;

	return $self->{"socket"};
}

=pod

=item $http->url( [ url_string ])

Get or set the URL. If a url string is passed, you will change the url
that is requested. If no parameter is passed, a L<URI> object will be 
returned containing the 

=cut

sub url($;$) {
	my $self = shift;
	my $url = shift;

	if (defined($url)) {
		my $uri = URI->new($url);

		$self->{"host"} = $uri->host();
		$self->{"http_path"} = $uri->path() || "/";
		$self->{"port"} = $uri->port();
		$self->{"uri"} = $uri;

		$self->{"http_request"}->{"Host"} = $self->{"host"};
		$self->{"http_action"} |= "GET";

	} else {
		return $self->{"url"};
	}
}

=pod

=item $http->follow_redirects( [ 0 | 1 ] )

If a value is passed then you will set whether or not we will 
automatically follow HTTP 302 Redirects. If no value is passed, then
we will return whatever the current option is. 

Defaults to 1 (will follow redirects).

=cut

sub follow_redirects($;$) {
	my $self = shift;
	my $opt = shift;

	if (defined($opt)) {
		$self->{"follow_redirects"} = $opt;
	} else {
		return $self->{"follow_redirects"};
	}
}

=pod

=item $http->http_request_string()

Returns a string containing the HTTP request and headers, this is used
when $http->connect() is called.

=cut

sub http_request_string($) {
	my $self = shift;

	my $ret = sprintf("%s %s HTTP/1.0\r\n", $self->{"http_action"}, $self->{"http_path"}); 
	map { $ret .= sprintf("%s: %s\r\n", $_, $self->{"http_request"}->{$_}) } keys(%{$self->{"http_request"}});

	$ret .= "\r\n\r\n";

	return $ret;
}

sub _fatal {
	print STDERR @_, "\n", "ERR: $!\n";
}

sub _debug {
	#print STDERR map { "$_\n" } @_;
}

=pod

=back

=head1 AUTHOR

Jordan Sissel <psionic@csh.rit.edu>

=cut

1;


