// The default implementation of extended_surface_onscreen_visibility under
// Sailfish OS hides the window forever. This can be loaded with LD_PRELOAD
// to override it.

#include <cstdint>

namespace QtWaylandClient {

class QWaylandExtendedSurface
{
private:
    void extended_surface_onscreen_visibility(int32_t visibility);
};

void QWaylandExtendedSurface::extended_surface_onscreen_visibility(int32_t /*visibility*/) {
    // TODO: forward visibility to original method unless it's the one
    // that hides the window.
}

}
