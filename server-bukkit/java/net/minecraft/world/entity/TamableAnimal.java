// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;
import java.util.Optional;
import net.minecraft.network.syncher.EntityDataAccessor;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.tags.TagKey;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.damagesource.DamageType;
import net.minecraft.world.entity.ai.goal.PanicGoal;
import net.minecraft.world.entity.animal.Animal;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.scores.PlayerTeam;
public abstract class TamableAnimal extends Animal implements OwnableEntity {
    public static final int TELEPORT_WHEN_DISTANCE_IS_SQ = 144;
    protected static final EntityDataAccessor<Byte> DATA_FLAGS_ID = null;
    protected static final EntityDataAccessor<Optional<EntityReference<LivingEntity>>> DATA_OWNERUUID_ID = null;
    protected TamableAnimal(EntityType<? extends TamableAnimal> type, Level level) {
        super(type, level);
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public boolean canBeLeashed() {
        return true;
    }
    protected void spawnTamingParticles(boolean success) {
        org.bukkit.Particle particle = success ? org.bukkit.Particle.HEART : org.bukkit.Particle.SMOKE;
        if (live() != null) {
            live().getWorld().spawnParticle(particle, live().getLocation().add(0.0, getBbHeight() + 0.5, 0.0), 7, 0.5, 0.5, 0.5, 0.02);
        }
    }
    public void handleEntityEvent(byte id) {
    }
    public boolean isTame() {
        return live() instanceof org.bukkit.entity.Tameable tameable && tameable.isTamed();
    }
    public void setTame(boolean isTame, boolean includeSideEffects) {
        if (live() instanceof org.bukkit.entity.Tameable tameable) {
            tameable.setTamed(isTame);
        }
    }
    protected void applyTamingSideEffects() {
        return;
    }
    protected void feed(Player player, InteractionHand hand, ItemStack itemStack, float healingFactor, float defaultHeal) {
        net.minecraft.world.food.FoodProperties food = itemStack.get(net.minecraft.core.component.DataComponents.FOOD);
        heal(food != null ? food.nutrition() * healingFactor : defaultHeal);
        itemStack.consume(1, player);
    }
    public boolean isInSittingPose() {
        return live() instanceof org.bukkit.entity.Sittable sittable && sittable.isSitting();
    }
    public void setInSittingPose(boolean value) {
        if (live() instanceof org.bukkit.entity.Sittable sittable) {
            sittable.setSitting(value);
        }
    }
    public EntityReference<LivingEntity> getOwnerReference() {
        return live() instanceof org.bukkit.entity.Tameable tameable && tameable.getOwnerUniqueId() != null
            ? EntityReference.of(tameable.getOwnerUniqueId()) : null;
    }
    public void setOwner(LivingEntity owner) {
        if (live() instanceof org.bukkit.entity.Tameable tameable) {
            org.bukkit.entity.Entity target = owner == null ? null : owner.live();
            tameable.setOwner(target instanceof org.bukkit.entity.AnimalTamer tamer ? tamer : null);
        }
    }
    public void setOwnerReference(EntityReference<LivingEntity> owner) {
        if (live() instanceof org.bukkit.entity.Tameable tameable) {
            org.bukkit.entity.AnimalTamer tamer = owner == null ? null : org.basaltmc.bukkit.facade.Owners.resolve(owner.getUUID());
            tameable.setOwner(tamer);
        }
    }
    public void tame(Player player) {
        setTame(true, true);
        setOwner(player);
    }
    public boolean canAttack(LivingEntity target) {
        return !isOwnedBy(target) && super.canAttack(target);
    }
    public boolean isOwnedBy(LivingEntity entity) {
        return live() instanceof org.bukkit.entity.Tameable tameable && tameable.getOwnerUniqueId() != null
            && tameable.getOwnerUniqueId().equals(entity.getUUID());
    }
    public boolean wantsToAttack(LivingEntity target, LivingEntity owner) {
        return true;
    }
    public PlayerTeam getTeam() {
        PlayerTeam own = super.getTeam();
        if (own != null || !isTame()) {
            return own;
        }
        LivingEntity root = getRootOwner();
        return root == null ? null : root.getTeam();
    }
    protected boolean considersEntityAsAlly(Entity other) {
        if (isTame()) {
            LivingEntity root = getRootOwner();
            return other == root || root != null && root.isAlliedTo(other);
        }
        return super.considersEntityAsAlly(other);
    }
    public void die(DamageSource source) {
        super.die(source);
    }
    public boolean isOrderedToSit() {
        return isInSittingPose();
    }
    public void setOrderedToSit(boolean orderedToSit) {
        setInSittingPose(orderedToSit);
    }
    public void tryToTeleportToOwner() {
        LivingEntity owner = getOwner();
        if (owner != null && live() != null && owner.live() != null) {
            live().teleport(owner.live().getLocation());
        }
    }
    public boolean shouldTryTeleportToOwner() {
        return false;
    }
    public final boolean unableToMoveToOwner() {
        return isOrderedToSit() || isPassenger() || getOwner() == null || getOwner().isSpectator();
    }
    protected boolean canFlyToOwner() {
        return false;
    }
    public class TamableAnimalPanicGoal extends PanicGoal {
        public TamableAnimalPanicGoal(double speedModifier, TagKey<DamageType> panicCausingDamageTypes) {
            super(TamableAnimal.this, speedModifier);
        }
        public TamableAnimalPanicGoal(double speedModifier) {
            super(TamableAnimal.this, speedModifier);
        }
        public void tick() {
        }
    }
}
