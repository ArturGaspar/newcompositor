Name:       newcompositor
Summary:    Nesting Wayland compositor for Sailfish OS
Version:    0.0.5
Release:    1
License:    BSD
URL:        https://github.com/ArturGaspar/newcompositor
Source0:    %{name}-%{version}.tar.bz2
Requires:   opt-qt5-qtbase >= 5.15.8
Requires:   opt-qt5-qtbase-gui >= 5.15.8
Requires:   opt-qt5-qtdeclarative >= 5.15.8
Requires:   opt-qt5-qtquickcontrols2 >= 5.15.8
Requires:   opt-qt5-qtwayland >= 5.15.8
BuildRequires:  opt-qt5-rpm-macros
BuildRequires:  opt-qt5-qtbase-devel >= 5.15.8
BuildRequires:  opt-qt5-qtdeclarative-devel >= 5.15.8
BuildRequires:  opt-qt5-qtquickcontrols2-devel >= 5.15.8
BuildRequires:  opt-qt5-qtwayland-devel >= 5.15.8
BuildRequires:  pkgconfig(xcb)
BuildRequires:  pkgconfig(xcb-composite)
BuildRequires:  pkgconfig(xkbcommon)
BuildRequires:  pkgconfig(wayland-server)
%{?opt_qt5_default_filter}

%description
Nesting Wayland compositor for Sailfish OS that attempts to transparently
handle xdg_shell clients.

%package qt-runner-compat
Summary:    Compatibility wrapper to use newcompositor instead of qt-runner
Conflicts:  qt-runner
Provides:   qt-runner
Requires:   %{name} = %{version}

%description qt-runner-compat
%{summary}.

%prep
%autosetup -n %{name}-%{version}

%build
sed -e 's|@@LIB@@|%{_libdir}|g' %{name}.sh.in > %{name}
%{opt_qmake_qt5} CONFIG+=qt-runner-compat CONFIG+=xwayland
%make_build

%install
rm -rf %{buildroot}
%make_install
%if "%{_bindir}" != "/usr/bin"
mkdir -p %{buildroot}/%{_bindir}
mv %{buildroot}/usr/bin/* %{buildroot}/%{_bindir}
%endif
%if "%{_libdir}" != "/usr/lib"
mkdir -p %{buildroot}/%{_libdir}
mv %{buildroot}/usr/lib/%{name} %{buildroot}/%{_libdir}/%{name}
%endif
%if "%{_userunitdir}" != "/usr/lib/systemd/user"
mv %{buildroot}/usr/lib/systemd/user/%{name}.service %{buildroot}/%{_userunitdir}/%{name}.service
%endif

%post
%systemd_user_post %{name}.service

%preun
%systemd_user_preun %{name}.service

%postun
%systemd_user_postun %{name}.service

%files
%{_bindir}/%{name}
%{_bindir}/%{name}.bin
%{_libdir}/%{name}/libnewcompositorhacks.so
%{_userunitdir}/%{name}.service

%files qt-runner-compat
%{_bindir}/qt-runner
