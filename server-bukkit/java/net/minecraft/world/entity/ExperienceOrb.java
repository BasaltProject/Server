// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;
import net.minecraft.core.BlockPos;
import net.minecraft.network.syncher.EntityDataAccessor;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundSource;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.Vec3;
public class ExperienceOrb extends Entity {
    protected static final EntityDataAccessor<Integer> DATA_VALUE = net.minecraft.network.syncher.EntityDataSerializers.INT.createAccessor(8);
    public ExperienceOrb(Level level, double x, double y, double z, int value) {
        this(EntityTypes.EXPERIENCE_ORB, level);
        setPos(x, y, z);
        this.value = value;
    }
    public ExperienceOrb(Level level, Vec3 pos, Vec3 roughly, int value) {
        this(EntityTypes.EXPERIENCE_ORB, level);
        this.value = value;
        Vec3 push = new Vec3(random.nextDouble() * 0.2 - 0.1, random.nextDouble() * 0.2, random.nextDouble() * 0.2 - 0.1).scale(2.0);
        if (roughly.lengthSqr() > 0.0 && roughly.dot(push) < 0.0) {
            push = push.scale(-1.0);
        }
        setPos(roughly.lengthSqr() > 0.0 ? pos.add(roughly.normalize().scale(getBbWidth() * 0.5)) : pos);
        setDeltaMovement(push);
    }
    public ExperienceOrb(EntityType<? extends ExperienceOrb> type, Level level) {
        super(type, level);
    }
    protected void unstuckIfPossible(double maxDistance) {
        return;
    }
    protected Entity.MovementEmission getMovementEmission() {
        return Entity.MovementEmission.NONE;
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
        entityData.define(DATA_VALUE, 0);
    }
    protected double getDefaultGravity() {
        return 0.03;
    }
    public void tick() {
        super.tick();
    }
    protected float getAirDrag() {
        return 0.98F;
    }
    public BlockPos getBlockPosBelowThatAffectsMyMovement() {
        return BlockPos.containing(getX(), getBoundingBox().minY - 0.999999, getZ());
    }
    public static void award(ServerLevel level, Vec3 pos, int amount) {
        org.bukkit.World world = level.getWorld();
        int remaining = amount;
        while (remaining > 0) {
            int part = getExperienceValue(remaining);
            remaining -= part;
            world.spawn(new org.bukkit.Location(world, pos.x, pos.y, pos.z), org.bukkit.entity.ExperienceOrb.class, orb -> orb.setExperience(part));
        }
    }
    public static void awardWithDirection(ServerLevel level, Vec3 pos, Vec3 roughDirection, int amount) {
        org.bukkit.World world = level.getWorld();
        int remaining = amount;
        while (remaining > 0) {
            int part = getExperienceValue(remaining);
            remaining -= part;
            ExperienceOrb orb = new ExperienceOrb(level, pos, roughDirection, part);
            world.spawn(new org.bukkit.Location(world, orb.getX(), orb.getY(), orb.getZ()), org.bukkit.entity.ExperienceOrb.class, spawned -> {
                spawned.setExperience(part);
                spawned.setVelocity(new org.bukkit.util.Vector(orb.getDeltaMovement().x, orb.getDeltaMovement().y, orb.getDeltaMovement().z));
            });
        }
    }
    protected void doWaterSplashEffect() {
        return;
    }
    public final boolean hurtClient(DamageSource source) {
        return false;
    }
    public final boolean hurtServer(ServerLevel level, DamageSource source, float damage) {
        return false;
    }
    protected void addAdditionalSaveData(ValueOutput output) {
        output.putInt("Value", getValue());
    }
    protected void readAdditionalSaveData(ValueInput input) {
        value = input.getIntOr("Value", 0);
    }
    public void playerTouch(Player player) {
        return;
    }
    public int getValue() {
        return live() instanceof org.bukkit.entity.ExperienceOrb orb ? orb.getExperience() : value;
    }
    public int getIcon() {
        int value = getValue();
        if (value >= 2477) return 10;
        if (value >= 1237) return 9;
        if (value >= 617) return 8;
        if (value >= 307) return 7;
        if (value >= 149) return 6;
        if (value >= 73) return 5;
        if (value >= 37) return 4;
        if (value >= 17) return 3;
        if (value >= 7) return 2;
        if (value >= 3) return 1;
        return 0;
    }
    public static int getExperienceValue(int maxValue) {
        if (maxValue >= 2477) return 2477;
        if (maxValue >= 1237) return 1237;
        if (maxValue >= 617) return 617;
        if (maxValue >= 307) return 307;
        if (maxValue >= 149) return 149;
        if (maxValue >= 73) return 73;
        if (maxValue >= 37) return 37;
        if (maxValue >= 17) return 17;
        if (maxValue >= 7) return 7;
        return maxValue >= 3 ? 3 : 1;
    }
    public boolean isAttackable() {
        return false;
    }
    public SoundSource getSoundSource() {
        return SoundSource.AMBIENT;
    }
    protected InterpolationHandler createInterpolationHandler() {
        return null;
    }
    public MoveSimulationType getMoveSimulationType() {
        return MoveSimulationType.SERVER_AND_CLIENT;
    }
    private int value;

}
