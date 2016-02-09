Summary:	Ebucket - Elliptics bucket processing library
Name:		ebucket
Version:	2.26.0.0.1
Release:	1%{?dist}

License:	Apache 2.0
Group: 		Development/Libraries
URL:		https://github.com/bioothod/ebucket
Source0:	%{name}-%{version}.tar.bz2
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires:	boost-devel, boost-system, boost-thread, boost-program-options
BuildRequires:	elliptics-client-devel >= 2.26.10.1
BuildRequires:  cmake, msgpack-devel, python-virtualenv

%global debug_package %{nil}

%description
Ebucket states for Elliptics Bucket - an object which contains metadata about how replicas should be arranged in the storage.
This library helps operating with buckets.
http://doc.reverbrain.com/backrunner:backrunner#bucket

%package devel
Summary:	Ebucket - Elliptics bucket processing library
Provides:	ebucket-static = %{version}-%{release}

%description devel
Ebucket states for Elliptics Bucket - an object which contains metadata about how replicas should be arranged in the storage.
This library helps operating with buckets.
http://doc.reverbrain.com/backrunner:backrunner#bucket

%prep
%setup -q

%build
%{cmake} .

make %{?_smp_mflags}
#make test

%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}

%post -p /sbin/ldconfig
%postun -p /sbin/ldconfig


%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root,-)
%{_includedir}/*
#%doc README.md
#%{_bindir}/*
#%{_libdir}/*.so.*

%changelog
* Fri Feb 05 2016 Evgeniy Polyakov <zbr@ioremap.net> - 2.26.0.0.1
- Initial commit

