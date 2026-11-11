// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;
import net.minecraft.util.Mth;
import net.minecraft.core.BlockPos;
import net.minecraft.world.phys.AABB;
import net.minecraft.world.level.material.FluidState;
import net.minecraft.world.entity.player.Player;
import java.util.Set;
import net.minecraft.core.Holder;
import net.minecraft.tags.TagKey;
import net.minecraft.world.level.material.Fluid;
import net.minecraft.world.phys.Vec3;
public class EntityFluidInteraction {
    private final java.util.List<Tracker> trackers = new java.util.ArrayList<>();
    private final java.util.Map<TagKey<Fluid>, CurrentAccumulator> currents = new java.util.LinkedHashMap<>();
    public EntityFluidInteraction(Set<TagKey<Fluid>> fluidsWithCurrent) {
        for (TagKey<Fluid> fluid : fluidsWithCurrent) {
            currents.put(fluid, new CurrentAccumulator());
        }
    }
    public boolean update(Entity entity, boolean ignoreCurrent) {
        trackers.removeIf(Tracker::reset);
        currents.values().forEach(CurrentAccumulator::reset);
        AABB area = entity.getFluidInteractionBox();
        if (area == null) {
            return false;
        }
        int minX = Mth.floor(area.minX);
        int minY = Mth.floor(area.minY);
        int minZ = Mth.floor(area.minZ);
        int maxX = Mth.ceil(area.maxX) - 1;
        int maxY = Mth.ceil(area.maxY) - 1;
        int maxZ = Mth.ceil(area.maxZ) - 1;
        double feet = entity.getBoundingBox().minY;
        double eye = entity.getEyeY();
        boolean sawFluid = false;
        BlockPos.MutableBlockPos cursor = new BlockPos.MutableBlockPos();
        for (int x = minX; x <= maxX; x++) {
            for (int y = minY; y <= maxY; y++) {
                for (int z = minZ; z <= maxZ; z++) {
                    cursor.set(x, y, z);
                    FluidState fluid = entity.level().getFluidState(cursor);
                    if (fluid.isEmpty()) {
                        continue;
                    }
                    double top = y + fluid.getHeight(entity.level(), cursor);
                    if (top < area.minY) {
                        continue;
                    }
                    sawFluid = true;
                    Holder<Fluid> kind = fluid.typeHolder();
                    Tracker tracker = trackerFor(kind);
                    if (x == entity.getBlockX() && z == entity.getBlockZ() && eye >= y
                        && eye <= y + fluid.getHeightForCamera(entity.level(), cursor)) {
                        tracker.eyesInside = true;
                    }
                    tracker.height = Math.max(top - feet, tracker.height);
                    CurrentAccumulator current = ignoreCurrent ? null : currentFor(kind);
                    if (current != null) {
                        Vec3 flow = fluid.getFlow(entity.level(), cursor);
                        current.height = Math.max(tracker.height, current.height);
                        current.accumulate(current.height < 0.4 ? flow.scale(current.height) : flow);
                    }
                }
            }
        }
        return sawFluid;
    }
    public void applyCurrentTo(TagKey<Fluid> fluid, Entity entity, double scale) {
        CurrentAccumulator current = currents.get(fluid);
        if (current != null) {
            current.applyTo(entity, scale);
        }
    }
    public double getFluidHeight(TagKey<Fluid> fluid) {
        double best = 0.0;
        for (Tracker tracker : trackers) {
            if (tracker.height > best && tracker.fluidType.is(fluid)) {
                best = tracker.height;
            }
        }
        return best;
    }
    public boolean isInFluid(TagKey<Fluid> fluid) {
        return getFluidHeight(fluid) > 0.0;
    }
    public boolean isEyeInFluid(TagKey<Fluid> fluid) {
        for (Tracker tracker : trackers) {
            if (tracker.eyesInside && tracker.fluidType.is(fluid)) {
                return true;
            }
        }
        return false;
    }
    private Tracker trackerFor(Holder<Fluid> kind) {
        for (Tracker each : trackers) {
            if (each.fluidType.equals(kind)) {
                return each;
            }
        }
        Tracker created = new Tracker(kind);
        trackers.add(created);
        return created;
    }
    private CurrentAccumulator currentFor(Holder<Fluid> kind) {
        for (java.util.Map.Entry<TagKey<Fluid>, CurrentAccumulator> each : currents.entrySet()) {
            if (kind.is(each.getKey())) {
                return each.getValue();
            }
        }
        return null;
    }
    static class Tracker {
        private final Holder<Fluid> fluidType;
        private double height;
        private boolean eyesInside;
        public Tracker(Holder<Fluid> fluidType) {
            this.fluidType = fluidType;
        }
        public boolean reset() {
            if (height == 0.0 && !eyesInside) {
                return true;
            }
            height = 0.0;
            eyesInside = false;
            return false;
        }
    }
    static class CurrentAccumulator {
        private double height;
        private Vec3 accumulated = Vec3.ZERO;
        private int samples;
        private CurrentAccumulator() {
            this.accumulated = Vec3.ZERO;
        }
        public void reset() {
            height = 0.0;
            accumulated = Vec3.ZERO;
            samples = 0;
        }
        public void accumulate(Vec3 flow) {
            accumulated = accumulated.add(flow);
            samples++;
        }
        public void applyTo(Entity entity, double scale) {
            if (samples == 0 || accumulated.lengthSqr() < 1.0E-5) {
                return;
            }
            Vec3 push = entity instanceof Player ? accumulated.scale(1.0 / samples) : accumulated.normalize();
            push = push.scale(scale);
            Vec3 motion = entity.getDeltaMovement();
            if (Math.abs(motion.x) < 0.003 && Math.abs(motion.z) < 0.003 && push.length() < 0.0045) {
                push = push.normalize().scale(0.0045);
            }
            entity.addDeltaMovement(push);
        }
    }
}
