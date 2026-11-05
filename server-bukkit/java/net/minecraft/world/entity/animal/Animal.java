// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.ai.attributes.Attributes;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.server.level.ServerPlayer;
import net.minecraft.util.RandomSource;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.AgeableMob;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.BlockAndLightGetter;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.Vec3;
public abstract class Animal extends AgeableMob {
    protected static final int PARENT_AGE_AFTER_BREEDING = 6000;
    protected Animal(EntityType<? extends Animal> type, Level level) {
        super(type, level);
    }
    public static AttributeSupplier.Builder createAnimalAttributes() {
        return Mob.createMobAttributes().add(Attributes.TEMPT_RANGE, 10.0);
    }
    protected void customServerAiStep(ServerLevel level) {
    }
    public void aiStep() {
    }
    public float getWalkTargetValue(BlockPos pos, LevelReader level) {
        return 0.0F;
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public static boolean checkAnimalSpawnRules(EntityType<? extends Animal> type, LevelAccessor level, EntitySpawnReason spawnReason, BlockPos pos, RandomSource random) {
        return level.getBlockState(pos.below()).is(net.minecraft.tags.BlockTags.ANIMALS_SPAWNABLE_ON) && isBrightEnoughToSpawn(level, pos);
    }
    protected static boolean isBrightEnoughToSpawn(BlockAndLightGetter level, BlockPos pos) {
        return org.basaltmc.bukkit.facade.SpawnLight.raw(level, pos, 0) > 8;
    }
    public int getAmbientSoundInterval() {
        return 120;
    }
    public boolean removeWhenFarAway(double distSqr) {
        return false;
    }
    protected int getBaseExperienceReward(ServerLevel level) {
        return 1 + level.getRandom().nextInt(3);
    }
    public abstract boolean isFood(ItemStack itemStack);
    public InteractionResult mobInteract(Player player, InteractionHand hand) {
        return super.mobInteract(player, hand);
    }
    protected void playEatingSound() {
        return;
    }
    public boolean canFallInLove() {
        return getInLoveTime() <= 0;
    }
    public void setInLove(Player player) {
        setInLoveTime(600);
    }
    public void setInLoveTime(int time) {
        if (live() instanceof org.bukkit.entity.Animals animals) {
            animals.setLoveModeTicks(time);
        }
    }
    public int getInLoveTime() {
        return live() instanceof org.bukkit.entity.Animals animals ? animals.getLoveModeTicks() : 0;
    }
    public ServerPlayer getLoveCause() {
        return null;
    }
    public boolean isInLove() {
        return getInLoveTime() > 0;
    }
    public void resetLove() {
        setInLoveTime(0);
    }
    public boolean canMate(Animal partner) {
        return partner != this && partner.getClass() == getClass() && isInLove() && partner.isInLove();
    }
    public void spawnChildFromBreeding(ServerLevel level, Animal partner) {
        if (live() instanceof org.bukkit.entity.Animals mine && partner.live() instanceof org.bukkit.entity.Animals other) {
            org.bukkit.entity.Entity baby = mine.getWorld().spawnEntity(mine.getLocation(), mine.getType());
            if (baby instanceof org.bukkit.entity.Ageable ageable) {
                ageable.setBaby();
            }
            mine.setLoveModeTicks(0);
            other.setLoveModeTicks(0);
        }
    }
    public void finalizeSpawnChildFromBreeding(ServerLevel level, Animal partner, AgeableMob offspring) {
        setAge(6000);
        partner.setAge(6000);
        resetLove();
        partner.resetLove();
    }
    public void handleEntityEvent(byte id) {
    }
}
