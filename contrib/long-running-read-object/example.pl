#!/usr/bin/perl
#
# Example implementation for the Git read-object protocol version 1
# See Documentation/technical/read-object-protocol.txt
#
# Allows you to test the ability for blobs to be pulled from a host git repo
# "on demand."  Called when git needs a blob it couldn't find locally due to
# a lazy clone that only cloned the commits and trees.
#
# A lazy clone can be simulated via the following commands from the host repo
# you wish to create a lazy clone of:
#
# cd /host_repo
# git rev-parse HEAD
# git init /guest_repo
# git cat-file --batch-check --batch-all-objects | grep -v 'blob' |
#	cut -d' ' -f1 | git pack-objects /guest_repo/.git/objects/pack/noblobs
# cd /guest_repo
# git config core.virtualizeobjects true
# git reset --hard <sha from rev-parse call above>
#
# Please note, this sample is a minimal skeleton. No proper error handling
# was implemented.
#

use strict;
use warnings;

#
# Point $DIR to the folder where your host git repo is located so we can pull
# missing objects from it
#
my $DIR = "/host_repo/.git/";

sub packet_bin_read {
	my $buffer;
	my $bytes_read = read STDIN, $buffer, 4;
	if ( $bytes_read == 0 ) {

		# EOF - Git stopped talking to us!
		exit();
	}
	elsif ( $bytes_read != 4 ) {
		die "invalid packet: '$buffer'";
	}
	my $pkt_size = hex($buffer);
	if ( $pkt_size == 0 ) {
		return ( 1, "" );
	}
	elsif ( $pkt_size > 4 ) {
		my $content_size = $pkt_size - 4;
		$bytes_read = read STDIN, $buffer, $content_size;
		if ( $bytes_read != $content_size ) {
			die "invalid packet ($content_size bytes expected; $bytes_read bytes read)";
		}
		return ( 0, $buffer );
	}
	else {
		die "invalid packet size: $pkt_size";
	}
}

sub packet_txt_read {
	my ( $res, $buf ) = packet_bin_read();
	unless ( $buf =~ s/\n$// ) {
		die "A non-binary line MUST be terminated by an LF.";
	}
	return ( $res, $buf );
}

sub packet_bin_write {
	my $buf = shift;
	print STDOUT sprintf( "%04x", length($buf) + 4 );
	print STDOUT $buf;
	STDOUT->flush();
}

sub packet_txt_write {
	packet_bin_write( $_[0] . "\n" );
}

sub packet_flush {
	print STDOUT sprintf( "%04x", 0 );
	STDOUT->flush();
}

( packet_txt_read() eq ( 0, "git-read-object-client" ) ) || die "bad initialize";
( packet_txt_read() eq ( 0, "version=1" ) )				 || die "bad version";
( packet_bin_read() eq ( 1, "" ) )                       || die "bad version end";

packet_txt_write("git-read-object-server");
packet_txt_write("version=1");
packet_flush();

( packet_txt_read() eq ( 0, "capability=get" ) )    || die "bad capability";
( packet_bin_read() eq ( 1, "" ) )                  || die "bad capability end";

packet_txt_write("capability=get");
packet_flush();

while (1) {
	my ($command) = packet_txt_read() =~ /^command=([^=]+)$/;

	if ( $command eq "get" ) {
		my ($sha1) = packet_txt_read() =~ /^sha1=([0-9a-f]{40})$/;
		packet_bin_read();

		system ('git --git-dir="' . $DIR . '" cat-file blob ' . $sha1 . ' | git -c core.virtualizeobjects=false hash-object -w --stdin >/dev/null 2>&1');
		packet_txt_write(($?) ? "status=error" : "status=success");
		packet_flush();
	} else {
		die "bad command '$command'";
	}
}
