# How to build RPM:
#
#   rpmbuild -ba pgpool.spec --define="pgpool_version 3.4.0" --define="pg_version 93" --define="pghome /usr/pgsql-9.3" --define="dist .rhel7" --define="pgsql_ver 93"
#
# OR
#
#   rpmbuild -ba pgpool.spec --define="pgpool_version 3.4.0" --define="pg_version 11" --define="pghome /usr/pgsql-11" --define="dist .rhel7" --define="pgsql_ver 110"
#
# expecting RPM name are:
#   pgpool-II-pg{pg_version}-{pgpool_version}-{rel}pgdg.rhel{v}.{arch}.rpm
#   pgpool-II-pg{pg_version}-devel-{pgpool_version}-{rel}pgdg.rhel{v}.{arch}.rpm
#   pgpool-II-pg{pg_version}-extensions-{pgpool_version}-{rel}pgdg.rhel{v}.{arch}.rpm
#   pgpool-II-pg{pg_version}-{pgpool_version}-{rel}pgdg.rhel{v}.src.rpm

%global short_name  pgpool-II

%if 0%{rhel} && 0%{rhel} <= 6
  %global systemd_enabled 0
%else
  %global systemd_enabled 1
%endif

%global _varrundir %{_localstatedir}/run/pgpool
%global _varlogdir %{_localstatedir}/log/pgpool_log

Summary:        Pgpool is a connection pooling/replication server for PostgreSQL
Name:           pgpool-II-pg%{pg_version}
Version:        %{pgpool_version}
Release:        1pgdg%{?dist}
License:        BSD
Group:          Applications/Databases
Vendor:         Pgpool Global Development Group
URL:            http://www.pgpool.net/
Source0:        pgpool-II-%{version}.tar.gz
Source1:        pgpool.init
Source2:        pgpool_rhel6.sysconfig
%if %{systemd_enabled}
Source3:        pgpool.service
%endif
Source4:        pgpool_rhel.sysconfig
Source5:        pgpool_tmpfiles.d
Source6:        pgpool_sudoers.d
Patch1:         pgpool-II-head.patch
%if %{pgsql_ver} >=94 && %{rhel} >= 7
Patch2:         pgpool_socket_dir.patch
Patch3:         pcp_unix_domain_path.patch
%endif
Patch4:         pgpool_log.patch
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildRequires:  postgresql%{pg_version}-devel pam-devel openssl-devel jade libxslt docbook-dtds docbook-style-xsl docbook-style-dsssl openldap-devel
%if %{rhel} >= 9
BuildRequires:  libmemcached-awesome-devel
%else
BuildRequires:  libmemcached-devel
%endif
%if %{pgsql_ver} >= 110 && %{rhel} == 7
BuildRequires:  llvm-toolset-7 llvm-toolset-7-llvm-devel llvm5.0
%endif
%if %{pgsql_ver} >= 110 && %{rhel} >= 8
BuildRequires:  llvm-devel >= 6.0.0 clang-devel >= 6.0.0
%endif
%if %{systemd_enabled}
BuildRequires:    systemd
Requires:         systemd
Requires(post):   systemd-sysv
Requires(post):   systemd
Requires(preun):  systemd
Requires(postun): systemd
%else
Requires(post):   chkconfig
Requires(preun):  chkconfig
Requires(preun):  initscripts
Requires(postun): initscripts
%endif
Obsoletes:      postgresql-pgpool

# original pgpool archive name
%define archive_name pgpool-II-%{version}

%description
pgpool-II is a inherited project of pgpool (to classify from
pgpool-II, it is sometimes called as pgpool-I). For those of
you not familiar with pgpool-I, it is a multi-functional
middle ware for PostgreSQL that features connection pooling,
replication and load balancing functions. pgpool-I allows a
user to connect at most two PostgreSQL servers for higher
availability or for higher search performance compared to a
single PostgreSQL server.

%package devel
Summary:     The development files for pgpool-II
Group:       Development/Libraries
Requires:    %{name} = %{version}

%description devel
Development headers and libraries for pgpool-II.

%package extensions
Summary:     PostgreSQL extensions for pgpool-II
Group:       Applications/Databases
%description extensions
PostgreSQL extensions libraries and sql files for pgpool-II.

%prep
%setup -q -n %{archive_name}
%patch1 -p1
%if %{pgsql_ver} >=94 && %{rhel} >= 7
%patch2 -p0
%patch3 -p0
%endif
%patch4 -p0

%build
%configure --with-pgsql=%{pghome} \
           --disable-static \
           --with-pam \
           --with-openssl \
           --with-ldap \
           --with-memcached=%{_usr} \
           --disable-rpath \
           --sysconfdir=%{_sysconfdir}/%{short_name}/

make %{?_smp_mflags}
make %{?_smp_mflags} -C doc

%install
rm -rf %{buildroot}

# make pgpool-II
export PATH=%{pghome}/bin:$PATH
make %{?_smp_mflags} DESTDIR=%{buildroot} install

# install to PostgreSQL
make %{?_smp_mflags} DESTDIR=%{buildroot} install -C src/sql/pgpool-recovery
%if %{pgsql_ver} <= 93
# From PostgreSQL 9.4 pgpool-regclass.so is not needed anymore
# because 9.4 or later has to_regclass.
make %{?_smp_mflags} DESTDIR=%{buildroot} install -C src/sql/pgpool-regclass
%endif
make %{?_smp_mflags} DESTDIR=%{buildroot} install -C src/sql/pgpool_adm

install -d %{buildroot}%{_datadir}/%{short_name}
install -d %{buildroot}%{_sysconfdir}/%{short_name}
install -d %{buildroot}%{_sysconfdir}/%{short_name}/sample_scripts
mv %{buildroot}%{_sysconfdir}/%{short_name}/failover.sh.sample \
        %{buildroot}%{_sysconfdir}/%{short_name}/sample_scripts/failover.sh.sample
mv %{buildroot}%{_sysconfdir}/%{short_name}/follow_primary.sh.sample \
        %{buildroot}%{_sysconfdir}/%{short_name}/sample_scripts/follow_primary.sh.sample
mv %{buildroot}%{_sysconfdir}/%{short_name}/pgpool_remote_start.sample \
        %{buildroot}%{_sysconfdir}/%{short_name}/sample_scripts/pgpool_remote_start.sample
mv %{buildroot}%{_sysconfdir}/%{short_name}/recovery_1st_stage.sample \
        %{buildroot}%{_sysconfdir}/%{short_name}/sample_scripts/recovery_1st_stage.sample
mv %{buildroot}%{_sysconfdir}/%{short_name}/replication_mode_recovery_1st_stage.sample \
        %{buildroot}%{_sysconfdir}/%{short_name}/sample_scripts/replication_mode_recovery_1st_stage.sample
mv %{buildroot}%{_sysconfdir}/%{short_name}/replication_mode_recovery_2nd_stage.sample \
        %{buildroot}%{_sysconfdir}/%{short_name}/sample_scripts/replication_mode_recovery_2nd_stage.sample
mv %{buildroot}%{_sysconfdir}/%{short_name}/escalation.sh.sample \
        %{buildroot}%{_sysconfdir}/%{short_name}/sample_scripts/escalation.sh.sample
mv %{buildroot}%{_sysconfdir}/%{short_name}/aws_eip_if_cmd.sh.sample \
        %{buildroot}%{_sysconfdir}/%{short_name}/sample_scripts/aws_eip_if_cmd.sh.sample
mv %{buildroot}%{_sysconfdir}/%{short_name}/aws_rtb_if_cmd.sh.sample \
        %{buildroot}%{_sysconfdir}/%{short_name}/sample_scripts/aws_rtb_if_cmd.sh.sample
cp %{buildroot}%{_sysconfdir}/%{short_name}/pcp.conf.sample %{buildroot}%{_sysconfdir}/%{short_name}/pcp.conf
cp %{buildroot}%{_sysconfdir}/%{short_name}/pgpool.conf.sample %{buildroot}%{_sysconfdir}/%{short_name}/pgpool.conf
cp %{buildroot}%{_sysconfdir}/%{short_name}/pool_hba.conf.sample %{buildroot}%{_sysconfdir}/%{short_name}/pool_hba.conf
touch %{buildroot}%{_sysconfdir}/%{short_name}/pool_passwd
touch %{buildroot}%{_sysconfdir}/%{short_name}/pgpool_node_id

%if %{systemd_enabled}
install -d %{buildroot}%{_unitdir}
install -m 644 %{SOURCE3} %{buildroot}%{_unitdir}/pgpool.service

install -d -m 755 %{buildroot}%{_varrundir}
mkdir -p %{buildroot}%{_tmpfilesdir}
install -m 0644 %{SOURCE5} %{buildroot}%{_tmpfilesdir}/%{name}.conf
%else
install -d %{buildroot}%{_initrddir}
install -m 755 %{SOURCE1} %{buildroot}%{_initrddir}/pgpool
%endif

mkdir -p %{buildroot}%{_varlogdir} 

install -d %{buildroot}%{_sysconfdir}/sysconfig
%if 0%{rhel} && 0%{rhel} <= 6
    install -m 644 %{SOURCE2} %{buildroot}%{_sysconfdir}/sysconfig/pgpool
%else
    install -m 644 %{SOURCE4} %{buildroot}%{_sysconfdir}/sysconfig/pgpool
%endif

# install sudoers.d to allow postgres user to run ip and arping with root privileges without a password
install -d %{buildroot}%{_sysconfdir}/sudoers.d
install -m 0644 %{SOURCE6} %{buildroot}%{_sysconfdir}/sudoers.d/pgpool

# nuke libtool archive and static lib
rm -f %{buildroot}%{_libdir}/libpcp.{a,la}

mkdir html
mv doc/src/sgml/html html/en
mv doc.ja/src/sgml/html html/ja

install -d %{buildroot}%{_mandir}/man1
install doc/src/sgml/man1/*.1 %{buildroot}%{_mandir}/man1
install -d %{buildroot}%{_mandir}/man8
install doc/src/sgml/man8/*.8 %{buildroot}%{_mandir}/man8

%clean
rm -rf %{buildroot}

%pre
groupadd -g 26 -o -r postgres >/dev/null 2>&1 || :
useradd -M -g postgres -o -r -d /var/lib/pgsql -s /bin/bash \
        -c "PostgreSQL Server" -u 26 postgres >/dev/null 2>&1 || :

%post
/sbin/ldconfig

%if %{systemd_enabled}
%systemd_post pgpool.service
%else
/sbin/chkconfig --add pgpool
%endif

%preun
%if %{systemd_enabled}
%systemd_preun pgpool.service
%else
if [ $1 = 0 ] ; then
  /sbin/service pgpool condstop >/dev/null 2>&1
  chkconfig --del pgpool
fi
%endif

%postun
/sbin/ldconfig

%if %{systemd_enabled}
%systemd_postun_with_restart pgpool.service

%triggerun -- pgpool < 3.1-1
# Save the current service runlevel info
# User must manually run systemd-sysv-convert --apply pgpool
# to migrate them to systemd targets
/usr/bin/systemd-sysv-convert --save pgpool >/dev/null 2>&1 ||:

# Run these because the SysV package being removed won't do them
/sbin/chkconfig --del pgpool >/dev/null 2>&1 || :
/bin/systemctl try-restart pgpool.service >/dev/null 2>&1 || :

%else
if [ $1 -ge 1 ] ; then
  /sbin/service pgpool condrestart >/dev/null 2>&1 || :
fi
%endif

%files
%defattr(-,root,root,-)
%dir %{_datadir}/%{short_name}
%doc README TODO COPYING INSTALL AUTHORS ChangeLog html
%{_bindir}/pgpool
%{_bindir}/pcp_attach_node
%{_bindir}/pcp_detach_node
%{_bindir}/pcp_node_count
%{_bindir}/pcp_node_info
%{_bindir}/pcp_pool_status
%{_bindir}/pcp_proc_count
%{_bindir}/pcp_proc_info
%{_bindir}/pcp_promote_node
%{_bindir}/pcp_stop_pgpool
%{_bindir}/pcp_recovery_node
%{_bindir}/pcp_watchdog_info
%{_bindir}/pcp_reload_config
%{_bindir}/pcp_health_check_stats
%{_bindir}/pg_md5
%{_bindir}/pg_enc
%{_bindir}/pgpool_setup
%{_bindir}/watchdog_setup
%{_bindir}/pgproto
%{_bindir}/wd_cli
%{_mandir}/man8/*.8.gz
%{_mandir}/man1/*.1.gz
%{_datadir}/%{short_name}/insert_lock.sql
%{_datadir}/%{short_name}/pgpool.pam
%{_libdir}/libpcp.so.*
%if %{systemd_enabled}
%attr(755,postgres,postgres) %dir %{_varrundir}
%{_tmpfilesdir}/%{name}.conf
%{_sysconfdir}/sudoers.d/pgpool
%{_unitdir}/pgpool.service
%else
%{_initrddir}/pgpool
%endif
%attr(0755,postgres,postgres) %dir %{_varlogdir}
%defattr(600,postgres,postgres,-)
%{_sysconfdir}/%{short_name}/pgpool.conf.sample
%{_sysconfdir}/%{short_name}/pcp.conf.sample
%{_sysconfdir}/%{short_name}/pool_hba.conf.sample
%defattr(755,postgres,postgres,-)
%{_sysconfdir}/%{short_name}/sample_scripts/failover.sh.sample
%{_sysconfdir}/%{short_name}/sample_scripts/follow_primary.sh.sample
%{_sysconfdir}/%{short_name}/sample_scripts/pgpool_remote_start.sample
%{_sysconfdir}/%{short_name}/sample_scripts/recovery_1st_stage.sample
%{_sysconfdir}/%{short_name}/sample_scripts/replication_mode_recovery_1st_stage.sample
%{_sysconfdir}/%{short_name}/sample_scripts/replication_mode_recovery_2nd_stage.sample
%{_sysconfdir}/%{short_name}/sample_scripts/escalation.sh.sample
%{_sysconfdir}/%{short_name}/sample_scripts/aws_eip_if_cmd.sh.sample
%{_sysconfdir}/%{short_name}/sample_scripts/aws_rtb_if_cmd.sh.sample
%attr(600,postgres,postgres) %config(noreplace) %{_sysconfdir}/%{short_name}/*.conf
%attr(600,postgres,postgres) %config(noreplace) %{_sysconfdir}/%{short_name}/pool_passwd
%attr(600,postgres,postgres) %config(noreplace) %{_sysconfdir}/%{short_name}/pgpool_node_id
%config(noreplace) %{_sysconfdir}/sysconfig/pgpool

%files devel
%defattr(-,root,root,-)
%{_includedir}/libpcp_ext.h
%{_includedir}/pcp.h
%{_includedir}/pool_process_reporting.h
%{_includedir}/pool_type.h
%{_libdir}/libpcp.so

%files extensions
%defattr(-,root,root,-)
%{pghome}/share/extension/pgpool-recovery.sql
%{pghome}/share/extension/pgpool_recovery--1.1.sql
%{pghome}/share/extension/pgpool_recovery--1.2.sql
%{pghome}/share/extension/pgpool_recovery--1.1--1.2.sql
%{pghome}/share/extension/pgpool_recovery--1.3.sql
%{pghome}/share/extension/pgpool_recovery--1.2--1.3.sql
%{pghome}/share/extension/pgpool_recovery--1.4.sql
%{pghome}/share/extension/pgpool_recovery--1.3--1.4.sql
%{pghome}/share/extension/pgpool_recovery.control
%{pghome}/lib/pgpool-recovery.so
%{pghome}/share/extension/pgpool_adm--1.0.sql
%{pghome}/share/extension/pgpool_adm--1.1.sql
%{pghome}/share/extension/pgpool_adm--1.0--1.1.sql
%{pghome}/share/extension/pgpool_adm--1.2.sql
%{pghome}/share/extension/pgpool_adm--1.1--1.2.sql
%{pghome}/share/extension/pgpool_adm--1.3.sql
%{pghome}/share/extension/pgpool_adm--1.2--1.3.sql
%{pghome}/share/extension/pgpool_adm--1.4.sql
%{pghome}/share/extension/pgpool_adm--1.3--1.4.sql
%{pghome}/share/extension/pgpool_adm.control
%{pghome}/lib/pgpool_adm.so
# From PostgreSQL 9.4 pgpool-regclass.so is not needed anymore
# because 9.4 or later has to_regclass.
%if %{pgsql_ver} <= 93
  %{pghome}/share/extension/pgpool_regclass--1.0.sql
  %{pghome}/share/extension/pgpool_regclass.control
  %{pghome}/share/extension/pgpool-regclass.sql
  %{pghome}/lib/pgpool-regclass.so
%endif
# From PostgreSQL 11 the relevant files have to be installed 
# into $pkglibdir/bitcode/
%if %{pgsql_ver} >= 110 && %{rhel} >= 7
  %{pghome}/lib/bitcode/pgpool-recovery.index.bc
  %{pghome}/lib/bitcode/pgpool-recovery/pgpool-recovery.bc
  %{pghome}/lib/bitcode/pgpool_adm.index.bc
  %{pghome}/lib/bitcode/pgpool_adm/pgpool_adm.bc
%endif

%changelog
* Wed Nov 2 2022 Bo Peng <pengbo@sraoss.co.jp> 4.4.0
- Change /lib/tmpfiles.d/ file from /var/run to /run
- Install /etc/sudoers.d/pgpool
- Add scripts aws_eip_if_cmd.sh.sample and aws_rtb_if_cmd.sh.sample

* Thu Sep 10 2020 Bo Peng <pengbo@sraoss.co.jp> 4.2.0
- Update to 4.2

* Mon Jul 27 2020 Bo Peng <pengbo@sraoss.co.jp> 4.1.3
- Rename src/redhat/pgpool_rhel7.sysconfig to src/redhat/pgpool_rhel.sysconfig.

* Thu Oct 10 2019 Bo Peng <pengbo@sraoss.co.jp> 4.1.0
- Update to support PostgreSQL 12

* Thu Sep 5 2019 Bo Peng <pengbo@sraoss.co.jp> 4.1.0
- Add sample scripts

* Wed Sep 19 2018 Bo Peng <pengbo@sraoss.co.jp> 4.0.0
- Update to 4.0

* Tue Oct 17 2017 Bo Peng <pengbo@sraoss.co.jp> 3.7.0
- Update to 4.0

* Tue Nov 22 2016 Bo Peng <pengbo@sraoss.co.jp> 3.6.0
- Update to 3.6.0

* Mon Dec 28 2015 Yugo Nagata <nagata@sraoss.co.jp> 3.5.0
- Add Chinese document

* Mon Aug 24 2015 Yugo Nagata <nagata@sraoss.co.jp> 3.5.0
- Remove system database

* Tue Feb 10 2015 Nozomi Anzai <anzai@sraoss.co.jp> 3.4.1-2
- Fix %tmpfiles_create to not be executed in RHEL/CentOS 6

* Wed Jan 28 2015 Nozomi Anzai <anzai@sraoss.co.jp> 3.4.1
- Fix typo of %{_smp_mflags}
- Change to use systemd if it is available

* Sat Dec 20 2014 Tatsuo Ishii <ishii@sraoss.co.jp> 3.4.0-3
- Fix "error: Installed (but unpackaged) file(s) found"

* Fri Nov 21 2014 Tatsuo Ishii <ishii@sraoss.co.jp> 3.4.0-2
- Re-enable to apply difference from HEAD patch.

* Tue Nov 18 2014 Yugo Nagata <nagata@sraoss.co.jp> 3.4.0-2
- Rename RPM filename to include RHEL version no.

* Tue Nov 11 2014 Tatsuo Ishii <ishii@sraoss.co.jp> 3.4.0-2
- Add memcached support to configure.

* Tue Oct 21 2014 Tatsuo Ishii <ishii@sraoss.co.jp> 3.4beta2
- Adopt to PostgreSQL 9.4

* Thu Sep 25 2014 Tatsuo Ishii <ishii@sraoss.co.jp> 3.3.4-2
- Split pgpool_regclass and pgpool_recovery as a separate extension package.
- Fix wrong OpenSSL build option.

* Fri Sep 5 2014 Yugo Nagata <nagata@sraoss.co.jp> 3.3.4-1
- Update to 3.3.4

* Wed Jul 30 2014 Tatsuo Ishii <ishii@sraoss.co.jp> 3.3.3-4
- Add PATCH2 which is diff between 3.3.3 and 3.3-stable tree head.
- RPM expert said this is the better way.

* Sat May 10 2014 Tatsuo Ishii <ishii@sraoss.co.jp> 3.3.3-3
- Use 3.3-stable tree head

* Sun May 4 2014 Tatsuo Ishii <ishii@sraoss.co.jp> 3.3.3-2
- Fix configure option
- Add openssl support

* Tue Nov 26 2013 Nozomi Anzai <anzai@sraoss.co.jp> 3.3.1-1
- Improved to specify the versions of pgpool-II and PostgreSQL

* Mon May 13 2013 Nozomi Anzai <anzai@sraoss.co.jp> 3.3.0-1
- Update to 3.3.0
- Change to install pgpool-recovery, pgpool-regclass to PostgreSQL

* Tue Nov 3 2009 Devrim Gunduz <devrim@CommandPrompt.com> 2.2.5-3
- Remove init script from all runlevels before uninstall. Per #RH Bugzilla
  532177

* Mon Oct 5 2009 Devrim Gunduz <devrim@CommandPrompt.com> 2.2.5-2
- Add 2 new docs, per Tatsuo.

* Sun Oct 4 2009 Devrim Gunduz <devrim@CommandPrompt.com> 2.2.5-1
- Update to 2.2.5, for various fixes described at
  http://lists.pgfoundry.org/pipermail/pgpool-general/2009-October/002188.html
- Re-apply a fix for Red Hat Bugzilla #442372

* Wed Sep 9 2009 Devrim Gunduz <devrim@CommandPrompt.com> 2.2.4-1
- Update to 2.2.4

* Wed May 6 2009 Devrim Gunduz <devrim@CommandPrompt.com> 2.2.2-1
- Update to 2.2.2

* Sun Mar 1 2009 Devrim Gunduz <devrim@CommandPrompt.com> 2.2-1
- Update to 2.2
- Fix URL
- Own /usr/share/pgpool-II directory.
- Fix pid file path in init script, per    pgcore #81.
- Fix spec file -- we don't use short_name macro in pgcore spec file.
- Create pgpool pid file directory, per pgcore #81.
- Fix stop/start routines, also improve init script a bit.
- Install conf files to a new directory (/etc/pgpool-II), and get rid
  of sample conf files.

* Fri Aug 8 2008 Devrim Gunduz <devrim@CommandPrompt.com> 2.1-1
- Update to 2.1
- Removed temp patch #4.

* Sun Jan 13 2008 Devrim Gunduz <devrim@CommandPrompt.com> 2.0.1-1
- Update to 2.0.1
- Add a temp patch that will disappear in 2.0.2

* Fri Oct 5 2007 Devrim Gunduz <devrim@CommandPrompt.com> 1.2.1-1
- Update to 1.2.1

* Wed Aug 29 2007 Devrim Gunduz <devrim@CommandPrompt.com> 1.2-5
- Chmod sysconfig/pgpool to 644, not 755. Per BZ review.
- Run chkconfig --add pgpool during %%post.

* Thu Aug 16 2007 Devrim Gunduz <devrim@CommandPrompt.com> 1.2-4
- Fixed the directory name where sample conf files and sql files
  are installed.

* Sun Aug 5 2007 Devrim Gunduz <devrim@CommandPrompt.com> 1.2-3
- Added a patch for sample conf file to use Fedora defaults

* Sun Aug 5 2007 Devrim Gunduz <devrim@CommandPrompt.com> 1.2-2
- Added an init script for pgpool
- Added /etc/sysconfig/pgpool

* Wed Aug 1 2007 Devrim Gunduz <devrim@CommandPrompt.com> 1.2-1
- Update to 1.2

* Fri Jun 15 2007 Devrim Gunduz <devrim@CommandPrompt.com> 1.1.1-1
- Update to 1.1.1

* Sat Jun 2 2007 Devrim Gunduz <devrim@CommandPrompt.com> 1.1-1
- Update to 1.1
- added --disable-rpath configure parameter.
- Chowned sample conf files, so that they can work with pgpoolAdmin.

* Sun Apr 22 2007 Devrim Gunduz <devrim@CommandPrompt.com> 1.0.2-4
- Added postgresql-devel as BR, per bugzilla review.
- Added --disable-static flan, per bugzilla review.
- Removed superfluous manual file installs, per bugzilla review.

* Sun Apr 22 2007 Devrim Gunduz <devrim@CommandPrompt.com> 1.0.2-3
- Rebuilt for the correct tarball
- Fixed man8 file ownership, per bugzilla review #229321

* Tue Feb 20 2007 Jarod Wilson <jwilson@redhat.com> 1.0.2-2
- Create proper devel package, drop -libs package
- Nuke rpath
- Don't install libtool archive and static lib
- Clean up %%configure line
- Use proper %%_smp_mflags
- Install config files properly, without .sample on the end
- Preserve timestamps on header files

* Tue Feb 20 2007 Devrim Gunduz <devrim@commandprompt.com> 1.0.2-1
- Update to 1.0.2-1

* Mon Oct 02 2006 Devrim Gunduz <devrim@commandprompt.com> 1.0.1-5
- Rebuilt

* Mon Oct 02 2006 Devrim Gunduz <devrim@commandprompt.com> 1.0.1-4
- Added -libs and RPM
- Fix .so link problem
- Cosmetic changes to spec file

* Wed Sep 27 2006 - Devrim GUNDUZ <devrim@commandprompt.com> 1.0.1-3
- Fix spec, per Yoshiyuki Asaba

* Tue Sep 26 2006 - Devrim GUNDUZ <devrim@commandprompt.com> 1.0.1-2
- Fixed rpmlint errors
- Fixed download url
- Added ldconfig for .so files

* Thu Sep 21 2006 - David Fetter <david@fetter.org> 1.0.1-1
- Initial build pgpool-II 1.0.1 for PgPool Global Development Group
