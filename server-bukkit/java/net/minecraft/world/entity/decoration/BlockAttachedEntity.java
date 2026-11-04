// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.decoration;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LightningBolt;
import net.minecraft.world.entity.MoverType;
import net.minecraft.world.level.Explosion;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.Vec3;
public abstract class BlockAttachedEntity extends Entity {
    public static final String TAG_BLOCK_POS = "block_pos";
    protected BlockPos pos;
    protected BlockAttachedEntity(EntityType<? extends BlockAttachedEntity> type, Level level) {
        super(type, level);
    }
    protected BlockAttachedEntity(EntityType<? extends BlockAttachedEntity> type, Level level, BlockPos pos) {
        super(type, level);
    }
    protected abstract void recalculateBoundingBox();
    public void tick() {
    }
    protected void tickAtCheckInterval() {
        }
    public abstract boolean survives();
    public boolean isPickable() {
        return super.isPickable();
    }
    public boolean skipAttackInteraction(Entity source) {
        return false;
    }
    public void kill(ServerLevel level) {
        discard();
    }
    public void kill(ServerLevel level, Entity attributedTo) {
        discard();
    }
    protected void onKilled() {
        }
    public boolean hurtClient(DamageSource source) {
        return false;
    }
    public boolean hurtServer(ServerLevel level, DamageSource source, float damage) {
        return super.hurtServer(level, source, damage);
    }
    public boolean ignoreExplosion(Explosion explosion) {
        return super.ignoreExplosion(explosion);
    }
    public void move(MoverType moverType, Vec3 delta) {

    }
    public void push(double xa, double ya, double za) {
        
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public abstract void dropItem(ServerLevel level, Entity causedBy);
    protected boolean repositionEntityAfterLoad() {
        return false;
    }
    public void setPos(double x, double y, double z) {
        super.setPos(x, y, z);
    }
    public BlockPos getPos() {
        return pos != null ? pos : blockPosition();
    }
    public void thunderHit(ServerLevel level, LightningBolt lightningBolt) {
        super.thunderHit(level, lightningBolt);
    }
    public void refreshDimensions() {
    }
}
