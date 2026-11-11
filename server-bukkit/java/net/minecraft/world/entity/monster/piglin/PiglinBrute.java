// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster.piglin;
import net.minecraft.world.item.Items;
import net.minecraft.core.BlockPos;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.util.RandomSource;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.SpawnGroupData;
import net.minecraft.world.entity.ai.Brain;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.block.state.BlockState;
public class PiglinBrute extends AbstractPiglin {
    public static final Brain.Provider<PiglinBrute> BRAIN_PROVIDER = Brain.provider(java.util.List.of(net.minecraft.world.entity.ai.memory.MemoryModuleType.NEAREST_VISIBLE_ADULT_PIGLINS), java.util.List.of(), PiglinBruteAi::getActivities);
    public PiglinBrute(EntityType<? extends PiglinBrute> type, Level level) {
        super(type, level);
    }
    public static AttributeSupplier.Builder createAttributes() {
        return net.minecraft.world.entity.Mob.createMobAttributes();
    }
    public SpawnGroupData finalizeSpawn(ServerLevelAccessor level, DifficultyInstance difficulty, EntitySpawnReason spawnReason, SpawnGroupData groupData) {
        return groupData;
    }
    protected void populateDefaultEquipmentSlots(RandomSource random, DifficultyInstance difficulty) {
        }
    protected Brain<PiglinBrute> makeBrain(Brain.Packed packedBrain) {
        @SuppressWarnings("unchecked") Brain<PiglinBrute> made = (Brain<PiglinBrute>) (Brain<?>) super.makeBrain(packedBrain); return made;
    }
    public Brain<PiglinBrute> getBrain() {
        @SuppressWarnings("unchecked") Brain<PiglinBrute> made = (Brain<PiglinBrute>) (Brain<?>) super.getBrain(); return made;
    }
    public boolean canHunt() {
        return false;
    }
    public boolean wantsToPickUp(ServerLevel level, ItemStack itemStack) {
        return itemStack.is(Items.GOLDEN_AXE) ? super.wantsToPickUp(level, itemStack) : false;
    }
    protected void customServerAiStep(ServerLevel level) {
    }
    public PiglinArmPose getArmPose() {
        return isAggressive() && isHoldingMeleeWeapon() ? PiglinArmPose.ATTACKING_WITH_MELEE_WEAPON : PiglinArmPose.DEFAULT;
    }
    public boolean hurtServer(ServerLevel level, DamageSource source, float damage) {
        return super.hurtServer(level, source, damage);
    }
    protected SoundEvent getAmbientSound() {
        return null;
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return null;
    }
    protected SoundEvent getDeathSound() {
        return null;
    }
    protected void playStepSound(BlockPos pos, BlockState blockState) {
    }
    protected void playAngrySound() {
        }
    protected void playConvertedSound() {
        }
}
