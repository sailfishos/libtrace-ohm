Name:       libtrace
Summary:    A simple tracing library with keyword filtering support
Version:    1.1.9
Release:    0
License:    LGPLv2
URL:        https://github.com/sailfishos/libtrace-ohm
Source0:    %{name}-%{version}.tar.gz
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig
BuildRequires: automake
BuildRequires: libtool

%description
A simple tracing library with keyword filtering support.

%package devel
Summary:    Development files for %{name}
Requires:   %{name} = %{version}-%{release}

%description devel
Development files for %{name}.

%package doc
Summary:   Documentation for %{name}
Requires:  %{name} = %{version}-%{release}

%description doc
Man pages for %{name}.

%prep
%setup -q -n %{name}-%{version}

%build

%autogen --disable-static
%configure --disable-static
%make_build

%install
%make_install

mkdir -p %{buildroot}%{_docdir}/%{name}-%{version}
install -m0644 -t %{buildroot}%{_docdir}/%{name}-%{version} \
        AUTHORS ChangeLog NEWS README
mv %{buildroot}%{_docdir}/libsimple-trace-doc/libsimple-trace.html \
   %{buildroot}%{_docdir}/%{name}-%{version}/

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%{_libdir}/libsimple-trace*.so.*
%license COPYING

%files devel
%{_includedir}/simple-trace
%{_libdir}/libsimple-trace*.so
%{_libdir}/pkgconfig/libsimple-trace*

%files doc
%{_mandir}/man3/libsimple-trace.*
%{_docdir}/%{name}-%{version}
