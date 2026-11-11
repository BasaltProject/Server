// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.gizmos;
import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import net.minecraft.util.Mth;
import net.minecraft.util.Util;
public class SimpleGizmoCollector implements GizmoCollector {
    private final List<GizmoInstance> persistent = new ArrayList<>();
    private final List<GizmoInstance> transients = new ArrayList<>();
    public GizmoProperties add(Gizmo gizmo) {
        GizmoInstance entry = new GizmoInstance(gizmo);
        persistent.add(entry);
        return entry;
    }
    public List<GizmoInstance> drainGizmos() {
        List<GizmoInstance> out = new ArrayList<>(persistent);
        out.addAll(transients);
        long now = Util.getMillis();
        persistent.removeIf(g -> g.getExpireTimeMillis() < now);
        transients.clear();
        return out;
    }
    public List<GizmoInstance> getGizmos() {
        return persistent;
    }
    public void addTemporaryGizmos(Collection<GizmoInstance> gizmos) {
        transients.addAll(gizmos);
    }
    public static class GizmoInstance implements GizmoProperties {
        private final Gizmo shape;
        private boolean onTop;
        private boolean fading;
        private long started;
        private long expires;
        private GizmoInstance(Gizmo gizmo) {
            this.shape = gizmo;
        }
        public GizmoProperties setAlwaysOnTop() {
            onTop = true;
            return this;
        }
        public GizmoProperties persistForMillis(int milliseconds) {
            started = Util.getMillis();
            expires = started + milliseconds;
            return this;
        }
        public GizmoProperties fadeOut() {
            fading = true;
            return this;
        }
        public float getAlphaMultiplier(long currentMillis) {
            if (!fading) {
                return 1.0F;
            }
            return 1.0F - Mth.clamp((float) (currentMillis - started) / (float) (expires - started), 0.0F, 1.0F);
        }
        public boolean isAlwaysOnTop() {
            return onTop;
        }
        public long getExpireTimeMillis() {
            return expires;
        }
        public Gizmo gizmo() {
            return shape;
        }
    }
}
