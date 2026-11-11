// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.boss.enderdragon;
import net.minecraft.core.BlockPos;
import net.minecraft.network.protocol.game.ClientboundAddEntityPacket;
import net.minecraft.network.syncher.EntityDataAccessor;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.sounds.SoundSource;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.effect.MobEffectInstance;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.MoveSimulationType;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.boss.enderdragon.phases.EnderDragonPhaseManager;
import net.minecraft.world.entity.monster.Enemy;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.dimension.end.EnderDragonFight;
import net.minecraft.world.level.pathfinder.Node;
import net.minecraft.world.level.pathfinder.Path;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.Vec3;
public class EnderDragon extends Mob implements Enemy {
    public static final EntityDataAccessor<Integer> DATA_PHASE = net.minecraft.network.syncher.EntityDataSerializers.INT.createAccessor(16);
    public final DragonFlightHistory flightHistory = null;
    public final EnderDragonPart head = null;
    public float oFlapTime;
    public float flapTime;
    public boolean inWall;
    public int dragonDeathTime;
    public float yRotA;
    public EndCrystal nearestCrystal;
    private EnderDragonPhaseManager phaseManager;
    public EnderDragon(EntityType<? extends EnderDragon> type, Level level) {
        super(type, level);
    }
    public void setDragonFight(EnderDragonFight fight) {
        this.dragonFight = fight;
    }
    public void setFightOrigin(BlockPos fightOrigin) {
        this.fightOrigin = fightOrigin;
    }
    public BlockPos getFightOrigin() {
        return fightOrigin;
    }
    public static AttributeSupplier.Builder createAttributes() {
        return net.minecraft.world.entity.Mob.createMobAttributes()
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MAX_HEALTH, 200.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.CAMERA_DISTANCE, 16.0);
    }
    public boolean isFlapping() {
        return super.isFlapping();
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public void aiStep() {
    }
    public boolean hurtServer(ServerLevel level, DamageSource source, float damage) {
        return super.hurtServer(level, source, damage);
    }
    protected void reallyHurt(ServerLevel level, DamageSource source, float damage) {
        }
    protected void handleKillingBlow() {
        }
    protected void tickDeath() {
    }
    public int findClosestNode() {
        return findClosestNode(getX(), getY(), getZ());
    }
    public int findClosestNode(double tX, double tY, double tZ) {
        boolean crystals = dragonFight != null && dragonFight.aliveCrystals() > 0;
        return org.basaltmc.bukkit.EntityNatives.call(level().getWorld().getName(), getId(), "dragon_closest_node", new Object[] {tX, tY, tZ, crystals})
            instanceof Long index ? index.intValue() : -1;
    }
    public Path findPath(int startIndex, int endIndex, Node finalNode) {
        boolean crystals = dragonFight != null && dragonFight.aliveCrystals() > 0;
        Object rows = finalNode == null
            ? org.basaltmc.bukkit.EntityNatives.call(level().getWorld().getName(), getId(), "dragon_find_path", new Object[] {(long) startIndex, (long) endIndex, crystals})
            : org.basaltmc.bukkit.EntityNatives.call(level().getWorld().getName(), getId(), "dragon_find_path",
                new Object[] {(long) startIndex, (long) endIndex, crystals, (long) finalNode.x, (long) finalNode.y, (long) finalNode.z});
        if (!(rows instanceof String[] lines) || lines.length == 0) {
            return null;
        }
        java.util.List<Node> nodes = new java.util.ArrayList<>();
        for (String line : lines) {
            String[] axes = line.split(",");
            nodes.add(new Node(Integer.parseInt(axes[0]), Integer.parseInt(axes[1]), Integer.parseInt(axes[2])));
        }
        Node last = nodes.get(nodes.size() - 1);
        return new Path(nodes, new BlockPos(last.x, last.y, last.z), true);
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public void checkDespawn() {
    }
    public EnderDragonPart[] getSubEntities() {
        return new EnderDragonPart[0];
    }
    public boolean isPickable() {
        return super.isPickable();
    }
    public SoundSource getSoundSource() {
        return super.getSoundSource();
    }
    protected SoundEvent getAmbientSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.ender_dragon.ambient");
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.ender_dragon.hurt");
    }
    protected float getSoundVolume() {
        return live() instanceof org.bukkit.entity.EnderDragon api ? (float) api.getSoundVolume() : 0;
    }
    public Vec3 getHeadLookVector(float a) {
        if (live() == null) {
            return getViewVector(1.0F);
        }
        org.bukkit.util.Vector direction = live().getLocation().getDirection();
        return new Vec3(direction.getX(), direction.getY(), direction.getZ());
    }
    public void onCrystalDestroyed(ServerLevel level, EndCrystal crystal, BlockPos pos, DamageSource source) {
        if (crystal == nearestCrystal) {
            nearestCrystal = null;
        }
    }
    public void onSyncedDataUpdated(EntityDataAccessor<?> accessor) {
    }
    public EnderDragonPhaseManager getPhaseManager() {
        if (phaseManager == null) {
            phaseManager = new EnderDragonPhaseManager(this);
        }
        return phaseManager;
    }
    public EnderDragonFight getDragonFight() {
        return dragonFight;
    }
    protected boolean canRide(Entity vehicle) {
        return false;
    }
    public void recreateFromPacket(ClientboundAddEntityPacket packet) {
        super.recreateFromPacket(packet);
    }
    protected float sanitizeScale(float scale) {
        return 0;
    }
    public MoveSimulationType getMoveSimulationType() {
        return super.getMoveSimulationType();
    }
    private EnderDragonFight dragonFight;
    private BlockPos fightOrigin = BlockPos.ZERO;

}
