// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.biome;

import com.mojang.serialization.JavaOps;
import java.util.Map;
import java.util.Optional;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.resources.Identifier;
import net.minecraft.world.attribute.EnvironmentAttribute;
import net.minecraft.world.attribute.EnvironmentAttributeMap;
import net.minecraft.world.attribute.modifier.AttributeModifier;

/** Reads the synced biome record (as the server registry holds it) into the typed objects of the facade. */
final class BiomeFacts {
    private BiomeFacts() {}

    static boolean flag(Object value, boolean fallback) {
        return value instanceof Number number ? number.intValue() != 0 : value instanceof Boolean bool ? bool : fallback;
    }

    static float number(Object value, float fallback) {
        return value instanceof Number number ? number.floatValue() : fallback;
    }

    static Optional<Integer> color(Object value) {
        if (value instanceof String text && text.length() == 7 && text.charAt(0) == '#') {
            return Optional.of(Integer.parseInt(text.substring(1), 16));
        }
        return value instanceof Number number ? Optional.of(number.intValue()) : Optional.empty();
    }

    @SuppressWarnings("unchecked")
    static BiomeSpecialEffects effects(Map<String, Object> facts) {
        Object found = facts.get("effects");
        Map<String, Object> effects = found instanceof Map<?, ?> map ? (Map<String, Object>) map : Map.of();
        String modifier = effects.get("grass_color_modifier") instanceof String text ? text : "none";
        BiomeSpecialEffects.GrassColorModifier grass = "dark_forest".equals(modifier) ? BiomeSpecialEffects.GrassColorModifier.DARK_FOREST
            : "swamp".equals(modifier) ? BiomeSpecialEffects.GrassColorModifier.SWAMP : BiomeSpecialEffects.GrassColorModifier.NONE;
        return new BiomeSpecialEffects(color(effects.get("water_color")).orElse(0x3F76E4), color(effects.get("foliage_color")),
            color(effects.get("dry_foliage_color")), color(effects.get("grass_color")), grass);
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    static EnvironmentAttributeMap attributes(Map<String, Object> facts) {
        if (!(facts.get("attributes") instanceof Map<?, ?> written)) {
            return EnvironmentAttributeMap.EMPTY;
        }
        EnvironmentAttributeMap.Builder builder = EnvironmentAttributeMap.builder();
        for (Map.Entry<?, ?> entry : written.entrySet()) {
            EnvironmentAttribute attribute = BuiltInRegistries.ENVIRONMENT_ATTRIBUTE.getValue(Identifier.parse((String) entry.getKey()));
            if (attribute == null) {
                continue;
            }
            try {
                Object value = entry.getValue();
                if (value instanceof Map<?, ?> modified && modified.get("modifier") instanceof String operation) {
                    AttributeModifier modifier = (AttributeModifier) attribute.type().modifierLibrary()
                        .get(AttributeModifier.OperationId.CODEC.parse(JavaOps.INSTANCE, operation).getOrThrow());
                    Object argument = modifier.argumentCodec(attribute).parse(JavaOps.INSTANCE, scalar(attribute, modified.get("argument"))).getOrThrow();
                    builder.modify(attribute, modifier, argument);
                } else {
                    builder.set(attribute, attribute.valueCodec().parse(JavaOps.INSTANCE, scalar(attribute, value)).getOrThrow());
                }
            } catch (RuntimeException skipped) {
                continue;
            }
        }
        return builder.build();
    }

    private static Object scalar(EnvironmentAttribute<?> attribute, Object value) {
        if (value instanceof Number number && attribute.defaultValue() instanceof Boolean) {
            return number.intValue() != 0;
        }
        return value instanceof Byte || value instanceof Short ? ((Number) value).intValue() : value;
    }
}
