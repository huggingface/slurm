#!/usr/bin/perl
use IO::Socket;

$pid = fork();
if ($pid)
{
#stdout
	my $sock1 = new IO::Socket::INET (
			LocalHost => 'localhost',
			LocalPort => '7071',
			Proto => 'tcp',
			Listen => 1,
			Reuse => 1,
			);
	my $new_sock1 = $sock1->accept();
	while(defined(<$new_sock1>)) 
	{
		print $_;
	}
	close($sock1);
}
else
{
#stderr
	my $sock2 = new IO::Socket::INET (
			LocalHost => 'localhost',
			LocalPort => '7072',
			Proto => 'tcp',
			Listen => 1,
			Reuse => 1,
			);
	my $new_sock2 = $sock2->accept();
	while(defined(<$new_sock2>)) 
	{
		print $_;
	}
	close($sock2);
}
