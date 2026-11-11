// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.projectile.arrow;
import java.util.List;
import net.minecraft.world.item.Items;
import net.minecraft.sounds.SoundEvents;
import java.util.Collection;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.BlockHitResult;
import net.minecraft.world.phys.EntityHitResult;
import net.minecraft.world.phys.Vec3;
public class ThrownTrident extends AbstractArrow {
    private boolean dealtDamage;
    public int clientSideReturnTridentTickCount;
    public ThrownTrident(EntityType<? extends ThrownTrident> type, Level level) {
        super(type, level);
    }
    public ThrownTrident(Level level, LivingEntity owner, ItemStack tridentItem) {
        super((net.minecraft.world.entity.EntityType) null, (net.minecraft.world.level.Level) null);
    }
    public ThrownTrident(Level level, double x, double y, double z, ItemStack tridentItem) {
        super((net.minecraft.world.entity.EntityType) null, (net.minecraft.world.level.Level) null);
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public void tick() {
    }
    public boolean isFoil() {
        return live() instanceof org.bukkit.entity.Trident api && api.hasGlint();
    }
    protected EntityHitResult findHitEntity(Vec3 from, Vec3 to) {
        return dealtDamage ? null : super.findHitEntity(from, to);
    }
    protected Collection<EntityHitResult> findHitEntities(Vec3 from, Vec3 to) {
        EntityHitResult hit = findHitEntity(from, to);
        return hit != null ? List.of(hit) : List.of();
    }
    protected void onHitEntity(EntityHitResult hitResult) {
        }
    protected void hitBlockEnchantmentEffects(ServerLevel level, BlockHitResult hitResult, ItemStack weapon) {
        }
    public ItemStack getWeaponItem() {
        return getPickupItemStackOrigin();
    }
    protected boolean tryPickup(Player player) {
        return false;
    }
    protected ItemStack getDefaultPickupItem() {
        return new ItemStack(Items.TRIDENT);
    }
    protected SoundEvent getDefaultHitGroundSoundEvent() {
        return SoundEvents.TRIDENT_HIT_GROUND;
    }
    public void playerTouch(Player player) {
        if (ownedBy(player) || getOwner() == null) {
            super.playerTouch(player);
        }
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    public void tickDespawn() {
        int loyalty = live() instanceof org.bukkit.entity.Trident trident ? trident.getLoyaltyLevel() : 0;
        if (pickup != AbstractArrow.Pickup.ALLOWED || loyalty <= 0) {
            super.tickDespawn();
        }
    }
    protected float getWaterInertia() {
        return 0;
    }
    public boolean shouldRender(double camX, double camY, double camZ) {
        return true;
    }
}
