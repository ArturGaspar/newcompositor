Name:       newcompositor
Summary:    Nesting Wayland compositor for Sailfish OS
Version:    0.0.4
Release:    1
License:    BSD
URL:        https://github.com/ArturGaspar/newcompositor
Requires:   opt-qt5-qtbase >= 5.15.8
Requires:   opt-qt5-qtbase-gui >= 5.15.8
Requires:   opt-qt5-qtdeclarative >= 5.15.8
Requires:   opt-qt5-qtquickcontrols2 >= 5.15.8
Requires:   opt-qt5-qtwayland >= 5.15.8
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

%prep
%autosetup -n %{name}-%{version}

%build
sed -e 's|@@LIB@@|%{_libdir}|g' %{name}.sh.in > %{name}
%{opt_qmake_qt5} 'CONFIG += xwayland'
%make_build

%install
rm -rf %{buildroot}
%qmake5_install
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
