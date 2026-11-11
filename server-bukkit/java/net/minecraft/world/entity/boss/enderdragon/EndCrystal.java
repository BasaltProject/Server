// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.boss.enderdragon;
import net.minecraft.world.item.Items;
import net.minecraft.core.BlockPos;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
public class EndCrystal extends Entity {
    private BlockPos beamTarget;
    private boolean showBottom = true;
    public int time;
    public EndCrystal(EntityType<? extends EndCrystal> type, Level level) {
        super(type, level);
    }
    public EndCrystal(Level level, double x, double y, double z) {
        super((net.minecraft.world.entity.EntityType) null, (net.minecraft.world.level.Level) null);
    }
    protected Entity.MovementEmission getMovementEmission() {
        return super.getMovementEmission();
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public void tick() {
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public boolean isPickable() {
        return super.isPickable();
    }
    public final boolean hurtClient(DamageSource source) {
        if (isInvulnerableToBase(source)) {
            return false;
        }
        return !(source.getEntity() instanceof EnderDragon);
    }
    public final boolean hurtServer(ServerLevel level, DamageSource source, float damage) {
        return super.hurtServer(level, source, damage);
    }
    public void kill(ServerLevel level) {
        super.kill(level);
    }
    public void setBeamTarget(BlockPos target) {
        this.beamTarget = target;
    }
    public BlockPos getBeamTarget() {
        return beamTarget;
    }
    public void setShowBottom(boolean showBottom) {
        this.showBottom = showBottom;
    }
    public boolean showsBottom() {
        return showBottom;
    }
    public boolean shouldRenderAtSqrDistance(double distance) {
        return super.shouldRenderAtSqrDistance(distance);
    }
    public ItemStack getPickResult() {
        return new ItemStack(Items.END_CRYSTAL);
    }
}
