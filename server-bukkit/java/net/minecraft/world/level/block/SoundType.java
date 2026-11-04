// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.sounds.SoundEvent;
public class SoundType {
    public static final SoundType EMPTY = of("empty");
    public static final SoundType WOOD = of("wood");
    public static final SoundType GRAVEL = of("gravel");
    public static final SoundType GRASS = of("grass");
    public static final SoundType LILY_PAD = of("lily_pad");
    public static final SoundType STONE = of("stone");
    public static final SoundType METAL = of(1.0F, 1.5F, "metal");
    public static final SoundType GLASS = of("glass");
    public static final SoundType WOOL = of("wool");
    public static final SoundType SAND = of("sand");
    public static final SoundType SNOW = of("snow");
    public static final SoundType POWDER_SNOW = of("powder_snow");
    public static final SoundType LADDER = of("ladder");
    public static final SoundType ANVIL = of(0.3F, 1.0F, "anvil");
    public static final SoundType SLIME_BLOCK = of("slime_block");
    public static final SoundType HONEY_BLOCK = of("honey_block");
    public static final SoundType WET_GRASS = of("wet_grass");
    public static final SoundType CORAL_BLOCK = of("coral_block");
    public static final SoundType BAMBOO = of("bamboo");
    public static final SoundType BAMBOO_SAPLING = of("bamboo_sapling");
    public static final SoundType SCAFFOLDING = of("scaffolding");
    public static final SoundType SWEET_BERRY_BUSH = of("sweet_berry_bush");
    public static final SoundType CROP = of("crop");
    public static final SoundType HARD_CROP = of("hard_crop");
    public static final SoundType VINE = of("vine");
    public static final SoundType NETHER_WART = of("nether_wart");
    public static final SoundType LANTERN = of("lantern");
    public static final SoundType STEM = of("stem");
    public static final SoundType NYLIUM = of("nylium");
    public static final SoundType FUNGUS = of("fungus");
    public static final SoundType ROOTS = of("roots");
    public static final SoundType SHROOMLIGHT = of("shroomlight");
    public static final SoundType WEEPING_VINES = of("weeping_vines");
    public static final SoundType TWISTING_VINES = of(1.0F, 0.5F, "weeping_vines");
    public static final SoundType SOUL_SAND = of("soul_sand");
    public static final SoundType SOUL_SOIL = of("soul_soil");
    public static final SoundType BASALT = of("basalt");
    public static final SoundType WART_BLOCK = of("wart_block");
    public static final SoundType NETHERRACK = of("netherrack");
    public static final SoundType NETHER_BRICKS = of("nether_bricks");
    public static final SoundType NETHER_SPROUTS = of("nether_sprouts");
    public static final SoundType NETHER_ORE = of("nether_ore");
    public static final SoundType BONE_BLOCK = of("bone_block");
    public static final SoundType NETHERITE_BLOCK = of("netherite_block");
    public static final SoundType ANCIENT_DEBRIS = of("ancient_debris");
    public static final SoundType LODESTONE = of("lodestone");
    public static final SoundType CHAIN = of("chain");
    public static final SoundType NETHER_GOLD_ORE = of("nether_gold_ore");
    public static final SoundType GILDED_BLACKSTONE = of("gilded_blackstone");
    public static final SoundType CANDLE = of("candle");
    public static final SoundType AMETHYST = of("amethyst");
    public static final SoundType AMETHYST_CLUSTER = of("amethyst_cluster");
    public static final SoundType SMALL_AMETHYST_BUD = of("small_amethyst_bud");
    public static final SoundType MEDIUM_AMETHYST_BUD = of("medium_amethyst_bud");
    public static final SoundType LARGE_AMETHYST_BUD = of("large_amethyst_bud");
    public static final SoundType TUFF = of("tuff");
    public static final SoundType TUFF_BRICKS = of("tuff_bricks");
    public static final SoundType POLISHED_TUFF = of("polished_tuff");
    public static final SoundType CALCITE = of("calcite");
    public static final SoundType DRIPSTONE_BLOCK = of("dripstone_block");
    public static final SoundType POINTED_DRIPSTONE = of("pointed_dripstone");
    public static final SoundType COPPER = of("copper");
    public static final SoundType COPPER_BULB = of("copper_bulb");
    public static final SoundType COPPER_GRATE = of("copper_grate");
    public static final SoundType COPPER_GOLEM_STATUE = of("copper_golem_statue");
    public static final SoundType CAVE_VINES = of("cave_vines");
    public static final SoundType SPORE_BLOSSOM = of("spore_blossom");
    public static final SoundType CACTUS_FLOWER = of("cactus_flower");
    public static final SoundType AZALEA = of("azalea");
    public static final SoundType FLOWERING_AZALEA = of("flowering_azalea");
    public static final SoundType MOSS_CARPET = of("moss_carpet");
    public static final SoundType PINK_PETALS = of("pink_petals");
    public static final SoundType LEAF_LITTER = of("leaf_litter");
    public static final SoundType MOSS = of("moss");
    public static final SoundType BIG_DRIPLEAF = of("big_dripleaf");
    public static final SoundType SMALL_DRIPLEAF = of("small_dripleaf");
    public static final SoundType ROOTED_DIRT = of("rooted_dirt");
    public static final SoundType HANGING_ROOTS = of("hanging_roots");
    public static final SoundType AZALEA_LEAVES = of("azalea_leaves");
    public static final SoundType SCULK_SENSOR = of("sculk_sensor");
    public static final SoundType SCULK_CATALYST = of("sculk_catalyst");
    public static final SoundType SCULK = of("sculk");
    public static final SoundType SCULK_VEIN = of("sculk_vein");
    public static final SoundType SCULK_SHRIEKER = of("sculk_shrieker");
    public static final SoundType GLOW_LICHEN = of("glow_lichen");
    public static final SoundType DEEPSLATE = of("deepslate");
    public static final SoundType DEEPSLATE_BRICKS = of("deepslate_bricks");
    public static final SoundType DEEPSLATE_TILES = of("deepslate_tiles");
    public static final SoundType POLISHED_DEEPSLATE = of("polished_deepslate");
    public static final SoundType FROGLIGHT = of("froglight");
    public static final SoundType FROGSPAWN = of("frogspawn");
    public static final SoundType MANGROVE_ROOTS = of("mangrove_roots");
    public static final SoundType MUDDY_MANGROVE_ROOTS = of("muddy_mangrove_roots");
    public static final SoundType MUD = of("mud");
    public static final SoundType MUD_BRICKS = of("mud_bricks");
    public static final SoundType PACKED_MUD = of("packed_mud");
    public static final SoundType HANGING_SIGN = of("hanging_sign");
    public static final SoundType NETHER_WOOD_HANGING_SIGN = of("nether_wood_hanging_sign");
    public static final SoundType BAMBOO_WOOD_HANGING_SIGN = of("bamboo_wood_hanging_sign");
    public static final SoundType BAMBOO_WOOD = of("bamboo_wood");
    public static final SoundType NETHER_WOOD = of("nether_wood");
    public static final SoundType CHERRY_WOOD = of("cherry_wood");
    public static final SoundType CHERRY_SAPLING = of("cherry_sapling");
    public static final SoundType CHERRY_LEAVES = of("cherry_leaves");
    public static final SoundType CHERRY_WOOD_HANGING_SIGN = of("cherry_wood_hanging_sign");
    public static final SoundType CHISELED_BOOKSHELF = of("chiseled_bookshelf");
    public static final SoundType SHELF = of("shelf");
    public static final SoundType SUSPICIOUS_SAND = of("suspicious_sand");
    public static final SoundType SUSPICIOUS_GRAVEL = of("suspicious_gravel");
    public static final SoundType DECORATED_POT = of("decorated_pot");
    public static final SoundType DECORATED_POT_CRACKED = of("decorated_pot_cracked");
    public static final SoundType TRIAL_SPAWNER = of("trial_spawner");
    public static final SoundType SPONGE = of("sponge");
    public static final SoundType WET_SPONGE = of("wet_sponge");
    public static final SoundType VAULT = of("vault");
    public static final SoundType CREAKING_HEART = of("creaking_heart");
    public static final SoundType HEAVY_CORE = of("heavy_core");
    public static final SoundType COBWEB = of("cobweb");
    public static final SoundType SPAWNER = of("spawner");
    public static final SoundType RESIN = of("resin");
    public static final SoundType RESIN_BRICKS = of("resin_bricks");
    public static final SoundType IRON = of("iron");
    public static final SoundType DRIED_GHAST = of("dried_ghast");
    public static final SoundType SULFUR = of("sulfur");
    public static final SoundType POTENT_SULFUR = of("potent_sulfur");
    public static final SoundType SULFUR_SPIKE = of("sulfur_spike");
    public static final SoundType CINNABAR = of("cinnabar");
    public static final SoundType SHELF_MUSHROOM = of("shelf_mushroom");
    public static final SoundType POPLAR_LEAVES = of("poplar_leaves");
    public static final SoundType STRAW_BED = of("straw_bed");
    public static final SoundType RED_SHRUB = of("red_shrub");
    private static SoundType of(String name) {
        return of(1.0F, 1.0F, name);
    }

    private static SoundType of(float volume, float pitch, String name) {
        SoundEvent[] events = new SoundEvent[5];
        String[] kinds = {"break", "step", "place", "hit", "fall"};
        for (int i = 0; i < 5; i++) {
            events[i] = net.minecraft.sounds.SoundEvents.byId("block." + name + "." + kinds[i]);
        }
        if (events[0] == null) {
            return null;
        }
        return new SoundType(volume, pitch, events[0], events[1], events[2], events[3], events[4]);
    }

    public final float volume;
    public final float pitch;
    private final SoundEvent breakSound;
    private final SoundEvent stepSound;
    private final SoundEvent placeSound;
    private final SoundEvent hitSound;
    private final SoundEvent fallSound;

    public SoundType(float volume, float pitch, SoundEvent breakSound, SoundEvent stepSound, SoundEvent placeSound,
            SoundEvent hitSound, SoundEvent fallSound) {
        this.volume = volume;
        this.pitch = pitch;
        this.breakSound = breakSound;
        this.stepSound = stepSound;
        this.placeSound = placeSound;
        this.hitSound = hitSound;
        this.fallSound = fallSound;
    }

    public float getVolume() {
        return volume;
    }

    public float getPitch() {
        return pitch;
    }

    public SoundEvent getBreakSound() {
        return breakSound;
    }

    public SoundEvent getStepSound() {
        return stepSound;
    }

    public SoundEvent getPlaceSound() {
        return placeSound;
    }

    public SoundEvent getHitSound() {
        return hitSound;
    }

    public SoundEvent getFallSound() {
        return fallSound;
    }
}
