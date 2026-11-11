// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.projectile.hurtingprojectile;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.SlotAccess;
import net.minecraft.world.entity.projectile.ItemSupplier;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.Vec3;
public abstract class Fireball extends AbstractHurtingProjectile implements ItemSupplier {
    private ItemStack held = ItemStack.EMPTY;
    public Fireball(EntityType<? extends Fireball> type, Level level) {
        super(type, level);
    }
    public Fireball(EntityType<? extends Fireball> type, double x, double y, double z, Vec3 direction, Level level) {
        super(type, x, y, z, direction, level);
    }
    public Fireball(EntityType<? extends Fireball> type, LivingEntity mob, Vec3 direction, Level level) {
        super(type, mob, direction, level);
    }
    public void setItem(ItemStack source) {
        this.held = source.isEmpty() ? new ItemStack(net.minecraft.world.item.Items.FIRE_CHARGE) : source.copyWithCount(1);
    }
    protected void playEntityOnFireExtinguishedSound() {
        }
    public ItemStack getItem() {
        return held.isEmpty() ? new ItemStack(net.minecraft.world.item.Items.FIRE_CHARGE) : held;
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public SlotAccess getSlot(int slot) {
        return super.getSlot(slot);
    }
    public boolean shouldRenderAtSqrDistance(double distance) {
        return super.shouldRenderAtSqrDistance(distance);
    }
}
