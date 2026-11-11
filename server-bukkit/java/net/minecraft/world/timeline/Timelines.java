// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.timeline;
import net.minecraft.core.registries.Registries;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.util.ARGB;
public interface Timelines {
    ResourceKey<Timeline> OVERWORLD_DAY = named("day");
    ResourceKey<Timeline> MOON = named("moon");
    ResourceKey<Timeline> VILLAGER_SCHEDULE = named("villager_schedule");
    ResourceKey<Timeline> EARLY_GAME = named("early_game");
    float DAY_SKY_LIGHT_LEVEL = 15.0F;
    float NIGHT_SKY_LIGHT_LEVEL = 4.0F;
    int NIGHT_SKY_LIGHT_COLOR = ARGB.colorFromFloat(1.0F, 0.48F, 0.48F, 1.0F);
    float NIGHT_SKY_LIGHT_FACTOR = 0.24F;
    int NIGHT_SKY_COLOR_MULTIPLIER = -16777216;
    int NIGHT_FOG_COLOR_MULTIPLIER = ARGB.colorFromFloat(1.0F, 0.06F, 0.06F, 0.09F);
    int NIGHT_CLOUD_COLOR_MULTIPLIER = ARGB.colorFromFloat(1.0F, 0.1F, 0.1F, 0.15F);
    static void bootstrap(BootstrapContext<Timeline> context) {
        // datapack generation only: the native registry loader owns the vanilla timelines
    }
    private static ResourceKey<Timeline> named(String id) {
        return ResourceKey.create(Registries.TIMELINE, Identifier.withDefaultNamespace(id));
    }
}
