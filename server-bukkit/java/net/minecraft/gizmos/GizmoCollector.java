// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.gizmos;
public interface GizmoCollector {
    GizmoProperties IGNORED = new GizmoProperties() {
        public GizmoProperties setAlwaysOnTop() {
            return this;
        }
        public GizmoProperties persistForMillis(int milliseconds) {
            return this;
        }
        public GizmoProperties fadeOut() {
            return this;
        }
    };
    GizmoCollector NOOP = gizmo -> IGNORED;
    GizmoProperties add(Gizmo gizmo);
}
