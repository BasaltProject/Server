// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.item;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.util.RandomSource;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.MoveSimulationType;
import net.minecraft.world.entity.TraceableEntity;
import net.minecraft.world.level.ExplosionDamageCalculator;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.portal.TeleportTransition;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
public class PrimedTnt extends Entity implements TraceableEntity {
    public static final short DEFAULT_FUSE_TIME = (short) 80;
    public static final int NO_FUSE = -1;
    public static final String TAG_FUSE = "fuse";
    public static final ExplosionDamageCalculator USED_PORTAL_DAMAGE_CALCULATOR = null;
    public PrimedTnt(EntityType<? extends PrimedTnt> type, Level level) {
        super(type, level);
    }
    public PrimedTnt(Level level, double x, double y, double z, LivingEntity owner) {
        super();
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public boolean isPickable() {
        return !isRemoved();
    }
    protected double getDefaultGravity() {
        return 0.04;
    }
    public void tick() {
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public LivingEntity getOwner() {
        return live() instanceof org.bukkit.entity.TNTPrimed tnt && tnt.getSource() instanceof org.bukkit.entity.LivingEntity source ? (LivingEntity) wrap(source) : null;
    }
    public void setFuse(int time) {
        if (live() instanceof org.bukkit.entity.TNTPrimed tnt) {
            tnt.setFuseTicks(time);
        }
    }
    public int getFuse() {
        return live() instanceof org.bukkit.entity.TNTPrimed tnt ? tnt.getFuseTicks() : 80;
    }
    public static int getRandomShortFuse(int fuse, RandomSource random) {
        return random.nextInt(fuse / 4) + fuse / 8;
    }
    public void setBlockState(BlockState blockState) {
        if (live() instanceof org.bukkit.entity.TNTPrimed api) {
            api.setBlockData(org.bukkit.craftbukkit.block.data.CraftBlockData.fromData(blockState));
        }
    }
    public BlockState getBlockState() {
        return live() instanceof org.bukkit.entity.TNTPrimed api && api.getBlockData() instanceof org.bukkit.craftbukkit.block.data.CraftBlockData craft
            ? craft.getState() : net.minecraft.world.level.block.Blocks.TNT.defaultBlockState();
    }
    public final boolean hurtServer(ServerLevel level, DamageSource source, float damage) {
        return false;
    }
}
