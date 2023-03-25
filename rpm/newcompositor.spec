Name:       newcompositor
Summary:    Nesting Wayland compositor for Sailfish OS
Version:    0.0.0a
Release:    1
License:    BSD
URL:        https://github.com/ArturGaspar/newcompositor
Requires:   opt-qt5-qtbase >= 5.15.8
Requires:   opt-qt5-qtbase-gui >= 5.15.8
Requires:   opt-qt5-qtdeclarative >= 5.15.8
Requires:   opt-qt5-qtquickcontrols2 >= 5.15.8
Requires:   opt-qt5-qtwayland >= 5.15.8
Requires:   opt-qt5-sfos-maliit-platforminputcontext
BuildRequires:  opt-qt5-qtbase-devel >= 5.15.8
BuildRequires:  opt-qt5-qtbase-private-devel >= 5.15.8
BuildRequires:  opt-qt5-qtbase-static >= 5.15.8
BuildRequires:  opt-qt5-qtdeclarative-devel >= 5.15.8
BuildRequires:  opt-qt5-qtquickcontrols2-devel >= 5.15.8
BuildRequires:  opt-qt5-qtwayland-devel >= 5.15.8
BuildRequires:  pkgconfig(xkbcommon)
BuildRequires:  pkgconfig(wayland-server)

%description
Nesting Wayland compositor for Sailfish OS that attempts to transparently
handle xdg_shell clients.

%prep
%autosetup -n %{name}-%{version}

%build
sed -i -e 's|@@LIB@@|%{_lib}|g' newcompositor.sh
%{opt_qmake_qt5}
%make_build

%install
rm -rf %{buildroot}
%qmake5_install
mv %{buildroot}/opt/newcompositor/lib %{buildroot}/opt/newcompositor/%{_lib}

%files
%defattr(-,root,root,-)
/opt/%{name}/bin/%{name}
/opt/%{name}/bin/%{name}.bin
/opt/%{name}/%{_lib}/libnewcompositorhacks.so
