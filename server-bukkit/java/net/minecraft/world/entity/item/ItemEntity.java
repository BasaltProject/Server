// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.item;
import java.util.UUID;
import net.minecraft.core.BlockPos;
import net.minecraft.network.chat.Component;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundSource;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.MoveSimulationType;
import net.minecraft.world.entity.SlotAccess;
import net.minecraft.world.entity.TraceableEntity;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Explosion;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.portal.TeleportTransition;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
public class ItemEntity extends Entity implements TraceableEntity {
    public static final float EYE_HEIGHT = 0.2125F;
    public static final int DEFAULT_HEALTH = 5;
    public final float bobOffs = 0;
    public ItemEntity(EntityType<? extends ItemEntity> type, Level level) {
        super(type, level);
    }
    public ItemEntity(Level level, double x, double y, double z, ItemStack itemStack) {
        super();
    }
    public ItemEntity(Level level, double x, double y, double z, ItemStack itemStack, double deltaX, double deltaY, double deltaZ) {
        super();
    }
    public boolean dampensVibrations() {
        return getItem().is(net.minecraft.tags.ItemTags.DAMPENS_VIBRATIONS);
    }
    public Entity getOwner() {
        return null;
    }
    protected Entity.MovementEmission getMovementEmission() {
        return Entity.MovementEmission.NONE;
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    protected double getDefaultGravity() {
        return 0.04;
    }
    public void tick() {
    }
    public BlockPos getBlockPosBelowThatAffectsMyMovement() {
        return getOnPos(0.999999F);
    }
    public static boolean areMergable(ItemStack thisItemStack, ItemStack otherItemStack) {
        return otherItemStack.getItem() == thisItemStack.getItem() && otherItemStack.getCount() + thisItemStack.getCount() <= otherItemStack.getMaxStackSize();
    }
    public static ItemStack merge(ItemStack toStack, ItemStack fromStack, int maxCount) {
        int moved = Math.min(Math.min(toStack.getMaxStackSize(), maxCount) - toStack.getCount(), fromStack.getCount());
        ItemStack merged = toStack.copyWithCount(toStack.getCount() + moved);
        fromStack.shrink(moved);
        return merged;
    }
    public boolean fireImmune() {
        return getItem().has(net.minecraft.core.component.DataComponents.DAMAGE_RESISTANT);
    }
    protected boolean shouldPlayLavaHurtSound() {
        return false;
    }
    public boolean ignoreExplosion(Explosion explosion) {
        return false;
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public Component getName() {
        return getItem().getHoverName();
    }
    public boolean isAttackable() {
        return false;
    }
    public ItemStack getItem() {
        org.bukkit.entity.Item item = live() instanceof org.bukkit.entity.Item i ? i : null;
        return item == null ? ItemStack.EMPTY : org.bukkit.craftbukkit.inventory.CraftItemStack.asNMSCopy(item.getItemStack());
    }
    public void setItem(ItemStack itemStack) {
        if (live() instanceof org.bukkit.entity.Item item) {
            item.setItemStack(org.bukkit.craftbukkit.inventory.CraftItemStack.asCraftMirror(itemStack));
        }
    }
    public void setTarget(UUID target) {
        if (live() instanceof org.bukkit.entity.Item item) {
            item.setOwner(target);
        }
    }
    public void setThrower(Entity thrower) {
        if (live() instanceof org.bukkit.entity.Item item) {
            item.setThrower(thrower == null ? null : thrower.getUUID());
        }
    }
    public int getAge() {
        return tickCount;
    }
    public void setDefaultPickUpDelay() {
        setPickUpDelay(10);
    }
    public void setNoPickUpDelay() {
        setPickUpDelay(0);
    }
    public void setNeverPickUp() {
        setPickUpDelay(32767);
    }
    public void setPickUpDelay(int ticks) {
        if (live() instanceof org.bukkit.entity.Item item) {
            item.setPickupDelay(ticks);
        }
    }
    public boolean hasPickUpDelay() {
        return live() instanceof org.bukkit.entity.Item item && item.getPickupDelay() > 0;
    }
    public void setUnlimitedLifetime() {
        if (live() instanceof org.bukkit.entity.Item item) {
            item.setUnlimitedLifetime(true);
        }
    }
    public void setExtendedLifetime() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.item.ItemEntity#setExtendedLifetime");
    }
    public void makeFakeItem() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.item.ItemEntity#makeFakeItem");
    }
    public static float getSpin(float ageInTicks, float bobOffset) {
        return ageInTicks / 20.0F + bobOffset;
    }
    public SoundSource getSoundSource() {
        return SoundSource.AMBIENT;
    }
}
