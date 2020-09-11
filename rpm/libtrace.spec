Name:       libtrace
Summary:    A simple tracing library with keyword filtering support
Version:    1.1.8
Release:    0
Group:      System/Libraries
License:    LGPLv2
URL:        https://git.sailfishos.org/mer-core/libtrace-ohm
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

%package doc
Summary:   Documentation for %{name}
Group:     Documentation
Requires:  %{name} = %{version}-%{release}

%description doc
Man pages for %{name}.

%prep
%setup -q -n %{name}-%{version}

%build

%autogen --disable-static
%configure --disable-static
make %{?_smp_mflags}

%install
rm -rf %{buildroot}

%make_install
rm -f %{buildroot}/%{_libdir}/*.la

mkdir -p %{buildroot}%{_docdir}/%{name}-%{version}
install -m0644 -t %{buildroot}%{_docdir}/%{name}-%{version} \
        AUTHORS ChangeLog NEWS README
mv %{buildroot}%{_docdir}/libsimple-trace-doc/libsimple-trace.html \
   %{buildroot}%{_docdir}/%{name}-%{version}/

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-,root,root,-)
%{_libdir}/libsimple-trace*.so.*
%license COPYING

%files devel
%defattr(-,root,root,-)
%{_includedir}/simple-trace
%{_libdir}/libsimple-trace*.so
%{_libdir}/pkgconfig/libsimple-trace*

%files doc
%defattr(-,root,root,-)
%{_mandir}/man3/libsimple-trace.*
%{_docdir}/%{name}-%{version}
