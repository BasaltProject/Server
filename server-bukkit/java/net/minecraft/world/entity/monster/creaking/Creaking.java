// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.monster.creaking;
import net.minecraft.core.BlockPos;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.AnimationState;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.LivingEntity;
import net.minecraft.world.entity.ai.Brain;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.ai.control.BodyRotationControl;
import net.minecraft.world.entity.ai.control.JumpControl;
import net.minecraft.world.entity.ai.control.LookControl;
import net.minecraft.world.entity.ai.control.MoveControl;
import net.minecraft.world.entity.ai.navigation.GroundPathNavigation;
import net.minecraft.world.entity.ai.navigation.PathNavigation;
import net.minecraft.world.entity.monster.Monster;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.pathfinder.PathFinder;
import net.minecraft.world.level.pathfinder.PathType;
import net.minecraft.world.level.pathfinder.PathfindingContext;
import net.minecraft.world.level.pathfinder.WalkNodeEvaluator;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
public class Creaking extends Monster {
    public static final int ATTACK_INTERVAL = 40;
    public static final float SPEED_MULTIPLIER_WHEN_IDLING = 0.3F;
    public static final int CREAKING_ORANGE = 16545810;
    public static final int CREAKING_GRAY = 6250335;
    public static final int INVULNERABILITY_ANIMATION_DURATION = 8;
    public static final int TWITCH_DEATH_DURATION = 45;
    public final AnimationState attackAnimationState = null;
    public final AnimationState invulnerabilityAnimationState = null;
    public final AnimationState deathAnimationState = null;
    public Creaking(EntityType<? extends Creaking> type, Level level) {
        super(type, level);
    }
    private BlockPos homePos;
    private boolean transientMob;
    private boolean tearingDown;
    private boolean canMove = true;
    private int eyeBlinkTicks;
    public void setTransient(BlockPos pos) {
        this.homePos = pos;
        this.transientMob = true;
    }
    public boolean isHeartBound() {
        return homePos != null && !transientMob;
    }
    protected BodyRotationControl createBodyControl() {
        return new CreakingBodyRotationControl(this);
    }
    protected Brain<Creaking> makeBrain(Brain.Packed packedBrain) {
        @SuppressWarnings("unchecked") Brain<Creaking> made = (Brain<Creaking>) (Brain<?>) super.makeBrain(packedBrain); return made;
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    public static AttributeSupplier.Builder createAttributes() {
        return Monster.createMonsterAttributes()
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MAX_HEALTH, 1.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.MOVEMENT_SPEED, 0.4)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.ATTACK_DAMAGE, 3.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.FOLLOW_RANGE, 32.0)
            .add(net.minecraft.world.entity.ai.attributes.Attributes.STEP_HEIGHT, 1.0625);
    }
    public boolean canMove() {
        return canMove;
    }
    public boolean doHurtTarget(ServerLevel level, Entity target) {
        return super.doHurtTarget(level, target);
    }
    public boolean hurtServer(ServerLevel level, DamageSource source, float damage) {
        return super.hurtServer(level, source, damage);
    }
    public Player blameSourceForDamage(DamageSource source) {
        return source.getEntity() instanceof Player player ? player : null;
    }
    public boolean isPushable() {
        return canMove && super.isPushable();
    }
    public void push(double xa, double ya, double za) {
        
    }
    public Brain<Creaking> getBrain() {
        @SuppressWarnings("unchecked") Brain<Creaking> made = (Brain<Creaking>) (Brain<?>) super.getBrain(); return made;
    }
    protected void customServerAiStep(ServerLevel level) {
    }
    public void aiStep() {
    }
    public void tick() {
    }
    protected void tickDeath() {
    }
    protected void updateWalkAnimation(float distance) {
        }
    public void tearDown() {
        tearingDown = true;
        discard();
    }
    public void creakingDeathEffects(DamageSource source) {
        tearingDown = true;
    }
    public void handleEntityEvent(byte id) {
    }
    public boolean fireImmune() {
        return true;
    }
    public boolean canUsePortal(boolean ignorePassenger) {
        return false;
    }
    protected PathNavigation createNavigation(Level level) {
        return super.createNavigation(level);
    }
    public boolean playerIsStuckInYou() {
        for (Player player : level().getEntitiesOfClass(Player.class, getBoundingBox().inflate(0.0, 0.0, 0.0))) {
            return true;
        }
        return false;
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    public void setHomePos(BlockPos pos) {
        this.homePos = pos;
    }
    public BlockPos getHomePos() {
        return homePos;
    }
    public void setTearingDown() {
        this.tearingDown = true;
    }
    public boolean isTearingDown() {
        return tearingDown;
    }
    public boolean hasGlowingEyes() {
        return isActive();
    }
    public void checkEyeBlink() {
        eyeBlinkTicks = eyeBlinkTicks > 0 ? eyeBlinkTicks - 1 : 0;
    }
    public void playAttackSound() {
    }
    protected SoundEvent getAmbientSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.creaking.ambient");
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return null;
    }
    protected SoundEvent getDeathSound() {
        return net.minecraft.sounds.SoundEvents.byId("entity.creaking.death");
    }
    protected void playStepSound(BlockPos pos, BlockState blockState) {
        playSound(net.minecraft.sounds.SoundEvents.byId("entity.creaking.step"), 0.15F, 1.0F);
    }
    public LivingEntity getTarget() {
        return super.getTarget();
    }
    public void knockback(double power, double xd, double zd, DamageSource source, float damage, boolean comesFromEffect) {
        if (canMove) {
            super.knockback(power, xd, zd, source, damage, comesFromEffect);
        }
    }
    public boolean checkCanMove() {
        boolean wasMoving = canMove;
        canMove = true;
        for (Player player : level().getEntitiesOfClass(Player.class, getBoundingBox().inflate(32.0))) {
            if (!player.isSpectator() && !player.isCreative() && isLookingAtMe(player, 0.025, true, false, getEyeY())) {
                canMove = false;
                break;
            }
        }
        return canMove != wasMoving;
    }
    public void activate(Player player) {
        if (live() instanceof org.bukkit.entity.Creaking api && player.getBukkitEntity() instanceof org.bukkit.entity.Player bukkitPlayer) {
            api.activate(bukkitPlayer);
        }
    }
    public void deactivate() {
        if (live() instanceof org.bukkit.entity.Creaking api) { api.deactivate(); }
    }
    public void setIsActive(boolean active) {
        if (live() instanceof org.bukkit.entity.Creaking api && !active) {
            api.deactivate();
        }
    }
    public boolean isActive() {
        return live() instanceof org.bukkit.entity.Creaking api ? (boolean) api.isActive() : false;
    }
    public float getWalkTargetValue(BlockPos pos, LevelReader level) {
        return super.getWalkTargetValue(pos, level);
    }
    class CreakingLookControl extends LookControl {
        public CreakingLookControl(Creaking creaking) {
            super(creaking);
        }
        public void tick() {
            if (Creaking.this.canMove()) {
                super.tick();
            }
        }
    }
    static class CreakingMoveControl<T extends Creaking> extends MoveControl<T> {
        public CreakingMoveControl(T creaking) {
            super(creaking);
        }
        public void tick() {
            if (mob.canMove()) {
                super.tick();
            }
        }
    }
    class CreakingJumpControl extends JumpControl {
        public CreakingJumpControl(Creaking creaking) {
            super(creaking);
        }
        public void tick() {
            if (Creaking.this.canMove()) {
                super.tick();
            } else {
                Creaking.this.setJumping(false);
            }
        }
    }
    class CreakingBodyRotationControl extends BodyRotationControl {
        public CreakingBodyRotationControl(Creaking creaking) {
            super(creaking);
        }
        public void clientTick() {
            if (Creaking.this.canMove()) {
                super.clientTick();
            }
        }
    }
    class HomeNodeEvaluator extends WalkNodeEvaluator {
        private HomeNodeEvaluator() {
            
        }
        public PathType getPathType(PathfindingContext context, int x, int y, int z) {
            return PathType.WALKABLE;
        }
    }
    class CreakingPathNavigation extends GroundPathNavigation {
        public CreakingPathNavigation(Creaking mob, Level level) {
            super(mob, level);
        }
        public void tick() {
        }
        protected PathFinder createPathFinder(int maxVisitedNodes) {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.entity.monster.creaking.Creaking.CreakingPathNavigation#createPathFinder");
        }
    }
}
