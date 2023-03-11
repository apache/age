%global pgmajorversion 12
%global pginstdir /usr/pgsql-%{pgmajorversion}

Summary:        Postgres AGE
Name:           postgres-%{pgmajorversion}-age
Version:        1
Release:        %{dist}
License:        Apache
Group:          Databases
Url:            https://github.com/apache/age.git
Source0:        %{name}-%{version}.tar.gz
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

%description
Apache AGE is an extension for PostgreSQL that enables users to leverage a graph database on top of the existing relational databases.

BuildRequires:  gcc glibc glib-common readline readline-devel zlib zlib-devel flex bison  
BuildRequires: centos-release-scl epel-release postgresql%{pgmajorversion} postgresql%{pgmajorversion}-devel

%prep
%autosetup

%build
PATH="%{pginstdir}/bin:$PATH"
export PATH
make install

%install
cp -p age.so %{pginstdir}/lib/age.so
cp -p age--1.1.1.sql %{pginstdir}/share/extension/age--1.1.1.sql
cp -p age.control %{pginstdir}/share/extension/age.control

%files

%changelog
* Fri Feb 17 2023 - Khurram Latif <Khurram.L@agedb.io>
- Initial packaging for AGE
