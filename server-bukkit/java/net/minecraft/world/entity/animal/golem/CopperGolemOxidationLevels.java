// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.animal.golem;
import net.minecraft.world.level.block.WeatheringCopper;
import java.util.Map;
import net.minecraft.sounds.SoundEvents;
import net.minecraft.resources.Identifier;
import net.minecraft.sounds.SoundEvent;
public class CopperGolemOxidationLevels {
    private static final Map<WeatheringCopper.WeatherState, CopperGolemOxidationLevel> kLevels = Map.of(
        WeatheringCopper.WeatherState.UNAFFECTED, level(SoundEvents.COPPER_GOLEM_SPIN, SoundEvents.COPPER_GOLEM_HURT, SoundEvents.COPPER_GOLEM_DEATH, SoundEvents.COPPER_GOLEM_STEP, ""),
        WeatheringCopper.WeatherState.EXPOSED, level(SoundEvents.COPPER_GOLEM_SPIN, SoundEvents.COPPER_GOLEM_HURT, SoundEvents.COPPER_GOLEM_DEATH, SoundEvents.COPPER_GOLEM_STEP, "_exposed"),
        WeatheringCopper.WeatherState.WEATHERED, level(SoundEvents.COPPER_GOLEM_WEATHERED_SPIN, SoundEvents.COPPER_GOLEM_WEATHERED_HURT, SoundEvents.COPPER_GOLEM_WEATHERED_DEATH, SoundEvents.COPPER_GOLEM_WEATHERED_STEP, "_weathered"),
        WeatheringCopper.WeatherState.OXIDIZED, level(SoundEvents.COPPER_GOLEM_OXIDIZED_SPIN, SoundEvents.COPPER_GOLEM_OXIDIZED_HURT, SoundEvents.COPPER_GOLEM_OXIDIZED_DEATH, SoundEvents.COPPER_GOLEM_OXIDIZED_STEP, "_oxidized"));
    private static CopperGolemOxidationLevel level(SoundEvent spin, SoundEvent hurt, SoundEvent death, SoundEvent step, String suffix) {
        String base = "textures/entity/copper_golem/";
        return new CopperGolemOxidationLevel(spin, hurt, death, step, Identifier.withDefaultNamespace(base + "copper_golem" + suffix + ".png"),
            Identifier.withDefaultNamespace(base + "copper_golem_eyes" + suffix + ".png"));
    }
    public static CopperGolemOxidationLevel getOxidationLevel(WeatheringCopper.WeatherState state) {
        return kLevels.get(state);
    }
}
