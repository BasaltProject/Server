// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.datafix.fixes;
import com.mojang.datafixers.DataFix;
import com.mojang.datafixers.TypeRewriteRule;
import com.mojang.datafixers.schemas.Schema;
import com.mojang.serialization.Codec;
import com.mojang.serialization.Dynamic;
import com.mojang.serialization.DynamicOps;
public class WorldGenSettingsFix extends DataFix {
    public WorldGenSettingsFix(Schema parent) {
        super(parent, true);
    }
    protected TypeRewriteRule makeRule() {
        return org.basaltmc.bukkit.facade.NativeStepRule.of(getClass().getSimpleName(), getVersionKey());
    }
    private static <T> Dynamic<T> map(Dynamic<T> like, Object... pairs) {
        java.util.Map<Dynamic<T>, Dynamic<T>> entries = new java.util.LinkedHashMap<>();
        for (int i = 0; i < pairs.length; i += 2) {
            entries.put(like.createString((String) pairs[i]), (Dynamic<T>) pairs[i + 1]);
        }
        return like.createMap(entries);
    }
    private static <T> Dynamic<T> noise(long seed, Dynamic<T> like, String settings, Dynamic<T> biomeSource) {
        return map(like, "type", like.createString("minecraft:noise"), "biome_source", biomeSource, "seed", like.createLong(seed), "settings", like.createString(settings));
    }
    protected static <T> Dynamic<T> defaultOverworld(Dynamic<T> input, long seed) {
        Dynamic<T> source = map(input, "type", input.createString("minecraft:vanilla_layered"), "seed", input.createLong(seed), "large_biomes", input.createBoolean(false));
        return noise(seed, input, "minecraft:overworld", source);
    }
    protected static <T> T vanillaLevels(Dynamic<T> input, long seed, Dynamic<T> overworldGenerator, boolean caves) {
        Dynamic<T> overworld = map(input, "type", input.createString(caves ? "minecraft:overworld_caves" : "minecraft:overworld"), "generator", overworldGenerator);
        Dynamic<T> nether = map(input, "type", input.createString("minecraft:the_nether"), "generator", noise(seed, input, "minecraft:nether",
            map(input, "type", input.createString("minecraft:multi_noise"), "seed", input.createLong(seed), "preset", input.createString("minecraft:nether"))));
        Dynamic<T> end = map(input, "type", input.createString("minecraft:the_end"), "generator", noise(seed, input, "minecraft:end",
            map(input, "type", input.createString("minecraft:the_end"), "seed", input.createLong(seed))));
        return map(input, "minecraft:overworld", overworld, "minecraft:the_nether", nether, "minecraft:the_end", end).getValue();
    }
    static final class StructureFeatureConfiguration {
        public static final Codec<StructureFeatureConfiguration> CODEC = com.mojang.serialization.codecs.RecordCodecBuilder.create(i -> i.group(
            Codec.INT.fieldOf("spacing").forGetter(c -> c.spacing), Codec.INT.fieldOf("separation").forGetter(c -> c.separation),
            Codec.INT.fieldOf("salt").forGetter(c -> c.salt)).apply(i, StructureFeatureConfiguration::new));
        private final int spacing;
        private final int separation;
        private final int salt;
        public StructureFeatureConfiguration(int spacing, int separation, int salt) {
            this.spacing = spacing;
            this.separation = separation;
            this.salt = salt;
        }
        public <T> Dynamic<T> serialize(DynamicOps<T> ops) {
            return new Dynamic<T>(ops, CODEC.encodeStart(ops, this).result().orElse(ops.emptyMap()));
        }
    }
}
