#!/usr/bin/env perl

use strict;
use bigint;

my $accum;
my $seen = 0;

while(<>) {
    chomp;
    /^\s*([0-9a-f]+)/ || next;
    my $n = $1;
    my $curr = 0 + "0x$n";

    if (!defined $accum) {
        $accum = $curr;
        $seen++;
        next;
    }

    $accum += $curr;
}

die "no records seen" if !$seen;

$accum %= 2**256;

my $o = $accum->as_hex();
$o =~ s/^0x//;
$o = '0'x(64 - length($o)) . $o;

print "$o\n";
