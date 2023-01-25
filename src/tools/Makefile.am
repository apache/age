SUBDIRS = pcp pgmd5 pgenc pgproto watchdog

bin_SCRIPTS =  pgpool_setup

pgpool_setup:
	ln -s ../test/pgpool_setup .

clean-local:
	-rm -f $(bin_SCRIPTS)
