// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.allay;
import com.google.common.collect.ImmutableList;
import java.util.function.BiConsumer;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.core.Vec3i;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.tags.TagKey;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.SimpleContainer;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.EquipmentSlot;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.entity.ai.Brain;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.ai.navigation.PathNavigation;
import net.minecraft.world.entity.item.ItemEntity;
import net.minecraft.world.entity.npc.InventoryCarrier;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.gameevent.DynamicGameEventListener;
import net.minecraft.world.level.gameevent.GameEvent;
import net.minecraft.world.level.gameevent.GameEventListener;
import net.minecraft.world.level.gameevent.PositionSource;
import net.minecraft.world.level.gameevent.vibrations.VibrationSystem;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.Vec3;
public class Allay extends PathfinderMob implements InventoryCarrier, VibrationSystem {
    private final VibrationSystem.Data vibrationData = new VibrationSystem.Data();
    private final VibrationSystem.User vibrationUser = new VibrationUser();

    public static final int MAX_NOTEBLOCK_DISTANCE = 1024;
    public static final ImmutableList<Float> THROW_SOUND_PITCHES = null;
    public Allay(EntityType<? extends Allay> type, Level level) {
        super(type, level);
    }
    private BlockPos jukeboxPos;
    protected Brain<Allay> makeBrain(Brain.Packed packedBrain) {
        @SuppressWarnings("unchecked") Brain<Allay> made = (Brain<Allay>) (Brain<?>) super.makeBrain(packedBrain); return made;
    }
    public Brain<Allay> getBrain() {
        @SuppressWarnings("unchecked") Brain<Allay> made = (Brain<Allay>) (Brain<?>) super.getBrain(); return made;
    }
    public static AttributeSupplier.Builder createAttributes() {
        return net.minecraft.world.entity.Mob.createMobAttributes()
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MAX_HEALTH, 20.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.FLYING_SPEED, 0.1)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MOVEMENT_SPEED, 0.1)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.ATTACK_DAMAGE, 2.0);
    }
    protected PathNavigation createNavigation(Level level) {
        return super.createNavigation(level);
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public void travel(Vec3 input) {
        move(net.minecraft.world.entity.MoverType.SELF, getDeltaMovement());
        setDeltaMovement(getDeltaMovement().scale(0.91));
    }
    public boolean hurtServer(ServerLevel level, DamageSource source, float damage) {
        return super.hurtServer(level, source, damage);
    }
    protected boolean considersEntityAsAlly(Entity other) {
        return false;
    }
    protected void playStepSound(BlockPos pos, BlockState blockState) {
    }
    protected void checkFallDamage(double ya, boolean onGround, BlockState onState, BlockPos pos) {
        }
    protected SoundEvent getAmbientSound() {
        return null;
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return net.minecraft.sounds.SoundEvents.byId("entity.allay.hurt");
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.allay.death");
    }
    protected float getSoundVolume() {
        return live() instanceof org.bukkit.entity.Allay api ? (float) api.getSoundVolume() : 0;
    }
    protected void customServerAiStep(ServerLevel level) {
    }
    public void aiStep() {
    }
    public void tick() {
    }
    public boolean hasItemInHand() {
        return !getItemInHand(net.minecraft.world.InteractionHand.MAIN_HAND).isEmpty();
    }
    protected boolean canDispenserEquipIntoSlot(EquipmentSlot slot) {
        return false;
    }
    protected InteractionResult mobInteract(Player player, InteractionHand hand) {
        return net.minecraft.world.InteractionResult.PASS;
    }
    public void setJukeboxPlaying(BlockPos jukebox, boolean isPlaying) {
        this.jukeboxPos = isPlaying ? jukebox : null;
    }
    public SimpleContainer getInventory() {
        SimpleContainer container = new SimpleContainer(1);
        if (live() instanceof org.bukkit.entity.Allay api) {
            org.bukkit.inventory.ItemStack[] contents = api.getInventory().getContents();
            if (contents.length > 0 && contents[0] != null) {
                container.setItem(0, org.bukkit.craftbukkit.inventory.CraftItemStack.asNMSCopy(contents[0]));
            }
        }
        return container;
    }
    protected void pickUpItem(ServerLevel level, ItemEntity entity) {
        }
    public boolean isFlapping() {
        return super.isFlapping();
    }
    public boolean isDancing() {
        return live() instanceof org.bukkit.entity.Allay api ? (boolean) api.isDancing() : false;
    }
    public void setDancing(boolean isDancing) {
        if (live() instanceof org.bukkit.entity.Allay api) {
            if (isDancing) { api.startDancing(); } else { api.stopDancing(); }
        }
    }
    public float getHoldingItemAnimationProgress(float a) {
        return hasItemInHand() ? 1.0F : 0.0F;
    }
    public boolean isSpinning() {
        return false;
    }
    public float getSpinningProgress(float a) {
        return 0.0F;
    }
    public boolean equipmentHasChanged(ItemStack previous, ItemStack current) {
        return previous.getItem() != current.getItem() || previous.getCount() != current.getCount();
    }
    protected void dropEquipment(ServerLevel level) {
    }
    public boolean removeWhenFarAway(double distSqr) {
        return super.removeWhenFarAway(distSqr);
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    protected boolean shouldStayCloseToLeashHolder() {
        return false;
    }
    public Vec3 getLeashOffset() {
        return new net.minecraft.world.phys.Vec3(0.0, getEyeHeight() * 0.6, getBbWidth() * 0.1);
    }
    public void handleEntityEvent(byte id) {
    }
    public VibrationSystem.Data getVibrationData() {
        return vibrationData;
    }
    public VibrationSystem.User getVibrationUser() {
        return vibrationUser;
    }
    class JukeboxListener implements GameEventListener {
        public JukeboxListener(PositionSource listenerSource, int listenerRadius) {
            
        }
        public PositionSource getListenerSource() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.animal.allay.Allay.JukeboxListener#getListenerSource");
        }
        public int getListenerRadius() {
            return 16;
        }
        public boolean handleGameEvent(ServerLevel level, Holder<GameEvent> event, GameEvent.Context context, Vec3 sourcePosition) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.animal.allay.Allay.JukeboxListener#handleGameEvent");
        }
    }
    class VibrationUser implements VibrationSystem.User {
        private final net.minecraft.world.level.gameevent.PositionSource source = new net.minecraft.world.level.gameevent.EntityPositionSource(Allay.this, Allay.this.getEyeHeight());

        private VibrationUser() {
        }
        public int getListenerRadius() {
            return 16;
        }
        public PositionSource getPositionSource() {
            return source;
        }
        public boolean canReceiveVibration(ServerLevel level, BlockPos pos, Holder<GameEvent> event, GameEvent.Context context) {
            return isAlive() && !isNoAi();
        }
        public void onReceiveVibration(ServerLevel level, BlockPos pos, Holder<GameEvent> event, Entity sourceEntity, Entity projectileOwner, float receivingDistance) {
        }
        public TagKey<GameEvent> getListenableEvents() {
            return net.minecraft.tags.GameEventTags.ALLAY_CAN_LISTEN;
        }
    }
}
