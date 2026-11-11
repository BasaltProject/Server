// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.heightproviders;
import com.mojang.datafixers.util.Either;
import com.mojang.serialization.Codec;
import com.mojang.serialization.DataResult;
import java.util.LinkedHashMap;
import java.util.Map;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.levelgen.VerticalAnchor;
import net.minecraft.world.level.levelgen.WorldGenerationContext;
public abstract class HeightProvider {
    private static final Map<String, HeightProviderType<?>> kTypes = new LinkedHashMap<>();
    private static final Codec<HeightProviderType<?>> kTypeCodec = Codec.STRING.comapFlatMap(HeightProvider::typeNamed, HeightProvider::nameOf);
    private static final Codec<HeightProvider> kTyped = kTypeCodec.dispatch(HeightProvider::getType, HeightProviderType::codec);
    public static final Codec<HeightProvider> CODEC = Codec.either(VerticalAnchor.CODEC, kTyped).xmap(
        either -> either.map(ConstantHeight::of, provider -> provider),
        provider -> provider.getType() == HeightProviderType.CONSTANT ? Either.left(((ConstantHeight) provider).getValue()) : Either.right(provider));
    private static synchronized DataResult<HeightProviderType<?>> typeNamed(String name) {
        if (kTypes.isEmpty()) {
            kTypes.put("constant", HeightProviderType.CONSTANT);
            kTypes.put("uniform", HeightProviderType.UNIFORM);
            kTypes.put("biased_to_bottom", HeightProviderType.BIASED_TO_BOTTOM);
            kTypes.put("very_biased_to_bottom", HeightProviderType.VERY_BIASED_TO_BOTTOM);
            kTypes.put("trapezoid", HeightProviderType.TRAPEZOID);
            kTypes.put("weighted_list", HeightProviderType.WEIGHTED_LIST);
        }
        HeightProviderType<?> found = kTypes.get(name.startsWith("minecraft:") ? name.substring("minecraft:".length()) : name);
        return found == null ? DataResult.error(() -> "Unknown height provider type: " + name) : DataResult.success(found);
    }
    private static String nameOf(HeightProviderType<?> type) {
        typeNamed("constant");
        for (Map.Entry<String, HeightProviderType<?>> entry : kTypes.entrySet()) {
            if (entry.getValue() == type) {
                return "minecraft:" + entry.getKey();
            }
        }
        throw new IllegalArgumentException("Unregistered height provider type");
    }
    public abstract int sample(RandomSource random, WorldGenerationContext heightAccessor);
    public abstract HeightProviderType<?> getType();
}
