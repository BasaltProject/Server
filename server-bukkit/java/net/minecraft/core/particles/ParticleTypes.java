// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.core.particles;
import com.mojang.serialization.Codec;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
public class ParticleTypes {
    private static final java.util.List<ParticleType<?>> kOrdered = new java.util.ArrayList<>();

    private static SimpleParticleType simple(String key) {
        SimpleParticleType type = new SimpleParticleType(false);
        type.named(key);
        kOrdered.add(type);
        return type;
    }

    @SuppressWarnings("unchecked")
    private static <T extends ParticleOptions> ParticleType<T> complex(String key) {
        ParticleType<T> type = new ParticleType<>(false) {
            @Override
            public com.mojang.serialization.MapCodec<T> codec() {
                throw new UnsupportedOperationException("Particle " + key + " has no codec");
            }

            @Override
            public net.minecraft.network.codec.StreamCodec<? super net.minecraft.network.RegistryFriendlyByteBuf, T> streamCodec() {
                return (net.minecraft.network.codec.StreamCodec<? super net.minecraft.network.RegistryFriendlyByteBuf, T>) ParticleStreams.of(this);
            }
        };
        type.named(key);
        kOrdered.add(type);
        return type;
    }

    public static final SimpleParticleType ANGRY_VILLAGER = simple("angry_villager");
    public static final ParticleType<BlockParticleOption> BLOCK = complex("block");
    public static final ParticleType<BlockParticleOption> BLOCK_MARKER = complex("block_marker");
    public static final SimpleParticleType BUBBLE = simple("bubble");
    public static final SimpleParticleType SULFUR_BUBBLES = simple("sulfur_bubbles");
    public static final SimpleParticleType NOXIOUS_GAS = simple("noxious_gas");
    public static final SimpleParticleType NOXIOUS_GAS_CLOUD = simple("noxious_gas_cloud");
    public static final ParticleType<GeyserParticleOptions> GEYSER = complex("geyser");
    public static final ParticleType<GeyserBaseParticleOptions> GEYSER_BASE = complex("geyser_base");
    public static final ParticleType<GeyserBaseParticleOptions> GEYSER_POOF = complex("geyser_poof");
    public static final ParticleType<GeyserParticleOptions> GEYSER_PLUME = complex("geyser_plume");
    public static final SimpleParticleType CLOUD = simple("cloud");
    public static final SimpleParticleType COPPER_FIRE_FLAME = simple("copper_fire_flame");
    public static final SimpleParticleType CRIT = simple("crit");
    public static final SimpleParticleType DAMAGE_INDICATOR = simple("damage_indicator");
    public static final ParticleType<PowerParticleOption> DRAGON_BREATH = complex("dragon_breath");
    public static final SimpleParticleType DRIPPING_LAVA = simple("dripping_lava");
    public static final SimpleParticleType FALLING_LAVA = simple("falling_lava");
    public static final SimpleParticleType LANDING_LAVA = simple("landing_lava");
    public static final SimpleParticleType DRIPPING_WATER = simple("dripping_water");
    public static final SimpleParticleType FALLING_WATER = simple("falling_water");
    public static final ParticleType<DustParticleOptions> DUST = complex("dust");
    public static final ParticleType<DustColorTransitionOptions> DUST_COLOR_TRANSITION = complex("dust_color_transition");
    public static final ParticleType<SpellParticleOption> EFFECT = complex("effect");
    public static final SimpleParticleType ELDER_GUARDIAN = simple("elder_guardian");
    public static final SimpleParticleType ENCHANTED_HIT = simple("enchanted_hit");
    public static final SimpleParticleType ENCHANT = simple("enchant");
    public static final SimpleParticleType END_ROD = simple("end_rod");
    public static final ParticleType<ColorParticleOption> ENTITY_EFFECT = complex("entity_effect");
    public static final SimpleParticleType EXPLOSION_EMITTER = simple("explosion_emitter");
    public static final SimpleParticleType EXPLOSION = simple("explosion");
    public static final SimpleParticleType GUST = simple("gust");
    public static final SimpleParticleType SMALL_GUST = simple("small_gust");
    public static final SimpleParticleType GUST_EMITTER_LARGE = simple("gust_emitter_large");
    public static final SimpleParticleType GUST_EMITTER_SMALL = simple("gust_emitter_small");
    public static final SimpleParticleType SONIC_BOOM = simple("sonic_boom");
    public static final ParticleType<BlockParticleOption> FALLING_DUST = complex("falling_dust");
    public static final SimpleParticleType FIREWORK = simple("firework");
    public static final SimpleParticleType FISHING = simple("fishing");
    public static final SimpleParticleType FLAME = simple("flame");
    public static final SimpleParticleType INFESTED = simple("infested");
    public static final SimpleParticleType CHERRY_LEAVES = simple("cherry_leaves");
    public static final SimpleParticleType PALE_OAK_LEAVES = simple("pale_oak_leaves");
    public static final SimpleParticleType RED_POPLAR_LEAVES = simple("red_poplar_leaves");
    public static final SimpleParticleType ORANGE_POPLAR_LEAVES = simple("orange_poplar_leaves");
    public static final SimpleParticleType YELLOW_POPLAR_LEAVES = simple("yellow_poplar_leaves");
    public static final ParticleType<ColorParticleOption> TINTED_LEAVES = complex("tinted_leaves");
    public static final SimpleParticleType SCULK_SOUL = simple("sculk_soul");
    public static final ParticleType<SculkChargeParticleOptions> SCULK_CHARGE = complex("sculk_charge");
    public static final SimpleParticleType SCULK_CHARGE_POP = simple("sculk_charge_pop");
    public static final SimpleParticleType SOUL_FIRE_FLAME = simple("soul_fire_flame");
    public static final SimpleParticleType SOUL = simple("soul");
    public static final ParticleType<ColorParticleOption> FLASH = complex("flash");
    public static final SimpleParticleType HAPPY_VILLAGER = simple("happy_villager");
    public static final SimpleParticleType COMPOSTER = simple("composter");
    public static final SimpleParticleType HEART = simple("heart");
    public static final ParticleType<SpellParticleOption> INSTANT_EFFECT = complex("instant_effect");
    public static final ParticleType<ItemParticleOption> ITEM = complex("item");
    public static final ParticleType<VibrationParticleOption> VIBRATION = complex("vibration");
    public static final ParticleType<TrailParticleOption> TRAIL = complex("trail");
    public static final SimpleParticleType PAUSE_MOB_GROWTH = simple("pause_mob_growth");
    public static final SimpleParticleType RESET_MOB_GROWTH = simple("reset_mob_growth");
    public static final SimpleParticleType ITEM_SLIME = simple("item_slime");
    public static final SimpleParticleType ITEM_COBWEB = simple("item_cobweb");
    public static final SimpleParticleType ITEM_SNOWBALL = simple("item_snowball");
    public static final SimpleParticleType LARGE_SMOKE = simple("large_smoke");
    public static final SimpleParticleType LAVA = simple("lava");
    public static final SimpleParticleType MYCELIUM = simple("mycelium");
    public static final SimpleParticleType NOTE = simple("note");
    public static final SimpleParticleType POOF = simple("poof");
    public static final SimpleParticleType PORTAL = simple("portal");
    public static final SimpleParticleType RAIN = simple("rain");
    public static final SimpleParticleType SMOKE = simple("smoke");
    public static final SimpleParticleType WHITE_SMOKE = simple("white_smoke");
    public static final SimpleParticleType SNEEZE = simple("sneeze");
    public static final SimpleParticleType SPIT = simple("spit");
    public static final SimpleParticleType SQUID_INK = simple("squid_ink");
    public static final SimpleParticleType SWEEP_ATTACK = simple("sweep_attack");
    public static final SimpleParticleType TOTEM_OF_UNDYING = simple("totem_of_undying");
    public static final SimpleParticleType UNDERWATER = simple("underwater");
    public static final SimpleParticleType SPLASH = simple("splash");
    public static final SimpleParticleType WITCH = simple("witch");
    public static final SimpleParticleType BUBBLE_POP = simple("bubble_pop");
    public static final SimpleParticleType CURRENT_DOWN = simple("current_down");
    public static final SimpleParticleType BUBBLE_COLUMN_UP = simple("bubble_column_up");
    public static final SimpleParticleType NAUTILUS = simple("nautilus");
    public static final SimpleParticleType DOLPHIN = simple("dolphin");
    public static final SimpleParticleType CAMPFIRE_COSY_SMOKE = simple("campfire_cosy_smoke");
    public static final SimpleParticleType CAMPFIRE_SIGNAL_SMOKE = simple("campfire_signal_smoke");
    public static final SimpleParticleType DRIPPING_HONEY = simple("dripping_honey");
    public static final SimpleParticleType FALLING_HONEY = simple("falling_honey");
    public static final SimpleParticleType LANDING_HONEY = simple("landing_honey");
    public static final SimpleParticleType FALLING_NECTAR = simple("falling_nectar");
    public static final SimpleParticleType FALLING_SPORE_BLOSSOM = simple("falling_spore_blossom");
    public static final SimpleParticleType ASH = simple("ash");
    public static final SimpleParticleType CRIMSON_SPORE = simple("crimson_spore");
    public static final SimpleParticleType WARPED_SPORE = simple("warped_spore");
    public static final SimpleParticleType SPORE_BLOSSOM_AIR = simple("spore_blossom_air");
    public static final SimpleParticleType DRIPPING_OBSIDIAN_TEAR = simple("dripping_obsidian_tear");
    public static final SimpleParticleType FALLING_OBSIDIAN_TEAR = simple("falling_obsidian_tear");
    public static final SimpleParticleType LANDING_OBSIDIAN_TEAR = simple("landing_obsidian_tear");
    public static final SimpleParticleType REVERSE_PORTAL = simple("reverse_portal");
    public static final SimpleParticleType WHITE_ASH = simple("white_ash");
    public static final SimpleParticleType SMALL_FLAME = simple("small_flame");
    public static final SimpleParticleType SNOWFLAKE = simple("snowflake");
    public static final SimpleParticleType DRIPPING_DRIPSTONE_LAVA = simple("dripping_dripstone_lava");
    public static final SimpleParticleType FALLING_DRIPSTONE_LAVA = simple("falling_dripstone_lava");
    public static final SimpleParticleType DRIPPING_DRIPSTONE_WATER = simple("dripping_dripstone_water");
    public static final SimpleParticleType FALLING_DRIPSTONE_WATER = simple("falling_dripstone_water");
    public static final SimpleParticleType GLOW_SQUID_INK = simple("glow_squid_ink");
    public static final SimpleParticleType GLOW = simple("glow");
    public static final SimpleParticleType WAX_ON = simple("wax_on");
    public static final SimpleParticleType WAX_OFF = simple("wax_off");
    public static final SimpleParticleType ELECTRIC_SPARK = simple("electric_spark");
    public static final SimpleParticleType SCRAPE = simple("scrape");
    public static final ParticleType<ShriekParticleOption> SHRIEK = complex("shriek");
    public static final SimpleParticleType EGG_CRACK = simple("egg_crack");
    public static final SimpleParticleType DUST_PLUME = simple("dust_plume");
    public static final SimpleParticleType TRIAL_SPAWNER_DETECTED_PLAYER = simple("trial_spawner_detected_player");
    public static final SimpleParticleType TRIAL_SPAWNER_DETECTED_PLAYER_OMINOUS = simple("trial_spawner_detected_player_ominous");
    public static final SimpleParticleType VAULT_CONNECTION = simple("vault_connection");
    public static final ParticleType<BlockParticleOption> DUST_PILLAR = complex("dust_pillar");
    public static final SimpleParticleType OMINOUS_SPAWNING = simple("ominous_spawning");
    public static final SimpleParticleType RAID_OMEN = simple("raid_omen");
    public static final SimpleParticleType TRIAL_OMEN = simple("trial_omen");
    public static final ParticleType<BlockParticleOption> BLOCK_CRUMBLE = complex("block_crumble");
    public static final SimpleParticleType FIREFLY = simple("firefly");
    public static final SimpleParticleType SULFUR_CUBE_GOO = simple("sulfur_cube_goo");
    public static final Codec<ParticleOptions> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.core.particles.ParticleTypes.class, "CODEC");
    public static int idOf(ParticleType<?> type) {
        return kOrdered.indexOf(type);
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    public static final StreamCodec<RegistryFriendlyByteBuf, ParticleOptions> STREAM_CODEC = StreamCodec.of(
        (buf, options) -> {
            int id = idOf(options.getType());
            if (id < 0) {
                throw new IllegalArgumentException("Unknown particle type " + options.getType());
            }
            buf.writeVarInt(id);
            ((StreamCodec) options.getType().streamCodec()).encode(buf, options);
        },
        buf -> {
            ParticleType<?> type = kOrdered.get(buf.readVarInt());
            return (ParticleOptions) ((StreamCodec) type.streamCodec()).decode(buf);
        });
}
