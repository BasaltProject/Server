// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.projectile;
import net.minecraft.world.item.Items;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.Vec3;
public class EyeOfEnder extends Entity implements ItemSupplier {
    private net.minecraft.world.phys.Vec3 target = net.minecraft.world.phys.Vec3.ZERO;
    private int life;
    private boolean surviveAfterDeath;
    private ItemStack heldItem = new ItemStack(Items.ENDER_EYE);
    public EyeOfEnder(EntityType<? extends EyeOfEnder> type, Level level) {
        super(type, level);
    }
    public EyeOfEnder(Level level, double x, double y, double z) {
        super((net.minecraft.world.entity.EntityType) null, (net.minecraft.world.level.Level) null);
    }
    public void setItem(ItemStack source) {
        this.heldItem = source.isEmpty() ? new ItemStack(Items.ENDER_EYE) : source.copyWithCount(1);
    }
    public ItemStack getItem() {
        return heldItem;
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public boolean shouldRenderAtSqrDistance(double distance) {
        return super.shouldRenderAtSqrDistance(distance);
    }
    public void signalTo(Vec3 target) {
        Vec3 delta = target.subtract(position());
        double flat = delta.horizontalDistance();
        this.target = flat > 12.0 ? position().add(delta.x / flat * 12.0, 8.0, delta.z / flat * 12.0) : target;
        this.life = 0;
        this.surviveAfterDeath = random.nextInt(5) > 0;
    }
    public void tick() {
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public float getLightLevelDependentMagicValue() {
        return 1.0F;
    }
    public boolean isAttackable() {
        return false;
    }
    public boolean hurtServer(ServerLevel level, DamageSource source, float damage) {
        return super.hurtServer(level, source, damage);
    }
}
