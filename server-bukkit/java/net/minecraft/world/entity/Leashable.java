// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;
import com.mojang.datafixers.util.Either;
import com.mojang.serialization.Codec;
import java.util.List;
import java.util.UUID;
import java.util.function.Predicate;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.Vec3;
public interface Leashable {
    String LEASH_TAG = "leash";
    double LEASH_TOO_FAR_DIST = 12.0;
    double LEASH_ELASTIC_DIST = 6.0;
    double MAXIMUM_ALLOWED_LEASHED_DIST = 16.0;
    Vec3 AXIS_SPECIFIC_ELASTICITY = new Vec3(0.8, 0.2, 0.8);
    float SPRING_DAMPENING = 0.7F;
    double TORSIONAL_ELASTICITY = 10.0;
    double STIFFNESS = 0.11;
    List<Vec3> ENTITY_ATTACHMENT_POINT = List.of(new Vec3(0.0, 0.5, 0.5));
    List<Vec3> LEASHER_ATTACHMENT_POINT = List.of(new Vec3(0.0, 0.5, 0.0));
    List<Vec3> SHARED_QUAD_ATTACHMENT_POINTS = List.of(new Vec3(-0.5, 0.5, 0.5), new Vec3(-0.5, 0.5, -0.5), new Vec3(0.5, 0.5, -0.5), new Vec3(0.5, 0.5, 0.5));
    LeashData getLeashData();
    void setLeashData(LeashData leashData);
    default boolean isLeashed() {
        return getLeashHolder() != null;
    }
    default boolean mayBeLeashed() {
        return true;
    }
    default boolean canHaveALeashAttachedTo(Entity entity) {
        return entity != this && leashDistanceTo(entity) <= leashSnapDistance() && canBeLeashed();
    }
    default double leashDistanceTo(Entity entity) {
        return ((Entity) this).distanceTo(entity);
    }
    default boolean canBeLeashed() {
        return true;
    }
    default void setDelayedLeashHolderId(int entityId) {
        setLeashData(new LeashData(entityId));
    }
    default void readLeashData(ValueInput input) {

    }
    default void writeLeashData(ValueOutput output, LeashData leashData) {

    }
    default void dropLeash() {
        removeLeash();
    }
    default void removeLeash() {
        if (this instanceof Entity entity && entity.live() instanceof org.bukkit.entity.LivingEntity living) {
            living.setLeashHolder(null);
        }
    }
    default void onLeashRemoved() {
        return;
    }
    static <E extends Entity & Leashable> void tickLeash(ServerLevel level, E entity) {
        Entity holder = entity.getLeashHolder();
        if (holder == null || holder.level() != entity.level()) {
            return;
        }
        double distance = entity.leashDistanceTo(holder);
        entity.whenLeashedTo(holder);
        if (distance > entity.leashSnapDistance()) {
            entity.leashTooFarBehaviour();
        } else if (distance > entity.leashElasticDistance() - holder.getBbWidth() - entity.getBbWidth()
            && entity.checkElasticInteractions(holder, entity.getLeashData())) {
            entity.onElasticLeashPull();
        } else {
            entity.closeRangeLeashBehaviour(holder);
        }
        LeashData data = entity.getLeashData();
        if (data != null) {
            entity.setYRot((float) (entity.getYRot() - data.angularMomentum));
            data.angularMomentum *= angularFriction(entity);
        }
    }
    default void onElasticLeashPull() {
        return;
    }
    default double leashSnapDistance() {
        return 12.0;
    }
    default double leashElasticDistance() {
        return 6.0;
    }
    static <E extends Entity & Leashable> float angularFriction(E entity) {
        float friction = entity.onGround()
            ? entity.level().getBlockState(entity.getBlockPosBelowThatAffectsMyMovement()).getBlock().getFriction() : 0.0F;
        return org.basaltmc.bukkit.MechanicsNatives.angularFriction(entity.onGround(), friction, entity.isInLiquid());
    }
    default void whenLeashedTo(Entity leashHolder) {
        return;
    }
    default void leashTooFarBehaviour() {
        removeLeash();
    }
    default void closeRangeLeashBehaviour(Entity leashHolder) {
        return;
    }
    default boolean checkElasticInteractions(Entity leashHolder, LeashData leashData) {
        Entity self = (Entity) this;
        boolean quad = leashHolder.supportQuadLeashAsHolder() && supportQuadLeash();
        Vec3 known = self.getKnownMovement();
        double[] pull = org.basaltmc.bukkit.MechanicsNatives.leashPull(body(self), body(leashHolder), quad, leashElasticDistance(),
            vector(movementOf(self)), vector(movementOf(leashHolder)), vector(known));
        if (pull == null) {
            return false;
        }
        leashData.angularMomentum += pull[0];
        self.addDeltaMovement(new Vec3(pull[1], pull[2], pull[3]));
        return true;
    }
    private static double[] body(Entity entity) {
        return new double[] {entity.getX(), entity.getY(), entity.getZ(), entity.getBbWidth(), entity.getBbHeight(), entity.getYRot()};
    }
    private static double[] vector(Vec3 v) {
        return new double[] {v.x, v.y, v.z};
    }
    private static Vec3 movementOf(Entity holder) {
        return holder instanceof Mob mob && mob.isNoAi() ? Vec3.ZERO : holder.getKnownMovement();
    }
    default boolean supportQuadLeash() {
        return false;
    }
    default Vec3[] getQuadLeashOffsets() {
        return createQuadLeashOffsets((Entity) this, 0.0, 0.5, 0.5, 0.5);
    }
    static Vec3[] createQuadLeashOffsets(Entity entity, double frontOffset, double frontBack, double leftRight, double height) {
        double width = entity.getBbWidth();
        double front = frontOffset * width;
        double depth = frontBack * width;
        double side = leftRight * width;
        double up = height * entity.getBbHeight();
        return new Vec3[] {new Vec3(-side, up, depth + front), new Vec3(-side, up, -depth + front), new Vec3(side, up, -depth + front), new Vec3(side, up, depth + front)};
    }
    default Vec3 getLeashOffset(float partialTicks) {
        return new Vec3(0.0, ((Entity) this).getEyeHeight() * 0.75F, ((Entity) this).getBbWidth() * 0.4F);
    }
    default Vec3 getLeashOffset() {
        return getLeashOffset(1.0F);
    }
    default void setLeashedTo(Entity holder, boolean synch) {
        if (this instanceof Entity entity && entity.live() instanceof org.bukkit.entity.LivingEntity living && holder.live() != null) {
            living.setLeashHolder(holder.live());
        }
    }
    default Entity getLeashHolder() {
        if (this instanceof Entity entity && entity.live() instanceof org.bukkit.entity.LivingEntity living && living.isLeashed()) {
            return entity.wrap(living.getLeashHolder());
        }
        return null;
    }
    static List<Leashable> leashableLeashedTo(Entity entity) {
        return leashableInArea(entity, leashable -> leashable.getLeashHolder() == entity);
    }
    static List<Leashable> leashableInArea(Entity entity, Predicate<Leashable> test) {
        return leashableInArea(entity.level(), entity.getBoundingBox().getCenter(), test);
    }
    static List<Leashable> leashableInArea(Level level, Vec3 pos, Predicate<Leashable> test) {
        net.minecraft.world.phys.AABB area = net.minecraft.world.phys.AABB.ofSize(pos, 32.0, 32.0, 32.0);
        return level.getEntitiesOfClass(Entity.class, area, found -> found instanceof Leashable leashable && test.test(leashable)).stream()
            .map(found -> (Leashable) found).toList();
    }
    public static final class LeashData {
        public static final Codec<LeashData> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.Leashable.LeashData.class, "CODEC");
        private int delayedLeashHolderId;
        public Entity leashHolder;
        public Either<UUID, BlockPos> delayedLeashInfo;
        public double angularMomentum;
        private LeashData(Either<UUID, BlockPos> delayedLeashInfo) {
            this.delayedLeashInfo = delayedLeashInfo;
        }
        private LeashData(Entity entity) {
            this.leashHolder = entity;
        }
        private LeashData(int entityId) {
            this.delayedLeashHolderId = entityId;
        }
        public void setLeashHolder(Entity leashHolder) {
            this.leashHolder = leashHolder;
            this.delayedLeashInfo = null;
            this.delayedLeashHolderId = 0;
        }
    }
    public static record Wrench(Vec3 force, double torque) {
        public static final Wrench ZERO = new Wrench(Vec3.ZERO, 0.0);
        public static double torqueFromForce(Vec3 leverArm, Vec3 force) {
            return leverArm.z * force.x - leverArm.x * force.z;
        }
        public static Wrench accumulate(List<Wrench> wrenches) {
            if (wrenches.isEmpty()) {
                return ZERO;
            }
            double x = 0.0;
            double y = 0.0;
            double z = 0.0;
            double t = 0.0;
            for (Wrench wrench : wrenches) {
                x += wrench.force.x;
                y += wrench.force.y;
                z += wrench.force.z;
                t += wrench.torque;
            }
            return new Wrench(new Vec3(x, y, z), t);
        }
        public Wrench scale(double scale) {
            return new Wrench(force.scale(scale), torque * scale);
        }
    }

}
