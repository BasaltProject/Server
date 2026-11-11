// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block.entity;
import net.minecraft.core.registries.Registries;
import net.minecraft.resources.Identifier;
import com.mojang.serialization.Codec;
import net.minecraft.core.Holder;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.resources.ResourceKey;
public class DecoratedPotPatterns {
    private static ResourceKey<DecoratedPotPattern> create(String id) {
        return ResourceKey.create(Registries.DECORATED_POT_PATTERN, Identifier.withDefaultNamespace(id));
    }
    private static void registerWithDefaultAsset(BootstrapContext<DecoratedPotPattern> registry, ResourceKey<DecoratedPotPattern> key) {
        registry.register(key, new DecoratedPotPattern(key.identifier().withSuffix("_pottery_pattern")));
    }
    public static final ResourceKey<DecoratedPotPattern> ANGLER = create("angler");
    public static final ResourceKey<DecoratedPotPattern> ARCHER = create("archer");
    public static final ResourceKey<DecoratedPotPattern> ARMS_UP = create("arms_up");
    public static final ResourceKey<DecoratedPotPattern> BLADE = create("blade");
    public static final ResourceKey<DecoratedPotPattern> BREWER = create("brewer");
    public static final ResourceKey<DecoratedPotPattern> BURN = create("burn");
    public static final ResourceKey<DecoratedPotPattern> DANGER = create("danger");
    public static final ResourceKey<DecoratedPotPattern> EXPLORER = create("explorer");
    public static final ResourceKey<DecoratedPotPattern> FLOW = create("flow");
    public static final ResourceKey<DecoratedPotPattern> FRIEND = create("friend");
    public static final ResourceKey<DecoratedPotPattern> GUSTER = create("guster");
    public static final ResourceKey<DecoratedPotPattern> HEART = create("heart");
    public static final ResourceKey<DecoratedPotPattern> HEARTBREAK = create("heartbreak");
    public static final ResourceKey<DecoratedPotPattern> HOWL = create("howl");
    public static final ResourceKey<DecoratedPotPattern> MINER = create("miner");
    public static final ResourceKey<DecoratedPotPattern> MOURNER = create("mourner");
    public static final ResourceKey<DecoratedPotPattern> PLENTY = create("plenty");
    public static final ResourceKey<DecoratedPotPattern> PRIZE = create("prize");
    public static final ResourceKey<DecoratedPotPattern> SCRAPE = create("scrape");
    public static final ResourceKey<DecoratedPotPattern> SHEAF = create("sheaf");
    public static final ResourceKey<DecoratedPotPattern> SHELTER = create("shelter");
    public static final ResourceKey<DecoratedPotPattern> SKULL = create("skull");
    public static final ResourceKey<DecoratedPotPattern> SNORT = create("snort");
    public static final Codec<Holder<DecoratedPotPattern>> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.codec(net.minecraft.world.level.block.entity.DecoratedPotPatterns.class, "CODEC");
    public static final StreamCodec<RegistryFriendlyByteBuf, Holder<DecoratedPotPattern>> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.level.block.entity.DecoratedPotPatterns.class, "STREAM_CODEC");
    public static void bootstrap(BootstrapContext<DecoratedPotPattern> registry) {
        for (ResourceKey<DecoratedPotPattern> key : java.util.List.of(
            ANGLER, ARCHER, ARMS_UP, BLADE, BREWER, BURN, DANGER, EXPLORER,
            FLOW, FRIEND, GUSTER, HEART, HEARTBREAK, HOWL, MINER, MOURNER,
            PLENTY, PRIZE, SCRAPE, SHEAF, SHELTER, SKULL, SNORT)) {
            registerWithDefaultAsset(registry, key);
        }
    }
}
