// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.attribute;
import com.google.common.collect.Sets;
import java.util.Set;
import net.minecraft.util.ARGB;
import net.minecraft.world.attribute.modifier.ColorModifier;
import net.minecraft.world.attribute.modifier.FloatModifier;
import net.minecraft.world.attribute.modifier.FloatWithAlpha;
import net.minecraft.world.level.Level;
import net.minecraft.world.timeline.Timelines;
public class WeatherAttributes {
    public static final EnvironmentAttributeMap RAIN = weather(0.6F, 0.75F, 0.5F, 0.6F, 0.24F, 0.5F, 0.3125F);
    public static final EnvironmentAttributeMap THUNDER = weather(0.24F, 0.94F, 0.25F, 0.3F, 0.095F, 0.94F, 0.52734375F);
    private static final Set<EnvironmentAttribute<?>> kWeatherAttributes = Sets.union(RAIN.keySet(), THUNDER.keySet());
    private static EnvironmentAttributeMap weather(float skyBrightness, float skyFactor, float tint, float tintBlue, float cloudBrightness, float cloudFactor, float lightAlpha) {
        int fogTint = ARGB.colorFromFloat(1.0F, tint, tint, tintBlue);
        return EnvironmentAttributeMap.builder()
                .modify(EnvironmentAttributes.SKY_COLOR, ColorModifier.BLEND_TO_GRAY_RGB, new ColorModifier.BlendToGray(skyBrightness, skyFactor))
                .modify(EnvironmentAttributes.FOG_COLOR, ColorModifier.MULTIPLY_RGB, ARGB.vector3fFromRGB24(fogTint))
                .modify(EnvironmentAttributes.CLOUD_COLOR, ColorModifier.BLEND_TO_GRAY_ARGB, new ColorModifier.BlendToGray(cloudBrightness, cloudFactor))
                .modify(EnvironmentAttributes.SKY_LIGHT_LEVEL, FloatModifier.ALPHA_BLEND, new FloatWithAlpha(4.0F, lightAlpha))
                .modify(EnvironmentAttributes.SKY_LIGHT_COLOR, ColorModifier.ALPHA_BLEND_RGB, ARGB.vector4fFromARGB32(ARGB.color(lightAlpha, Timelines.NIGHT_SKY_LIGHT_COLOR)))
                .modify(EnvironmentAttributes.SKY_LIGHT_FACTOR, FloatModifier.ALPHA_BLEND, new FloatWithAlpha(0.24F, lightAlpha))
                .set(EnvironmentAttributes.STAR_BRIGHTNESS, 0.0F)
                .modify(EnvironmentAttributes.SUNRISE_SUNSET_COLOR, ColorModifier.MULTIPLY_ARGB, ARGB.vector4fFromARGB32(fogTint))
                .set(EnvironmentAttributes.BEES_STAY_IN_HIVE, true)
                .build();
    }
    public static void addBuiltinLayers(EnvironmentAttributeSystem.Builder system, WeatherAccess weatherAccess) {
        for (EnvironmentAttribute<?> attribute : kWeatherAttributes) {
            addLayer(system, weatherAccess, attribute);
        }
    }
    private static <Value> void addLayer(EnvironmentAttributeSystem.Builder system, WeatherAccess weatherAccess, EnvironmentAttribute<Value> attribute) {
        EnvironmentAttributeMap.Entry<Value, ?> rain = RAIN.get(attribute);
        EnvironmentAttributeMap.Entry<Value, ?> thunder = THUNDER.get(attribute);
        system.addTimeBasedLayer(attribute, (value, cacheTickId) -> {
            float thunderLevel = weatherAccess.thunderLevel();
            float rainLevel = weatherAccess.rainLevel() - thunderLevel;
            if (rain != null && rainLevel > 0.0F) {
                value = attribute.type().stateChangeLerp().apply(rainLevel, value, rain.applyModifier(value));
            }
            if (thunder != null && thunderLevel > 0.0F) {
                value = attribute.type().stateChangeLerp().apply(thunderLevel, value, thunder.applyModifier(value));
            }
            return value;
        });
    }
    public interface WeatherAccess {
        static WeatherAccess from(Level level) {
            return new WeatherAccess() {
                public float rainLevel() {
                    return level.getRainLevel(1.0F);
                }
                public float thunderLevel() {
                    return level.getThunderLevel(1.0F);
                }
            };
        }
        float rainLevel();
        float thunderLevel();
    }
}
