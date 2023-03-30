Name:       newcompositor
Summary:    Nesting Wayland compositor for Sailfish OS
Version:    0.0.1
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
sed -e 's|@@LIB@@|%{_libdir}|g' newcompositor.sh.in > newcompositor
%{opt_qmake_qt5}
%make_build

%install
rm -rf %{buildroot}
%qmake5_install
mkdir -p %{buildroot}/%{_libdir}
mv %{buildroot}/usr/lib/newcompositor %{buildroot}/%{_libdir}/newcompositor

%files
%{_bindir}/%{name}
%{_bindir}/%{name}.bin
%{_libdir}/%{name}/libnewcompositorhacks.so
