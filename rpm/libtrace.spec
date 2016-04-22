Name:       libtrace
Summary:    A simple tracing library with keyword filtering support
Version:    1.1.8
Release:    0
Group:      System/Libraries
License:    LGPLv2.1
URL:        http://meego.gitorious.org/maemo-multimedia/trace
Source0:    %{name}-%{version}.tar.gz
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig

%description
A simple tracing library with keyword filtering support.

%package devel
Summary:    Development files for %{name}
Group:      Development/Libraries
Requires:   %{name} = %{version}-%{release}

%description devel
Development files for %{name}.

%prep
%setup -q -n %{name}-%{version}

%build

%autogen --disable-static
%configure --disable-static
make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}

%make_install

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-,root,root,-)
%{_libdir}/libsimple-trace*.so.*
%doc COPYING AUTHORS INSTALL README NEWS ChangeLog

%files devel
%defattr(-,root,root,-)
%{_includedir}/simple-trace
%{_libdir}/libsimple-trace*.so
%{_libdir}/pkgconfig/libsimple-trace*
%{_mandir}/man3/*
%doc %{_docdir}/libsimple-trace-doc/*
