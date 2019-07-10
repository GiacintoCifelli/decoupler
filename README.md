README
==================================================

decoupler (ka) is a filter to be placed between a client/server connection.

It will keep the client alive all the time (unless it dies by itself, in which case it will accept a reconnection, too).

It is designed for connections when the server is likely to die, for example when using a mobile connection, and the new IP address will not be the same as the previous one (ie: not handled quickly with an IP table manipulation).

decoupler will attempt to reconnect to the server, and until this is not possible will ***drop the packets*** from the client, without buffering.


configuration
============================================

decoupler offers a series or properties to configure the log, in particular to run well under systemd.

On top of this, the disconnection/reconnection work better if the socket is closed quickly.
Check:
	cat /proc/sys/net/ipv4/tcp_fin_timeout
	// default 60 seconds
change to 1 second:
	sudo su
	echo "1" > /proc/sys/net/ipv4/tcp_fin_timeout
