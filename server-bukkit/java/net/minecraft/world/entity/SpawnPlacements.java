// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;
import java.util.Map;
import net.minecraft.core.BlockPos;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.util.RandomSource;
import net.minecraft.world.Difficulty;
import net.minecraft.world.entity.ambient.Bat;
import net.minecraft.world.entity.animal.AgeableWaterCreature;
import net.minecraft.world.entity.animal.Animal;
import net.minecraft.world.entity.animal.armadillo.Armadillo;
import net.minecraft.world.entity.animal.axolotl.Axolotl;
import net.minecraft.world.entity.animal.camel.Camel;
import net.minecraft.world.entity.animal.cow.MushroomCow;
import net.minecraft.world.entity.animal.equine.SkeletonHorse;
import net.minecraft.world.entity.animal.feline.Ocelot;
import net.minecraft.world.entity.animal.fish.TropicalFish;
import net.minecraft.world.entity.animal.fish.WaterAnimal;
import net.minecraft.world.entity.animal.fox.Fox;
import net.minecraft.world.entity.animal.frog.Frog;
import net.minecraft.world.entity.animal.goat.Goat;
import net.minecraft.world.entity.animal.nautilus.AbstractNautilus;
import net.minecraft.world.entity.animal.parrot.Parrot;
import net.minecraft.world.entity.animal.polarbear.PolarBear;
import net.minecraft.world.entity.animal.rabbit.Rabbit;
import net.minecraft.world.entity.animal.squid.GlowSquid;
import net.minecraft.world.entity.animal.turtle.Turtle;
import net.minecraft.world.entity.animal.wolf.Wolf;
import net.minecraft.world.entity.monster.Endermite;
import net.minecraft.world.entity.monster.Ghast;
import net.minecraft.world.entity.monster.Guardian;
import net.minecraft.world.entity.monster.Monster;
import net.minecraft.world.entity.monster.PatrollingMonster;
import net.minecraft.world.entity.monster.Silverfish;
import net.minecraft.world.entity.monster.Strider;
import net.minecraft.world.entity.monster.cubemob.MagmaCube;
import net.minecraft.world.entity.monster.cubemob.Slime;
import net.minecraft.world.entity.monster.cubemob.SulfurCube;
import net.minecraft.world.entity.monster.hoglin.Hoglin;
import net.minecraft.world.entity.monster.piglin.Piglin;
import net.minecraft.world.entity.monster.skeleton.Stray;
import net.minecraft.world.entity.monster.zombie.Drowned;
import net.minecraft.world.entity.monster.zombie.ZombifiedPiglin;
import net.minecraft.world.level.LevelReader;
import net.minecraft.world.level.ServerLevelAccessor;
import net.minecraft.world.level.levelgen.Heightmap;
import org.jspecify.annotations.Nullable;
public class SpawnPlacements {
    private static final java.util.Map<EntityType<?>, Data> DATA_BY_TYPE = new java.util.HashMap<>();
    private static <T extends Mob> void bind(EntityType<T> type, Heightmap.Types heightmap, SpawnPlacementType placementType, SpawnPredicate<T> spawnPredicate) {
        if (DATA_BY_TYPE.put(type, new Data(heightmap, placementType, spawnPredicate)) != null) {
            throw new IllegalStateException("Duplicate registration for type " + BuiltInRegistries.ENTITY_TYPE.getKey(type));
        }
    }
    static {
        bind(EntityTypes.AXOLOTL, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.IN_WATER, Axolotl::checkAxolotlSpawnRules);
        bind(EntityTypes.COD, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.IN_WATER, WaterAnimal::checkSurfaceWaterAnimalSpawnRules);
        bind(EntityTypes.DOLPHIN, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.IN_WATER, AgeableWaterCreature::checkSurfaceAgeableWaterCreatureSpawnRules);
        bind(EntityTypes.DROWNED, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.IN_WATER, Drowned::checkDrownedSpawnRules);
        bind(EntityTypes.GUARDIAN, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.IN_WATER, Guardian::checkGuardianSpawnRules);
        bind(EntityTypes.PUFFERFISH, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.IN_WATER, WaterAnimal::checkSurfaceWaterAnimalSpawnRules);
        bind(EntityTypes.SALMON, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.IN_WATER, WaterAnimal::checkSurfaceWaterAnimalSpawnRules);
        bind(EntityTypes.SQUID, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.IN_WATER, AgeableWaterCreature::checkSurfaceAgeableWaterCreatureSpawnRules);
        bind(EntityTypes.TROPICAL_FISH, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.IN_WATER, TropicalFish::checkTropicalFishSpawnRules);
        bind(EntityTypes.ARMADILLO, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, Armadillo::checkArmadilloSpawnRules);
        bind(EntityTypes.BAT, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, Bat::checkBatSpawnRules);
        bind(EntityTypes.BLAZE, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, Monster::checkAnyLightMonsterSpawnRules);
        bind(EntityTypes.BOGGED, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, Monster::checkMonsterSpawnRules);
        bind(EntityTypes.BREEZE, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, Monster::checkAnyLightMonsterSpawnRules);
        bind(EntityTypes.CAMEL, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, Camel::checkCamelSpawnRules);
        bind(EntityTypes.CAMEL_HUSK, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, Monster::checkSurfaceMonstersSpawnRules);
        bind(EntityTypes.CAVE_SPIDER, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, Monster::checkMonsterSpawnRules);
        bind(EntityTypes.CHICKEN, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, Animal::checkAnimalSpawnRules);
        bind(EntityTypes.COW, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, Animal::checkAnimalSpawnRules);
        bind(EntityTypes.CREEPER, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, Monster::checkMonsterSpawnRules);
        bind(EntityTypes.DONKEY, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, Animal::checkAnimalSpawnRules);
        bind(EntityTypes.ENDERMAN, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, Monster::checkMonsterSpawnRules);
        bind(EntityTypes.ENDERMITE, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, Endermite::checkEndermiteSpawnRules);
        bind(EntityTypes.ENDER_DRAGON, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, Mob::checkMobSpawnRules);
        bind(EntityTypes.FROG, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, Frog::checkFrogSpawnRules);
        bind(EntityTypes.GHAST, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, Ghast::checkGhastSpawnRules);
        bind(EntityTypes.HAPPY_GHAST, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, Animal::checkAnimalSpawnRules);
        bind(EntityTypes.GIANT, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, Monster::checkMonsterSpawnRules);
        bind(EntityTypes.GLOW_SQUID, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.IN_WATER, GlowSquid::checkGlowSquidSpawnRules);
        bind(EntityTypes.GOAT, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, Goat::checkGoatSpawnRules);
        bind(EntityTypes.HORSE, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, Animal::checkAnimalSpawnRules);
        bind(EntityTypes.HUSK, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, Monster::checkSurfaceMonstersSpawnRules);
        bind(EntityTypes.IRON_GOLEM, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, Mob::checkMobSpawnRules);
        bind(EntityTypes.LLAMA, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, Animal::checkAnimalSpawnRules);
        bind(EntityTypes.MAGMA_CUBE, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, MagmaCube::checkMagmaCubeSpawnRules);
        bind(EntityTypes.SULFUR_CUBE, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, SulfurCube::checkSulfurCubeSpawnRules);
        bind(EntityTypes.MOOSHROOM, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, MushroomCow::checkMushroomSpawnRules);
        bind(EntityTypes.MULE, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, Animal::checkAnimalSpawnRules);
        bind(EntityTypes.NAUTILUS, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.IN_WATER, AbstractNautilus::checkNautilusSpawnRules);
        bind(EntityTypes.OCELOT, Heightmap.Types.MOTION_BLOCKING, SpawnPlacementTypes.ON_GROUND, Ocelot::checkOcelotSpawnRules);
        bind(EntityTypes.PARROT, Heightmap.Types.MOTION_BLOCKING, SpawnPlacementTypes.ON_GROUND, Parrot::checkParrotSpawnRules);
        bind(EntityTypes.PIG, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, Animal::checkAnimalSpawnRules);
        bind(EntityTypes.HOGLIN, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, Hoglin::checkHoglinSpawnRules);
        bind(EntityTypes.PIGLIN, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, Piglin::checkPiglinSpawnRules);
        bind(EntityTypes.PILLAGER, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, PatrollingMonster::checkPatrollingMonsterSpawnRules);
        bind(EntityTypes.POLAR_BEAR, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, PolarBear::checkPolarBearSpawnRules);
        bind(EntityTypes.RABBIT, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, Rabbit::checkRabbitSpawnRules);
        bind(EntityTypes.SHEEP, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, Animal::checkAnimalSpawnRules);
        bind(EntityTypes.SILVERFISH, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, Silverfish::checkSilverfishSpawnRules);
        bind(EntityTypes.SKELETON, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, Monster::checkMonsterSpawnRules);
        bind(EntityTypes.SKELETON_HORSE, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, SkeletonHorse::checkSkeletonHorseSpawnRules);
        bind(EntityTypes.SLIME, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, Slime::checkSlimeSpawnRules);
        bind(EntityTypes.SNOW_GOLEM, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, Mob::checkMobSpawnRules);
        bind(EntityTypes.SPIDER, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, Monster::checkMonsterSpawnRules);
        bind(EntityTypes.STRAY, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, Stray::checkStraySpawnRules);
        bind(EntityTypes.PARCHED, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, Monster::checkSurfaceMonstersSpawnRules);
        bind(EntityTypes.STRIDER, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.IN_LAVA, Strider::checkStriderSpawnRules);
        bind(EntityTypes.TURTLE, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, Turtle::checkTurtleSpawnRules);
        bind(EntityTypes.VILLAGER, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, Mob::checkMobSpawnRules);
        bind(EntityTypes.WITCH, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, Monster::checkMonsterSpawnRules);
        bind(EntityTypes.WITHER, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, Monster::checkMonsterSpawnRules);
        bind(EntityTypes.WITHER_SKELETON, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, Monster::checkMonsterSpawnRules);
        bind(EntityTypes.WOLF, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, Wolf::checkWolfSpawnRules);
        bind(EntityTypes.ZOGLIN, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, Monster::checkAnyLightMonsterSpawnRules);
        bind(EntityTypes.CREAKING, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, Monster::checkMonsterSpawnRules);
        bind(EntityTypes.ZOMBIE, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, Monster::checkMonsterSpawnRules);
        bind(EntityTypes.ZOMBIE_HORSE, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, Monster::checkMonsterSpawnRules);
        bind(EntityTypes.ZOMBIFIED_PIGLIN, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, ZombifiedPiglin::checkZombifiedPiglinSpawnRules);
        bind(EntityTypes.ZOMBIE_VILLAGER, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, Monster::checkMonsterSpawnRules);
        bind(EntityTypes.CAT, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, Animal::checkAnimalSpawnRules);
        bind(EntityTypes.ELDER_GUARDIAN, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.IN_WATER, Guardian::checkGuardianSpawnRules);
        bind(EntityTypes.EVOKER, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.NO_RESTRICTIONS, Monster::checkMonsterSpawnRules);
        bind(EntityTypes.FOX, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.NO_RESTRICTIONS, Fox::checkFoxSpawnRules);
        bind(EntityTypes.ILLUSIONER, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.NO_RESTRICTIONS, Monster::checkMonsterSpawnRules);
        bind(EntityTypes.PANDA, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.NO_RESTRICTIONS, Animal::checkAnimalSpawnRules);
        bind(EntityTypes.PHANTOM, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.NO_RESTRICTIONS, Mob::checkMobSpawnRules);
        bind(EntityTypes.RAVAGER, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, Monster::checkMonsterSpawnRules);
        bind(EntityTypes.SHULKER, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.NO_RESTRICTIONS, Mob::checkMobSpawnRules);
        bind(EntityTypes.TRADER_LLAMA, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.NO_RESTRICTIONS, Animal::checkAnimalSpawnRules);
        bind(EntityTypes.VEX, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.NO_RESTRICTIONS, Monster::checkMonsterSpawnRules);
        bind(EntityTypes.VINDICATOR, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.NO_RESTRICTIONS, Monster::checkMonsterSpawnRules);
        bind(EntityTypes.WANDERING_TRADER, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.ON_GROUND, Mob::checkMobSpawnRules);
        bind(EntityTypes.WARDEN, Heightmap.Types.MOTION_BLOCKING_NO_LEAVES, SpawnPlacementTypes.NO_RESTRICTIONS, Monster::checkMonsterSpawnRules);
    }
    public static SpawnPlacementType getPlacementType(EntityType<?> type) {
        Data data = DATA_BY_TYPE.get(type);
        return data == null ? SpawnPlacementTypes.NO_RESTRICTIONS : data.placement();
    }
    public static boolean isSpawnPositionOk(EntityType<?> type, LevelReader level, BlockPos blockPos) {
        return getPlacementType(type).isSpawnPositionOk(level, blockPos, type);
    }
    public static Heightmap.Types getHeightmapType(EntityType<?> type) {
        Data data = DATA_BY_TYPE.get(type);
        return data == null ? Heightmap.Types.MOTION_BLOCKING_NO_LEAVES : data.heightMap();
    }
    public static <T extends Entity> boolean checkSpawnRules(EntityType<T> type, ServerLevelAccessor level, EntitySpawnReason spawnReason, BlockPos pos, RandomSource random) {
        if (!type.isAllowedInPeaceful() && level.getDifficulty() == Difficulty.PEACEFUL) {
            return false;
        }
        Data data = DATA_BY_TYPE.get(type);
        return data == null || ((SpawnPredicate<T>) data.predicate()).test(type, level, spawnReason, pos, random);


    }
    static record Data(Heightmap.Types heightMap, SpawnPlacementType placement, SpawnPredicate<?> predicate) {
    }
    public interface SpawnPredicate<T extends Entity> {
        boolean test(EntityType<T> type, ServerLevelAccessor level, EntitySpawnReason spawnReason, BlockPos pos, RandomSource random);
    }
}
