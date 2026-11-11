// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.material.PushReaction;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
public class OminousItemSpawner extends Entity {
    private ItemStack stored = ItemStack.EMPTY;
    public static final int TICKS_BEFORE_ABOUT_TO_SPAWN_SOUND = 36;
    public OminousItemSpawner(EntityType<? extends OminousItemSpawner> type, Level level) {
        super(type, level);
    }
    public static OminousItemSpawner create(Level level, ItemStack item) {
        OminousItemSpawner spawner = new OminousItemSpawner(EntityTypes.OMINOUS_ITEM_SPAWNER, level);
        spawner.stored = item;
        return spawner;
    }
    public void tick() {
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected boolean canAddPassenger(Entity passenger) {
        return false;
    }
    protected boolean couldAcceptPassenger() {
        return false;
    }
    protected void addPassenger(Entity passenger) {
        }
    public PushReaction getPistonPushReaction() {
        return super.getPistonPushReaction();
    }
    public boolean isIgnoringBlockTriggers() {
        return super.isIgnoringBlockTriggers();
    }
    public void addParticles() {
    }
    public ItemStack getItem() {
        return stored;
    }
    public final boolean hurtServer(ServerLevel level, DamageSource source, float damage) {
        return super.hurtServer(level, source, damage);
    }
}
