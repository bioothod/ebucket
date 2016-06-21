Summary:	Ebucket - Elliptics bucket processing library
Name:		ebucket
Version:	0.3.0
Release:	1%{?dist}

License:	Apache 2.0
Group: 		Development/Libraries
URL:		https://github.com/bioothod/ebucket
Source0:	%{name}-%{version}.tar.bz2
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

BuildRequires:	boost-devel, boost-system, boost-thread, boost-program-options, boost-filesystem
BuildRequires:	elliptics-client-devel >= 2.26.10.1
BuildRequires:  cmake, msgpack-devel, python-virtualenv, libswarm3-devel, libthevoid3-devel

%global debug_package %{nil}

%description
Ebucket states for Elliptics Bucket - an object which contains metadata about how replicas should be arranged in the storage.
This library helps operating with buckets.
http://doc.reverbrain.com/backrunner:backrunner#bucket

ebucket package contains HTTP server which selects bucket the most suitable for requested size.

%package devel
Summary:	Ebucket - Elliptics bucket processing library
Provides:	ebucket-static = %{version}-%{release}

%description devel
Ebucket states for Elliptics Bucket - an object which contains metadata about how replicas should be arranged in the storage.
This library helps operating with buckets.
http://doc.reverbrain.com/backrunner:backrunner#bucket

ebucket-devel package contains headers needed to embed bucket processing into your code.

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
%{_bindir}/*
%doc conf/

%files devel
%defattr(-,root,root,-)
%{_includedir}/*
#%doc README.md
#%{_bindir}/*
#%{_libdir}/*.so.*

%changelog
* Tue Jun 21 2016 Evgeniy Polyakov <zbr@ioremap.net> - 0.3.0
- bucket_processor: return error if there are no buckets suitable for given size
- Added HTTP server which searches for bucket given requested size and returns JSON-serialized reply
- bucket: added metadata to_string() method
- debian: added build dependencies as dependencies for header-only development package

* Tue May 24 2016 Evgeniy Polyakov <zbr@ioremap.net> - 0.2.1
- bucket: destructor must wait until all pending operations are completed
- bucket_processor: moved test() around, export error session
- Added global license file
- Updated debian/copyright
- cmake: fixed elliptics version lookup
- Create README.md
- Added example of bucket processor initialization by single key

* Sat Feb 27 2016 Evgeniy Polyakov <zbr@ioremap.net> - 0.2.0
- test: added automatic bucket and bucket list generation and initialization test which checks bucket key
- bucket_processor: added initalization via bucket key
- weight: per-bucket weight is decreased if stats are incomplete, it is further decreased in @get_bucket() if some groups are absent from current route table

* Wed Feb 10 2016 Evgeniy Polyakov <zbr@ioremap.net> - 0.1.0
- Added test and example code
- Changed bucket API to setup session only, no need to use duplicated methods from elliptics

* Fri Feb 05 2016 Evgeniy Polyakov <zbr@ioremap.net> - 2.26.0.0.1
- Initial commit

