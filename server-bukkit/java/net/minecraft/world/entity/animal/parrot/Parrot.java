// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.parrot;
import net.minecraft.core.Direction;
import net.minecraft.tags.BlockTags;
import net.minecraft.util.Mth;
import net.minecraft.world.entity.ai.util.LandRandomPos;
import net.minecraft.world.level.block.LeavesBlock;
import net.minecraft.world.entity.Mob;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import net.minecraft.sounds.SoundEvents;
import net.minecraft.world.Difficulty;
import net.minecraft.world.entity.EntityTypes;
import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import net.minecraft.core.BlockPos;
import net.minecraft.core.component.DataComponentGetter;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.syncher.SynchedEntityData;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.sounds.SoundEvent;
import net.minecraft.sounds.SoundSource;
import net.minecraft.util.RandomSource;
import net.minecraft.util.StringRepresentable;
import net.minecraft.world.DifficultyInstance;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.InteractionResult;
import net.minecraft.world.damagesource.DamageSource;
import net.minecraft.world.entity.AgeableMob;
import net.minecraft.world.entity.Entity;
import net.minecraft.world.entity.EntitySpawnReason;
import net.minecraft.world.entity.EntityType;
import net.minecraft.world.entity.PathfinderMob;
import net.minecraft.world.entity.SpawnGroupData;
import net.minecraft.world.entity.ai.attributes.AttributeSupplier;
import net.minecraft.world.entity.ai.goal.WaterAvoidingRandomFlyingGoal;
import net.minecraft.world.entity.ai.navigation.PathNavigation;
import net.minecraft.world.entity.animal.Animal;
import net.minecraft.world.entity.player.Player;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;
import net.minecraft.world.phys.Vec3;
public class Parrot extends ShoulderRidingEntity {
    private BlockPos jukeboxPos;
    private boolean partyParrot;
    public float flap;
    public float flapSpeed;
    public float oFlapSpeed;
    public float oFlap;
    public Parrot(EntityType<? extends Parrot> type, Level level) {
        super(type, level);
    }
    public SpawnGroupData finalizeSpawn(ServerLevelAccessor level, DifficultyInstance difficulty, EntitySpawnReason spawnReason, SpawnGroupData groupData) {
        return groupData;
    }
    protected boolean canBeABaby() {
        return false;
    }
    protected void registerGoals() {
    }
    public static AttributeSupplier.Builder createAttributes() {
        return net.minecraft.world.entity.Mob.createMobAttributes();
    }
    protected PathNavigation createNavigation(Level level) {
        return super.createNavigation(level);
    }
    public void aiStep() {
    }
    public void setRecordPlayingNearby(BlockPos jukebox, boolean isPlaying) {
        this.jukeboxPos = jukebox;
        this.partyParrot = isPlaying;
    }
    public boolean isPartyParrot() {
        return partyParrot;
    }
    public static boolean imitateNearbyMobs(Level level, Entity entity) {
        RandomSource random = level.getRandom();
        if (!entity.isAlive() || entity.isSilent() || random.nextInt(2) != 0) {
            return false;
        }
        List<Mob> mobs = level.getEntitiesOfClass(Mob.class, entity.getBoundingBox().inflate(20.0), mob -> !(mob instanceof Parrot));
        if (mobs.isEmpty()) {
            return false;
        }
        Mob mob = mobs.get(random.nextInt(mobs.size()));
        if (mob.isSilent()) {
            return false;
        }
        level.playSound(null, entity.getX(), entity.getY(), entity.getZ(), imitationOf(mob.getType()), entity.getSoundSource(), 0.7F, (random.nextFloat() - random.nextFloat()) * 0.2F + 1.0F);
        return true;
    }
    public InteractionResult mobInteract(Player player, InteractionHand hand) {
        return net.minecraft.world.InteractionResult.PASS;
    }
    public boolean isFood(ItemStack itemStack) {
        return live() instanceof org.bukkit.entity.Animals animals && animals.isBreedItem(org.bukkit.craftbukkit.inventory.CraftItemStack.asCraftMirror(itemStack));
    }
    public static boolean checkParrotSpawnRules(EntityType<Parrot> type, LevelAccessor level, EntitySpawnReason spawnReason, BlockPos pos, RandomSource random) {
        return true;
    }
    protected void checkFallDamage(double ya, boolean onGround, BlockState onState, BlockPos pos) {
        }
    public boolean canMate(Animal partner) {
        return false;
    }
    public AgeableMob getBreedOffspring(ServerLevel level, AgeableMob partner) {
        return null;
    }
    public SoundEvent getAmbientSound() {
        return null;
    }
    public static SoundEvent getAmbient(Level level, RandomSource random) {
        if (level.getDifficulty() != Difficulty.PEACEFUL && random.nextInt(1000) == 0) {
            List<EntityType<?>> keys = new ArrayList<>(kImitations.keySet());
            return imitationOf(keys.get(random.nextInt(keys.size())));
        }
        return SoundEvents.PARROT_AMBIENT;
    }
    protected SoundEvent getHurtSound(DamageSource source) {
        return null;
    }
    protected SoundEvent getDeathSound() {
        return null;
    }
    protected void playStepSound(BlockPos pos, BlockState blockState) {
    }
    protected boolean isFlapping() {
        return super.isFlapping();
    }
    protected void onFlap() {
        }
    public float getVoicePitch() {
        return getPitch(random);
    }
    public static float getPitch(RandomSource random) {
        return (random.nextFloat() - random.nextFloat()) * 0.2F + 1.0F;
    }
    public SoundSource getSoundSource() {
        return super.getSoundSource();
    }
    public boolean isPushable() {
        return true;
    }
    protected void doPush(Entity entity) {
    }
    public boolean hurtServer(ServerLevel level, DamageSource source, float damage) {
        return super.hurtServer(level, source, damage);
    }
    private Variant variant = Variant.DEFAULT;
    private static final Map<EntityType<?>, SoundEvent> kImitations = Map.ofEntries(
        Map.entry(EntityTypes.BLAZE, SoundEvents.PARROT_IMITATE_BLAZE),
        Map.entry(EntityTypes.BOGGED, SoundEvents.PARROT_IMITATE_BOGGED),
        Map.entry(EntityTypes.BREEZE, SoundEvents.PARROT_IMITATE_BREEZE),
        Map.entry(EntityTypes.CAMEL_HUSK, SoundEvents.PARROT_IMITATE_CAMEL_HUSK),
        Map.entry(EntityTypes.CAVE_SPIDER, SoundEvents.PARROT_IMITATE_SPIDER),
        Map.entry(EntityTypes.CREAKING, SoundEvents.PARROT_IMITATE_CREAKING),
        Map.entry(EntityTypes.CREEPER, SoundEvents.PARROT_IMITATE_CREEPER),
        Map.entry(EntityTypes.DROWNED, SoundEvents.PARROT_IMITATE_DROWNED),
        Map.entry(EntityTypes.ELDER_GUARDIAN, SoundEvents.PARROT_IMITATE_ELDER_GUARDIAN),
        Map.entry(EntityTypes.ENDER_DRAGON, SoundEvents.PARROT_IMITATE_ENDER_DRAGON),
        Map.entry(EntityTypes.ENDERMITE, SoundEvents.PARROT_IMITATE_ENDERMITE),
        Map.entry(EntityTypes.EVOKER, SoundEvents.PARROT_IMITATE_EVOKER),
        Map.entry(EntityTypes.GHAST, SoundEvents.PARROT_IMITATE_GHAST),
        Map.entry(EntityTypes.HAPPY_GHAST, SoundEvents.EMPTY),
        Map.entry(EntityTypes.GUARDIAN, SoundEvents.PARROT_IMITATE_GUARDIAN),
        Map.entry(EntityTypes.HOGLIN, SoundEvents.PARROT_IMITATE_HOGLIN),
        Map.entry(EntityTypes.HUSK, SoundEvents.PARROT_IMITATE_HUSK),
        Map.entry(EntityTypes.ILLUSIONER, SoundEvents.PARROT_IMITATE_ILLUSIONER),
        Map.entry(EntityTypes.MAGMA_CUBE, SoundEvents.PARROT_IMITATE_MAGMA_CUBE),
        Map.entry(EntityTypes.PARCHED, SoundEvents.PARROT_IMITATE_PARCHED),
        Map.entry(EntityTypes.PHANTOM, SoundEvents.PARROT_IMITATE_PHANTOM),
        Map.entry(EntityTypes.PIGLIN, SoundEvents.PARROT_IMITATE_PIGLIN),
        Map.entry(EntityTypes.PIGLIN_BRUTE, SoundEvents.PARROT_IMITATE_PIGLIN_BRUTE),
        Map.entry(EntityTypes.PILLAGER, SoundEvents.PARROT_IMITATE_PILLAGER),
        Map.entry(EntityTypes.RAVAGER, SoundEvents.PARROT_IMITATE_RAVAGER),
        Map.entry(EntityTypes.SHULKER, SoundEvents.PARROT_IMITATE_SHULKER),
        Map.entry(EntityTypes.SILVERFISH, SoundEvents.PARROT_IMITATE_SILVERFISH),
        Map.entry(EntityTypes.SKELETON, SoundEvents.PARROT_IMITATE_SKELETON),
        Map.entry(EntityTypes.SLIME, SoundEvents.PARROT_IMITATE_SLIME),
        Map.entry(EntityTypes.SPIDER, SoundEvents.PARROT_IMITATE_SPIDER),
        Map.entry(EntityTypes.STRAY, SoundEvents.PARROT_IMITATE_STRAY),
        Map.entry(EntityTypes.VEX, SoundEvents.PARROT_IMITATE_VEX),
        Map.entry(EntityTypes.VINDICATOR, SoundEvents.PARROT_IMITATE_VINDICATOR),
        Map.entry(EntityTypes.WARDEN, SoundEvents.PARROT_IMITATE_WARDEN),
        Map.entry(EntityTypes.WITCH, SoundEvents.PARROT_IMITATE_WITCH),
        Map.entry(EntityTypes.WITHER, SoundEvents.PARROT_IMITATE_WITHER),
        Map.entry(EntityTypes.WITHER_SKELETON, SoundEvents.PARROT_IMITATE_WITHER_SKELETON),
        Map.entry(EntityTypes.ZOGLIN, SoundEvents.PARROT_IMITATE_ZOGLIN),
        Map.entry(EntityTypes.ZOMBIE, SoundEvents.PARROT_IMITATE_ZOMBIE),
        Map.entry(EntityTypes.ZOMBIE_HORSE, SoundEvents.PARROT_IMITATE_ZOMBIE_HORSE),
        Map.entry(EntityTypes.ZOMBIE_NAUTILUS, SoundEvents.PARROT_IMITATE_ZOMBIE_NAUTILUS),
        Map.entry(EntityTypes.ZOMBIE_VILLAGER, SoundEvents.PARROT_IMITATE_ZOMBIE_VILLAGER));
    private static SoundEvent imitationOf(EntityType<?> type) {
        return kImitations.getOrDefault(type, SoundEvents.PARROT_AMBIENT);
    }
    public Variant getVariant() {
        return variant;
    }
    public <T> T get(DataComponentType<? extends T> type) {
        return super.get(type);
    }
    protected void applyImplicitComponents(DataComponentGetter components) {
        }
    protected <T> boolean applyImplicitComponent(DataComponentType<T> type, T value) {
        return false;
    }
    protected void defineSynchedData(SynchedEntityData.Builder entityData) {
    }
    protected void addAdditionalSaveData(ValueOutput output) {
    }
    protected void readAdditionalSaveData(ValueInput input) {
    }
    public boolean isFlying() {
        return !onGround();
    }
    protected boolean omnidirectionalAirMover() {
        return false;
    }
    protected boolean canFlyToOwner() {
        return false;
    }
    public Vec3 getLeashOffset() {
        return new net.minecraft.world.phys.Vec3(0.0, 0.5F * getEyeHeight(), getBbWidth() * 0.4F);
    }
    public static enum Variant implements StringRepresentable {
        RED_BLUE, BLUE, GREEN, YELLOW_BLUE, GRAY;
        public static final Variant DEFAULT = RED_BLUE;
        public static final Codec<Variant> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.animal.parrot.Parrot.Variant.class, "CODEC");
        public static final Codec<Variant> LEGACY_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.entity.animal.parrot.Parrot.Variant.class, "LEGACY_CODEC");
        public static final StreamCodec<ByteBuf, Variant> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.entity.animal.parrot.Parrot.Variant.class, "STREAM_CODEC");
        public int getId() {
            return ordinal();
        }
        public static Variant byId(int id) {
            return values()[Math.floorMod(id, values().length)];
        }
        public String getSerializedName() {
            return name().toLowerCase(java.util.Locale.ROOT);
        }
    }
    static class ParrotWanderGoal extends WaterAvoidingRandomFlyingGoal {
        public ParrotWanderGoal(PathfinderMob mob, double speedModifier) {
            super(mob, speedModifier);
        }
        protected Vec3 getPosition() {
            Vec3 pos = mob.isInWater() ? LandRandomPos.getPos(mob, 15, 15) : null;
            if (mob.getRandom().nextFloat() >= probability) {
                pos = treeSpot();
            }
            return pos == null ? super.getPosition() : pos;
        }
        private Vec3 treeSpot() {
            BlockPos here = mob.blockPosition();
            BlockPos.MutableBlockPos below = new BlockPos.MutableBlockPos();
            BlockPos.MutableBlockPos above = new BlockPos.MutableBlockPos();
            BlockPos low = new BlockPos(Mth.floor(mob.getX() - 3.0), Mth.floor(mob.getY() - 6.0), Mth.floor(mob.getZ() - 3.0));
            BlockPos high = new BlockPos(Mth.floor(mob.getX() + 3.0), Mth.floor(mob.getY() + 6.0), Mth.floor(mob.getZ() + 3.0));
            for (BlockPos pos : BlockPos.betweenClosed(low, high)) {
                if (here.equals(pos)) {
                    continue;
                }
                BlockState support = mob.level().getBlockState(below.setWithOffset(pos, Direction.DOWN));
                boolean perch = support.getBlock() instanceof LeavesBlock || support.is(BlockTags.LOGS);
                if (perch && mob.level().isEmptyBlock(pos) && mob.level().isEmptyBlock(above.setWithOffset(pos, Direction.UP))) {
                    return Vec3.atBottomCenterOf(pos);
                }
            }
            return null;
        }
    }
}
